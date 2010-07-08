#include <QGraphicsPixmapItem>
#include <QPixmapCache>
#include <cmath>
#include "mapscene.h"
#include "map.h"
#include "mapprojection.h"
#include "tileiothread.h"
#include <iostream>

uint qHash(const TileKey& k)
{
  return qHash(k.x) ^ qHash(k.y) ^ qHash(k.level);
}


MapScene::MapScene(Map *m, QObject *parent)
  : QGraphicsScene(parent), map(m), emptyPixmap(m->emptyTile())
{
  setSceneRect(0, 0, m->requestedSize().x(), m->requestedSize().y());
  ioThread = new TileIOThread(this, this);
  connect(ioThread, SIGNAL(tileLoaded(int, int, int, QString, QImage)),
          this, SLOT(tileLoaded(int, int, int, QString, QImage)));
  ioThread->start();

  gridBounds = QRect(0, 0, 0, 0);
  gridPen.setColor(QColor(qRgb(0, 0, 0xFF)));
}

void MapScene::tileLoaded(int x, int y, int level, QString filename, QImage img)
{
  TileKey key(x, y, level);
  if (tileMap.contains(key)) {
    QGraphicsPixmapItem *item = tileMap[key];
    QPixmap p = QPixmap::fromImage(img);
    QPixmapCache::insert(filename, p);
    item->setPixmap(p);
    item->setVisible(true);
  }
}

void MapScene::updateTiles(QSet<TileKey> newTiles)
{
  foreach (TileKey key, newTiles) {
    if (!tileMap.contains(key)) {
      QString filename = map->tilePath(QPoint(key.x, key.y), key.level);
      //      std::cout << "adding tile " << key.x << " " << key.y << " " << key.level <<
      //        " " << filename.toStdString() << std::endl;
      QPixmap p = emptyPixmap;
      bool visible = true;
      if (!QPixmapCache::find(filename, &p)) {
        tileQueueMutex.lock();
        tileQueue.enqueue(QPair<TileKey, QString>(key, filename));
        tileQueueCond.wakeOne();
        tileQueueMutex.unlock();
        visible = false;
      }
      QGraphicsPixmapItem *item = addPixmap(p);
      int logTileSize = map->logTileSize(key.level);
      int scale = 1 << (map->maxLevel() - key.level);
      item->setTransformationMode(Qt::SmoothTransformation);
      item->setZValue(key.level);
      item->setPos(key.x << logTileSize, key.y << logTileSize);
      item->scale(scale, scale);
      item->setVisible(visible);
      tileMap[key] = item;
    }
  }

  QMutableMapIterator<TileKey, QGraphicsPixmapItem *> i(tileMap);
  while (i.hasNext()) {
    i.next();

    if (!newTiles.contains(i.key())) {
      delete i.value();
      i.remove();
    }
  }
}


void MapScene::updateBounds(QRect bounds, int maxLevel)
{
  QSet<TileKey> newTiles;
  for (int level = maxLevel; level >= 0; level--) {
    int tileSize = map->tileSize(level);
    int minTileX = std::max(0, bounds.left() / tileSize - 1);
    int maxTileX = std::min(1<<level, bounds.right() / tileSize + 1);
    int minTileY = std::max(0, bounds.top() / tileSize - 1);
    int maxTileY = std::min(1<<level, bounds.bottom() / tileSize + 1);
  
    for (int tileY = minTileY; tileY <= maxTileY; tileY++) {
      for (int tileX = minTileX; tileX <= maxTileX; tileX++) {
        newTiles.insert(TileKey(tileX, tileY, level));
      }
    }
  }
  updateTiles(newTiles);

  drawGrid(bounds);
}

void MapScene::drawGrid(QRect bounds)
{
  MapProjection *proj = map->projection();
  float interval = 100.0;
  QRectF projBounds(proj->mapToProj(bounds.topLeft()),
                    proj->mapToProj(bounds.bottomRight()));

  int gridMinX = (int)std::floor(gridBounds.left() / interval) - 1;
  int gridMinY = (int)std::floor(gridBounds.bottom() / interval) - 1;
  int gridMaxX = (int)std::ceil(gridBounds.right() / interval) + 1;
  int gridMaxY = (int)std::ceil(gridBounds.top() / interval) + 1;
  QRect newGridBounds(gridMinX, gridMinY, gridMaxX, gridMaxY);
  
  for (int x = gridMinX; x <= gridMaxX; x++) {
    for (int y = gridMinY; y <= gridMaxY; y++) {
      QPointF p = proj->projToMap(QPointF(x * interval, y * interval));
      QPointF pr = proj->projToMap(QPointF((x + 1) * interval, y * interval));
      QPointF pu = proj->projToMap(QPointF(x * interval, (y + 1) * interval));

      QGraphicsLineItem *rItem = addLine(QLineF(p, pr), gridPen);
      rItem->setZValue(maxLevel() + 1);
      QGraphicsLineItem *uItem = addLine(QLineF(p, pu), gridPen);
      uItem->setZValue(maxLevel() + 1);

      gridSegmentMap[IntPair(x, y)] = GridLinePair(rItem, uItem);
    }
  }
  gridBounds = newGridBounds;
}
