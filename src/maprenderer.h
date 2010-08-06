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
class QNetworkAccessManager;

enum Direction {
  Left = 0,
  Top = 1,
  Right = 2,
  Bottom = 3
};

struct GridTick
{
  GridTick(Direction, qreal, qreal);

  Direction side;
  qreal mapPos;
  qreal gridPos;
};

class MapRendererClient {
 public:
  virtual int currentLayer() const = 0;
  virtual QRect visibleArea() const = 0;
};

// Abstract base class for clients of a MapRenderer
class MapRenderer : public QObject {
  Q_OBJECT
public:
  MapRenderer(Map *m, Cache::Cache &tileCache, QObject *parent = 0);

  void addClient(MapRendererClient *);
  void removeClient(MapRendererClient *);

  // Bump a scale factor to the nearest scale which gives integer tile sizes
  void bumpScale(int layer, qreal scale, qreal &bumpedScale, int &bumpedTileSize);


  // Load the tiles needed to display a given map area at a given scale.
  // Loading is asynchronous; this function usually returns before the loading takes 
  // place.
  // Returns true if all the tiles are present in memory.
  bool loadTiles(int layer, QRect area, qreal scale);
  
  // Render an area of a map layer onto a paint device at a given scale
  // Does not clip precisely to the map boundary; the area drawn may lie over
  // the edge of the map area.
  void render(QPainter &p, int layer, QRect area, qreal scale);

  void renderRuler(QPainter &p, int width, qreal scale);

  void renderGeographicGrid(QPainter &p, QRect area, qreal scale, Datum d,
                            qreal interval, QList<GridTick> *);
  void renderUTMGrid(QPainter &p, QRect area, qreal scale, Datum d,
                     qreal interval, QList<GridTick> *);

  Cache::Cache &getCache() { return tileCache; }

private slots:
  // Prune tiles not needed by any client
  void pruneTiles();

private:
  QTimer pruneTimer;

  // We keep a list of clients of the renderer, which we interrogate when pruning to
  // see which tiles they still want us to keep around.
  QList<MapRendererClient *> clients;
  
  // UTM Zone boundaries in projection coordinates
  QPainterPath *zoneBoundaries[numDatums][UTM::numZones];

  QPainterPath *getUTMZoneBoundary(Datum d, int zone);

  Map *map;

  Cache::Cache &tileCache;

  //  void findTile(Tile key, QPixmap &p, QRect &r);
  void drawTile(Tile key, QPainter &p, const QRect &r);

  QPointF mapToView(QPoint origin, qreal scale, QPointF p);

  void rulerInterval(qreal length, qreal &interval, int &ilog);

  // Render a grid on a projection
  void renderGrid(QPainter &p, QPainterPath *clipPath, QRect area, Projection *pj, 
                  qreal interval, QList<GridTick> *ticks);
};

#endif
