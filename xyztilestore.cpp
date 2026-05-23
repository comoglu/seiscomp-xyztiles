#define SEISCOMP_COMPONENT MapTilesCommunity

#include "xyztilestore.h"

#include <seiscomp/client/application.h>
#include <seiscomp/logging/log.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

using namespace Seiscomp::Gui;
using namespace Seiscomp::Gui::Map;

REGISTER_TILESTORE_INTERFACE(XYZTileStore, "xyz");


XYZTileStore::XYZTileStore()
: _userAgent("SeisComP-maptiles-community/1.0") {}

XYZTileStore::~XYZTileStore() {
	refresh(); // abort all in-flight requests cleanly
}


bool XYZTileStore::open(MapsDesc &desc) {
	_urlTemplate = desc.location.trimmed();
	if ( _urlTemplate.isEmpty() ) {
		SEISCOMP_ERROR("xyz: map.location is empty — set it to an XYZ tile URL template, "
		               "e.g. https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png");
		return false;
	}

	// Read plugin-specific config from the SeisComP application config
	auto *app = Seiscomp::Client::Application::Instance();
	if ( app ) {
		try { _maxLevel      = app->configGetInt("map.xyz.maxLevel");    } catch (...) {}
		try { _cacheDuration = app->configGetInt("map.xyz.cacheDuration"); } catch (...) {}
		try {
			_cacheDir = QString::fromStdString(app->configGetString("map.xyz.cacheDir"));
		} catch (...) {}
		try {
			_userAgent = QString::fromStdString(app->configGetString("map.xyz.userAgent"));
		} catch (...) {}
		try {
			QString s = QString::fromStdString(app->configGetString("map.xyz.subdomains"));
			for ( auto &sub : s.split(',', Qt::SkipEmptyParts) )
				_subdomains << sub.trimmed();
		} catch (...) {}
		try {
			int sz = app->configGetInt("map.xyz.tileSize");
			_tilesize = QSize(sz, sz);
		} catch (...) {}
	}

	if ( _tilesize.isEmpty() )
		_tilesize = QSize(256, 256);

	// Default OSM-style subdomains when the URL uses {s} but none were configured
	if ( _subdomains.isEmpty() && _urlTemplate.contains("{s}") )
		_subdomains << "a" << "b" << "c";

	// All XYZ/slippy-map tiles are Web Mercator
	_projection = Mercator;

	_nam = new QNetworkAccessManager(this);
	connect(_nam, &QNetworkAccessManager::finished,
	        this, &XYZTileStore::onRequestFinished);

	if ( !_cacheDir.isEmpty() )
		QDir().mkpath(_cacheDir);

	SEISCOMP_INFO("xyz: tile store opened — url=%s  maxLevel=%d  cache=%s  ttl=%ds",
	              qUtf8Printable(_urlTemplate), _maxLevel,
	              _cacheDir.isEmpty() ? "disabled" : qUtf8Printable(_cacheDir),
	              _cacheDuration);
	return true;
}


TileStore::LoadResult XYZTileStore::load(QImage &img, const TileIndex &tile) {
	// Deduplicate: already fetching this tile?
	if ( _inflight.contains(tile.id) )
		return Deferred;

	// Serve from disk cache if fresh
	if ( !_cacheDir.isEmpty() ) {
		QString path = cachePath(tile);
		if ( QFile::exists(path) && isCacheFresh(path) ) {
			if ( img.load(path) )
				return OK;
			// Corrupt cache file — fall through and re-fetch
			QFile::remove(path);
		}
	}

	startRequest(tile);
	return Deferred;
}


void XYZTileStore::startRequest(const TileIndex &tile) {
	_inflight.insert(tile.id);

	QNetworkRequest req{QUrl{buildURL(tile)}};
	req.setHeader(QNetworkRequest::UserAgentHeader, _userAgent);
	req.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
	req.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
	// Follow redirects (needed for some CDN-backed tile servers)
	req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
	                 QNetworkRequest::NoLessSafeRedirectPolicy);

	QNetworkReply *reply = _nam->get(req);
	_replyMap.insert(reply, tile.id);
}


