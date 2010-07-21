#ifndef MAPRENDERER_H
#define MAPRENDERER_H 1

#include <QImage>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QPainter>
#include <QPair>
#include <QPixmap>
#include <QQueue>
#include <QThread>
#include <QTimer>
#include <QWaitCondition>
#include "map.h"
#include "projection.h"
class MapRenderer;
class QPainterPath;
class TileEntry;

class MapTileIOThread : public QThread {
  Q_OBJECT;

public:
  MapTileIOThread(MapRenderer *s, QObject *parent = 0);

protected:
  void run();

private:
  MapRenderer *view;

signals:
  void tileLoaded(Tile tile, QString filename, QImage img);
};

class MapRendererClient {
 public:
  virtual int currentLayer() = 0;
  virtual QRect visibleArea() = 0;
};

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


  // Public for thread class
  qint64 bytesRead;
  QMutex tileQueueMutex;
  QWaitCondition tileQueueCond;
  QQueue<QPair<Tile, QString> > tileQueue;

signals:
  void tileUpdated(Tile key);

public slots:
  void tileLoaded(Tile key, QString filename, QImage img);

private slots:
  // Prune tiles not needed by any client
  void pruneTiles();

private:
  QTimer pruneTimer;

  QList<MapRendererClient *> clients;
  
  // UTM Zone boundaries in projection coordinates
  QPainterPath *zoneBoundaries[numDatums][UTM::numZones];

  QPainterPath *getUTMZoneBoundary(Datum d, int zone);

  QVector<MapTileIOThread *> ioThreads;

  Map *map;

  // All currently loaded tiles.
  // Tiles are present but have value NULL if they are queued to be loaded from disk.
  QMap<Tile, TileEntry *> tileMap;

  //  void findTile(Tile key, QPixmap &p, QRect &r);
  void drawTile(Tile key, QPainter &p, const QRect &r);

  QPointF mapToView(QPoint origin, qreal scale, QPointF p);

  void rulerInterval(qreal length, qreal &interval, int &ilog);

  // Render a grid on a projection
  void renderGrid(QPainter &p, QRect area, Projection *p, 
                  qreal interval);
};

#endif