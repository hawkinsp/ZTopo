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
  MapRenderer(Map *m, QObject *parent = 0);

  void addClient(MapRendererClient *);
  void removeClient(MapRendererClient *);

  // Bump a scale factor to the nearest scale which gives integer tile sizes
  void bumpScale(int layer, qreal scale, qreal &bumpedScale, int &bumpedTileSize);


  // Load the tiles needed to display a given map area at a given scale.
  // Loading is asynchronous; this function may return before the loading takes place.
  void loadTiles(int layer, QRect area, qreal scale, bool wait = false);
  
  // Render an area of a map layer onto a paint device at a given scale
  void render(QPainter &p, int layer, QRect area, qreal scale);

  void renderRuler(QPainter &p, int width, qreal scale);

  void renderGeographicGrid(QPainter &p, QRect area, qreal scale, Datum d,
                            qreal interval);
  void renderUTMGrid(QPainter &p, QRect area, qreal scale, Datum d,
                     qreal interval);


signals:
  void tileUpdated(Tile key);

private slots:
  // Prune tiles not needed by any client
  void pruneTiles();
  void tileLoaded(Tile key);

private:
  QTimer pruneTimer;

  // We keep a list of clients of the renderer, which we interrogate when pruning to
  // see which tiles they still want us to keep around.
  QList<MapRendererClient *> clients;
  
  // UTM Zone boundaries in projection coordinates
  QPainterPath *zoneBoundaries[numDatums][UTM::numZones];

  QPainterPath *getUTMZoneBoundary(Datum d, int zone);

  Map *map;

  TileCache tileCache;

  //  void findTile(Tile key, QPixmap &p, QRect &r);
  void drawTile(Tile key, QPainter &p, const QRect &r);

  QPointF mapToView(QPoint origin, qreal scale, QPointF p);

  void rulerInterval(qreal length, qreal &interval, int &ilog);

  // Render a grid on a projection
  void renderGrid(QPainter &p, QRect area, Projection *p, 
                  qreal interval);
};

#endif
