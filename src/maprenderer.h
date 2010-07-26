/*
  ZTopo --- a viewer for topographic maps
  Copyright (C) 2010 Peter Hawkins
  
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef MAPRENDERER_H
#define MAPRENDERER_H 1


#include <QImage>
#include <QList>
#include <QMap>
#include <QPainter>
#include <QPair>
#include <QTimer>
#include "map.h"
#include "projection.h"
#include "tilecache.h"
class MapRenderer;
class QPainterPath;


class MapRendererClient {
 public:
  virtual int currentLayer() const = 0;
  virtual QRect visibleArea() const = 0;
};

// Abstract base class for clients of a MapRenderer
class MapRenderer : public QObject {
  Q_OBJECT
public:
  MapRenderer(Map *m, const QString &cachePath, QObject *parent = 0);

  void addClient(MapRendererClient *);
  void removeClient(MapRendererClient *);

  // Bump a scale factor to the nearest scale which gives integer tile sizes
  void bumpScale(int layer, qreal scale, qreal &bumpedScale, int &bumpedTileSize);


  // Load the tiles needed to display a given map area at a given scale.
  // Loading is asynchronous; this function usually returns before the loading takes place.
  // Returns true if all the tiles are present in memory.
  bool loadTiles(int layer, QRect area, qreal scale);
  
  // Render an area of a map layer onto a paint device at a given scale
  void render(QPainter &p, int layer, QRect area, qreal scale);

  void renderRuler(QPainter &p, int width, qreal scale);

  void renderGeographicGrid(QPainter &p, QRect area, qreal scale, Datum d,
                            qreal interval);
  void renderUTMGrid(QPainter &p, QRect area, qreal scale, Datum d,
                     qreal interval);


signals:
  void tileUpdated();

private slots:
  // Prune tiles not needed by any client
  void pruneTiles();
  void tileLoaded();

private:
  QTimer pruneTimer;

  // We keep a list of clients of the renderer, which we interrogate when pruning to
  // see which tiles they still want us to keep around.
  QList<MapRendererClient *> clients;
  
  // UTM Zone boundaries in projection coordinates
  QPainterPath *zoneBoundaries[numDatums][UTM::numZones];

  QPainterPath *getUTMZoneBoundary(Datum d, int zone);

  Map *map;

  Cache::Cache tileCache;

  //  void findTile(Tile key, QPixmap &p, QRect &r);
  void drawTile(Tile key, QPainter &p, const QRect &r);

  QPointF mapToView(QPoint origin, qreal scale, QPointF p);

  void rulerInterval(qreal length, qreal &interval, int &ilog);

  // Render a grid on a projection
  void renderGrid(QPainter &p, QRect area, Projection *p, 
                  qreal interval);
};

#endif
