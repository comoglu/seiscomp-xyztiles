/***************************************************************************
 * Copyright (C) comoglu@gmail.com                                         *
 * All rights reserved.                                                    *
 *                                                                         *
 * GNU Affero General Public License Usage                                 *
 * This file may be used under the terms of the GNU Affero                 *
 * Public License version 3.0 as published by the Free Software Foundation *
 * and appearing in the file LICENSE included in the packaging of this     *
 * file. Please review the following information to ensure the GNU Affero  *
 * Public License version 3.0 requirements will be met:                    *
 * https://www.gnu.org/licenses/agpl-3.0.html.                             *
 ***************************************************************************/


#define SEISCOMP_COMPONENT XYZTiles

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

#include <algorithm>

using namespace Seiscomp::Gui;
using namespace Seiscomp::Gui::Map;

REGISTER_TILESTORE_INTERFACE(XYZTileStore, "xyz");


// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
XYZTileStore::XYZTileStore()
: _userAgent("SeisComP-xyztiles/1.0") {}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
XYZTileStore::~XYZTileStore() {
	refresh();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool XYZTileStore::open(MapsDesc &desc) {
	QString defaultURL = desc.location.trimmed();

	auto *app = Seiscomp::Client::Application::Instance();

	// Single-source level band, also used as default when no per-source
	// band is given on a "map.xyz.sources" entry.
	int defaultMin = 0;
	int defaultMax = 19;
	if ( app ) {
		try { defaultMin = app->configGetInt("map.xyz.minLevel"); } catch (...) {}
		try { defaultMax = app->configGetInt("map.xyz.maxLevel"); } catch (...) {}
	}

	// "map.xyz.sources": one entry per zoom band, "minLevel:maxLevel:urlTemplate".
	// The URL keeps its "https://" colons intact because only the first two
	// colons are treated as field separators.
	if ( app ) {
		try {
			for ( const auto &raw : app->configGetStrings("map.xyz.sources") ) {
				QString entry = QString::fromStdString(raw).trimmed();
				if ( entry.isEmpty() )
					continue;

				int c1 = entry.indexOf(':');
				int c2 = c1 >= 0 ? entry.indexOf(':', c1 + 1) : -1;
				if ( c1 < 0 || c2 < 0 ) {
					SEISCOMP_WARNING("xyz: ignoring malformed map.xyz.sources entry "
					                 "(want minLevel:maxLevel:url): %s",
					                 qUtf8Printable(entry));
					continue;
				}

				bool okMin = false, okMax = false;
				Source src;
				src.minLevel = entry.left(c1).trimmed().toInt(&okMin);
				src.maxLevel = entry.mid(c1 + 1, c2 - c1 - 1).trimmed().toInt(&okMax);
				src.url      = entry.mid(c2 + 1).trimmed();
				if ( !okMin || !okMax || src.url.isEmpty() || src.minLevel > src.maxLevel ) {
					SEISCOMP_WARNING("xyz: ignoring invalid map.xyz.sources entry: %s",
					                 qUtf8Printable(entry));
					continue;
				}
				_sources.push_back(src);
			}
		} catch (...) {}
	}

	// No per-source bands configured: fall back to the single map.location.
	if ( _sources.isEmpty() ) {
		if ( defaultURL.isEmpty() ) {
			SEISCOMP_ERROR("xyz: no tile source — set map.location to an XYZ URL template "
			               "(e.g. https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png) "
			               "or provide map.xyz.sources entries");
			return false;
		}
		_sources.push_back(Source{defaultMin, defaultMax, defaultURL});
	}

	// Overall level bounds = union across sources.
	_minLevel = _sources.front().minLevel;
	_maxLevel = _sources.front().maxLevel;
	for ( const auto &s : _sources ) {
		_minLevel = std::min(_minLevel, s.minLevel);
		_maxLevel = std::max(_maxLevel, s.maxLevel);
	}
	if ( _maxLevel > static_cast<int>(TileIndex::MaxLevel) )
		_maxLevel = static_cast<int>(TileIndex::MaxLevel);

	if ( app ) {
		try { _cacheDuration = app->configGetInt("map.xyz.cacheDuration"); } catch (...) {}
		try { _missingTTL    = app->configGetInt("map.xyz.missingTTL");    } catch (...) {}
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
			// Stored as a string so scconfig can present a 256/512 dropdown.
			bool ok = false;
			int sz = QString::fromStdString(app->configGetString("map.xyz.tileSize"))
			         .trimmed().toInt(&ok);
			if ( ok && sz > 0 )
				_tilesize = QSize(sz, sz);
			else
				SEISCOMP_WARNING("xyz: ignoring invalid map.xyz.tileSize");
		} catch (...) {}
	}

	if ( _tilesize.isEmpty() )
		_tilesize = QSize(256, 256);

	bool needSubdomains = false;
	for ( const auto &s : _sources )
		needSubdomains = needSubdomains || s.url.contains("{s}");
	if ( _subdomains.isEmpty() && needSubdomains )
		_subdomains << "a" << "b" << "c";

	_projection = Mercator;

	_nam = new QNetworkAccessManager(this);
	connect(_nam, &QNetworkAccessManager::finished,
	        this, &XYZTileStore::onRequestFinished);

	if ( !_cacheDir.isEmpty() )
		QDir().mkpath(_cacheDir);

	SEISCOMP_INFO("xyz: tile store opened — %d source(s)  levels=%d..%d  cache=%s  ttl=%ds",
	              static_cast<int>(_sources.size()), _minLevel, _maxLevel,
	              _cacheDir.isEmpty() ? "disabled" : qUtf8Printable(_cacheDir),
	              _cacheDuration);
	for ( const auto &s : _sources )
		SEISCOMP_INFO("xyz:   level %2d..%-2d → %s",
		              s.minLevel, s.maxLevel, qUtf8Printable(s.url));
	return true;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
TileStore::LoadResult XYZTileStore::load(QImage &img, const TileIndex &tile) {
	if ( _inflight.contains(tile.id) )
		return Deferred;

	// Negative cache: don't re-hammer the server for tiles it already told us
	// it doesn't have (HTTP >= 400). Respect map.xyz.missingTTL.
	auto miss = _missing.constFind(tile.id);
	if ( miss != _missing.constEnd() ) {
		if ( _missingTTL < 0 ||
		     miss.value() + _missingTTL > QDateTime::currentSecsSinceEpoch() )
			return Error;
		_missing.erase(_missing.find(tile.id));
	}

	if ( !_cacheDir.isEmpty() ) {
		QString path = cachePath(tile);
		if ( QFile::exists(path) && isCacheFresh(path) ) {
			if ( img.load(path) )
				return OK;
			QFile::remove(path);
		}
	}

	startRequest(tile);
	return Deferred;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void XYZTileStore::startRequest(const TileIndex &tile) {
	_inflight.insert(tile.id);

	QNetworkRequest req{QUrl{buildURL(tile)}};
	req.setHeader(QNetworkRequest::UserAgentHeader, _userAgent);
	req.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
	req.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
	req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
	                 QNetworkRequest::NoLessSafeRedirectPolicy);

	QNetworkReply *reply = _nam->get(req);
	_replyMap.insert(reply, tile.id);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void XYZTileStore::onRequestFinished(QNetworkReply *reply) {
	reply->deleteLater();

	auto it = _replyMap.find(reply);
	if ( it == _replyMap.end() )
		return;

	TileId tileId = it.value();
	_replyMap.erase(it);
	_inflight.remove(tileId);

	TileIndex tile;
	tile.id = tileId;

	if ( reply->error() != QNetworkReply::NoError ) {
		if ( reply->error() != QNetworkReply::OperationCanceledError ) {
			SEISCOMP_WARNING("xyz: fetch error for %s: %s",
			                 qUtf8Printable(getID(tile)),
			                 qUtf8Printable(reply->errorString()));
		}
		loadingCancelled(tile);
		return;
	}

	int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	if ( httpStatus >= 400 ) {
		SEISCOMP_WARNING("xyz: HTTP %d for tile %s",
		                 httpStatus, qUtf8Printable(getID(tile)));
		if ( _missingTTL != 0 )
			_missing.insert(tileId, QDateTime::currentSecsSinceEpoch());
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

	// One-time sanity check: the configured map.xyz.tileSize must match the
	// pixels the server actually serves, otherwise SeisComP's projection scale
	// (which uses tileSize) picks the wrong zoom level and the map looks blurry.
	if ( !_tileSizeChecked ) {
		_tileSizeChecked = true;
		if ( img.size() != _tilesize )
			SEISCOMP_WARNING("xyz: server tile is %dx%d but map.xyz.tileSize is %dx%d — "
			                 "set map.xyz.tileSize to %d to avoid blurry rendering",
			                 img.width(), img.height(),
			                 _tilesize.width(), _tilesize.height(), img.width());
	}

	// Only persist when caching is actually enabled for reads
	// (cacheDuration == 0 disables the cache entirely).
	if ( !_cacheDir.isEmpty() && _cacheDuration != 0 ) {
		QString path = cachePath(tile);
		QDir().mkpath(QFileInfo(path).absolutePath());
		QFile f(path);
		if ( f.open(QIODevice::WriteOnly) )
			f.write(data);
	}

	loadingComplete(img, tile);
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
const XYZTileStore::Source *XYZTileStore::sourceForLevel(int level) const {
	// Prefer a source whose band contains the level.
	for ( const auto &s : _sources ) {
		if ( level >= s.minLevel && level <= s.maxLevel )
			return &s;
	}
	// Gap between bands: fall back to the source with the nearest edge so a
	// tile still renders (scaled) rather than showing a hole.
	const Source *best = nullptr;
	int bestDist = 0;
	for ( const auto &s : _sources ) {
		int d = std::min(std::abs(level - s.minLevel), std::abs(level - s.maxLevel));
		if ( !best || d < bestDist ) {
			best = &s;
			bestDist = d;
		}
	}
	return best;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
QString XYZTileStore::buildURL(const TileIndex &tile) {
	const Source *src = sourceForLevel(tile.level());
	if ( !src )
		return QString();

	QString url = src->url;
	url.replace("{z}", QString::number(tile.level()));
	url.replace("{x}", QString::number(tile.column()));
	url.replace("{y}", QString::number(tile.row()));

	if ( !_subdomains.isEmpty() ) {
		url.replace("{s}", _subdomains[_subdomainIndex % _subdomains.size()]);
		++_subdomainIndex;
	}

	return url;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
QString XYZTileStore::cachePath(const TileIndex &tile) const {
	return QString("%1/%2/%3/%4")
	    .arg(_cacheDir)
	    .arg(tile.level())
	    .arg(tile.column())
	    .arg(tile.row());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool XYZTileStore::isCacheFresh(const QString &path) const {
	if ( _cacheDuration < 0 ) return true;
	if ( _cacheDuration == 0 ) return false;
	return QFileInfo(path).lastModified().secsTo(QDateTime::currentDateTime())
	       < _cacheDuration;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void XYZTileStore::refresh() {
	for ( auto *reply : _replyMap.keys() )
		reply->abort();
	_replyMap.clear();
	_inflight.clear();
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int XYZTileStore::maxLevel() const { return _maxLevel; }
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool XYZTileStore::hasPendingRequests() const { return !_inflight.isEmpty(); }
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
QString XYZTileStore::getID(const TileIndex &tile) const {
	return QString("%1/%2/%3")
	    .arg(tile.level()).arg(tile.column()).arg(tile.row());
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool XYZTileStore::validate(int level, int column, int row) const {
	if ( level < _minLevel || level > _maxLevel ) return false;
	const int n = 1 << level;
	return column >= 0 && column < n && row >= 0 && row < n;
}
// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
