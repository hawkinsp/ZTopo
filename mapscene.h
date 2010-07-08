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

class TileKey {
public:
  TileKey(int vx, int vy, int vlevel) : x(vx), y(vy), level(vlevel) { } 
  int x;
  int y;
  int level;

  bool operator== (const TileKey& other) const {
    return x == other.x && y == other.y && level == other.level; 
  }
  bool operator<  (const TileKey& other) const {
    return (x < other.x) ||
      (x == other.x && y < other.y) ||
      (x == other.x && y == other.y && level < other.level);    
  }
};

uint qHash(const TileKey& k);

typedef QPair<QGraphicsLineItem *, QGraphicsLineItem *> GridLinePair;
typedef QPair<int, int> IntPair;

class MapScene : public QGraphicsScene {
  Q_OBJECT
public:
  MapScene(Map *m, QObject *parent = 0);

  // Notify the scene that the bounds of the view have changed
  void updateBounds(QRect bounds, int maxLevel);

  int maxLevel() { return map->maxLevel(); }

  QMutex tileQueueMutex;
  QWaitCondition tileQueueCond;
  QQueue<QPair<TileKey, QString> > tileQueue;

public slots:
  void tileLoaded(int x, int y, int level, QString filename, QImage img);

private:
  Map *map;
  QMap<TileKey, QGraphicsPixmapItem *> tileMap;
  QPixmap emptyPixmap;
  TileIOThread *ioThread;

  QPen gridPen;
  QRect gridBounds;
  QMap<QPair<int, int>, GridLinePair> gridSegmentMap;

  void drawGrid(QRect bounds);
  void updateTiles(QSet<TileKey> newTiles);
};

#endif
