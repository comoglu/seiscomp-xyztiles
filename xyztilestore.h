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


#ifndef SEISCOMP_GUI_MAP_XYZTILESTORE_H__
#define SEISCOMP_GUI_MAP_XYZTILESTORE_H__


#include <seiscomp/gui/map/imagetree.h>

#include <QHash>
#include <QObject>
#include <QSet>
#include <QSize>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;


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

private:
	using TileId = Seiscomp::Gui::Map::TileIndex::Storage;

	QString buildURL(const Seiscomp::Gui::Map::TileIndex &tile);
	QString cachePath(const Seiscomp::Gui::Map::TileIndex &tile) const;
	bool    isCacheFresh(const QString &path) const;
	void    startRequest(const Seiscomp::Gui::Map::TileIndex &tile);

	QNetworkAccessManager          *_nam{nullptr};
	QString                         _urlTemplate;
	QStringList                     _subdomains;
	int                             _subdomainIndex{0};
	int                             _maxLevel{18};
	QString                         _cacheDir;
	int                             _cacheDuration{86400};
	QString                         _userAgent;

	QHash<QNetworkReply *, TileId>  _replyMap;
	QSet<TileId>                    _inflight;
};


#endif
