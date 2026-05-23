#pragma once

#include <seiscomp/gui/map/imagetree.h>

#include <QHash>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QSize>
#include <QString>
#include <QStringList>

class QFileSystemWatcher;
class QNetworkAccessManager;
class QNetworkReply;

namespace Seiscomp { namespace Client { class Application; } }


struct XYZSource {
	QString     name;
	QString     urlTemplate;
	QStringList subdomains;
};


class XYZTileStore : public QObject, public Seiscomp::Gui::Map::TileStore {
	Q_OBJECT

public:
	XYZTileStore();
	~XYZTileStore() override;

	bool       open(Seiscomp::Gui::MapsDesc &desc) override;
	int        maxLevel() const override;
	LoadResult load(QImage &img, const Seiscomp::Gui::Map::TileIndex &tile) override;
	QString    getID(const Seiscomp::Gui::Map::TileIndex &tile) const override;
	bool       validate(int level, int column, int row) const override;
	bool       hasPendingRequests() const override;
	void       refresh() override;

private slots:
	void onRequestFinished(QNetworkReply *reply);
	void onSourceFileChanged(const QString &path);

private:
	using TileId = Seiscomp::Gui::Map::TileIndex::Storage;

	void    loadSources(Seiscomp::Client::Application *app);
	bool    switchSource(const QString &name);
	QString buildURL(const Seiscomp::Gui::Map::TileIndex &tile);
	QString cachePath(const Seiscomp::Gui::Map::TileIndex &tile) const;
	bool    isCacheFresh(const QString &path) const;
	void    startRequest(const Seiscomp::Gui::Map::TileIndex &tile);
	void    abortInflight();

	QNetworkAccessManager           *_nam{nullptr};
	QFileSystemWatcher              *_watcher{nullptr};

	// Active source
	QString                          _urlTemplate;
	QStringList                      _subdomains;
	int                              _subdomainIndex{0};
	QString                          _currentSourceName;

	// All configured sources (name → XYZSource)
	QMap<QString, XYZSource>         _sources;

	int                              _maxLevel{18};
	QString                          _cacheDir;
	int                              _cacheDuration{86400};
	QString                          _userAgent;
	QString                          _switchFile;

	QHash<QNetworkReply *, TileId>   _replyMap;
	QSet<TileId>                     _inflight;
};