void XYZTileStore::onRequestFinished(QNetworkReply *reply) {
	reply->deleteLater();

	// Stale reply from before a refresh() — discard silently
	auto it = _replyMap.find(reply);
	if ( it == _replyMap.end() )
		return;

	TileId tileId = it.value();
	_replyMap.erase(it);
	_inflight.remove(tileId);

	TileIndex tile;
	tile.id = tileId;

	// Network-level error (connection refused, DNS failure, timeout, abort, …)
	if ( reply->error() != QNetworkReply::NoError ) {
		// OperationCanceledError is raised by abort() — not worth logging
		if ( reply->error() != QNetworkReply::OperationCanceledError ) {
			SEISCOMP_WARNING("xyz: fetch error for %s: %s",
			                 qUtf8Printable(getID(tile)),
			                 qUtf8Printable(reply->errorString()));
		}
		loadingCancelled(tile);
		return;
	}

	// HTTP-level error (404, 429, 500, …)
	int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	if ( httpStatus >= 400 ) {
		SEISCOMP_WARNING("xyz: HTTP %d for tile %s",
		                 httpStatus, qUtf8Printable(getID(tile)));
		loadingCancelled(tile);
		return;
	}

	QByteArray data = reply->readAll();
	if ( data.isEmpty() ) {
		loadingCancelled(tile);
		return;
	}

	QImage img;
	if ( !img.loadFromData(data) ) {
		SEISCOMP_WARNING("xyz: image decode failed for tile %s", qUtf8Printable(getID(tile)));
		loadingCancelled(tile);
		return;
	}

	// Persist raw bytes to disk cache (no re-encoding — preserves format and quality)
	if ( !_cacheDir.isEmpty() ) {
		QString path = cachePath(tile);
		QDir().mkpath(QFileInfo(path).absolutePath());
		QFile f(path);
		if ( f.open(QIODevice::WriteOnly) )
			f.write(data);
	}

	loadingComplete(img, tile);
}


QString XYZTileStore::buildURL(const TileIndex &tile) {
	QString url = _urlTemplate;
	url.replace("{z}", QString::number(tile.level()));
	url.replace("{x}", QString::number(tile.column()));
	url.replace("{y}", QString::number(tile.row()));

	if ( !_subdomains.isEmpty() ) {
		url.replace("{s}", _subdomains[_subdomainIndex % _subdomains.size()]);
		++_subdomainIndex;
	}

	return url;
}


QString XYZTileStore::cachePath(const TileIndex &tile) const {
	// Mirror the standard z/x/y directory layout — human-readable and inspectable
	return QString("%1/%2/%3/%4")
	    .arg(_cacheDir)
	    .arg(tile.level())
	    .arg(tile.column())
	    .arg(tile.row());
}


bool XYZTileStore::isCacheFresh(const QString &path) const {
	if ( _cacheDuration < 0 ) return true;  // infinite TTL
	if ( _cacheDuration == 0 ) return false; // caching disabled
	return QFileInfo(path).lastModified().secsTo(QDateTime::currentDateTime())
	       < _cacheDuration;
}


void XYZTileStore::refresh() {
	// Abort all in-flight requests; onRequestFinished will ignore them
	// because we clear _replyMap first
	for ( auto *reply : _replyMap.keys() )
		reply->abort();
	_replyMap.clear();
	_inflight.clear();
}


int     XYZTileStore::maxLevel()           const { return _maxLevel; }
bool    XYZTileStore::hasPendingRequests() const { return !_inflight.isEmpty(); }

QString XYZTileStore::getID(const TileIndex &tile) const {
	return QString("%1/%2/%3")
	    .arg(tile.level()).arg(tile.column()).arg(tile.row());
}

bool XYZTileStore::validate(int level, int column, int row) const {
	if ( level < 0 || level > _maxLevel ) return false;
	const int n = 1 << level;
	return column >= 0 && column < n && row >= 0 && row < n;
}
