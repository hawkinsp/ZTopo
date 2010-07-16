#ifndef MAPSCENE_H
#define MAPSCENE_H 1

#include <QSet>
#include <QMap>
#include <QGraphicsScene>
#include <QMutex>
#include <QPen>
#include <QQueue>
#include <QWaitCondition>
#include "map.h"

class TileIOThread;


typedef QPair<QGraphicsLineItem *, QGraphicsLineItem *> GridLinePair;
typedef QPair<int, int> IntPair;

class MapScene : public QGraphicsScene {
  Q_OBJECT
public:
  MapScene(Map *m, QObject *parent = 0);

  // Notify the scene that the bounds of the view have changed
  void updateBounds(QRect bounds, int maxLevel);
  void setScalingMode(bool smooth);

  int maxLevel() { return map->maxLevel(); }

  QMutex tileQueueMutex;
  QWaitCondition tileQueueCond;
  QQueue<QPair<TileKey, QString> > tileQueue;

public slots:
  void tileLoaded(int x, int y, int level, QString filename, QImage img);

private:
  Map *map;

  // All currently loaded tiles.
  // Tile keys are present but have value NULL if they are queued to be loaded from disk.
  QMap<TileKey, QGraphicsPixmapItem *> tileMap;

  // Visible tiles that do not belong to the current zoom level; as soon as we load the appropriate tiles
  // from the current zoom level we will delete these.
  QSet<TileKey> staleTiles;

  // Currently visible area of the map
  QRect visibleArea;
  int zoomLevel;

  TileIOThread *ioThread;

  bool smoothScaling;

  QPen gridPen;
  QRect gridBounds;
  QMap<QPair<int, int>, GridLinePair> gridSegmentMap;

  void makeTile(TileKey key, QPixmap p);
  void drawGrid(QRect bounds);
  void pruneStaleTiles();
  bool shouldPrune(TileKey key);
};

#endif
