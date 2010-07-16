#include <QGraphicsPixmapItem>
#include <QPixmapCache>
#include <cmath>
#include "mapscene.h"
#include "map.h"
#include "mapprojection.h"
#include "tileiothread.h"
#include <iostream>


MapScene::MapScene(Map *m, QObject *parent)
  : QGraphicsScene(parent), map(m),
    visibleArea(0, 0, 0, 0), zoomLevel(0), smoothScaling(true)
{
  setSceneRect(0, 0, m->requestedSize().x(), m->requestedSize().y());
  ioThread = new TileIOThread(this, this);
  connect(ioThread, SIGNAL(tileLoaded(int, int, int, QString, QImage)),
          this, SLOT(tileLoaded(int, int, int, QString, QImage)));
  ioThread->start();

  gridBounds = QRect(0, 0, 0, 0);
  gridPen.setColor(QColor(qRgb(0, 0, 0xFF)));
}

void MapScene::setScalingMode(bool smooth)
{
  Qt::TransformationMode mode = 
    smooth ? Qt::SmoothTransformation : Qt::FastTransformation;
  smoothScaling = smooth;
  QMutableMapIterator<TileKey, QGraphicsPixmapItem *> i(tileMap);
  while (i.hasNext()) {
    i.next();
    QGraphicsPixmapItem *item = i.value();
    if (item) {
      item->setTransformationMode(mode);
    }
  }  
}

void MapScene::makeTile(TileKey key, QPixmap p)
{
  QGraphicsPixmapItem *item = addPixmap(p);

  item->setShapeMode(QGraphicsPixmapItem::BoundingRectShape);

  Qt::TransformationMode mode = smoothScaling ? Qt::SmoothTransformation : Qt::FastTransformation;
  item->setTransformationMode(mode);

  item->setZValue(key.level);

  int logTileSize = map->logTileSize(key.level);
  item->setPos(key.x << logTileSize, key.y << logTileSize);

  int scale = 1 << (map->maxLevel() - key.level);
  item->scale(scale, scale);

  tileMap[key] = item;
}

void MapScene::tileLoaded(int x, int y, int level, QString filename, QImage img)
{
  TileKey key(x, y, level);
  QPixmap p = QPixmap::fromImage(img);
  std::cout << "has alpha " << (p.hasAlpha() ? "true" : "false") << std::endl;
  QPixmapCache::insert(filename, p);

  // Check to see that the tile should still be displayed; it may have already been pruned
  if (tileMap.contains(key)) {
    makeTile(key, p);
    pruneStaleTiles();
  }
}

bool MapScene::shouldPrune(TileKey key) 
{
  if (abs(key.level - zoomLevel) > 1) return true;
  if (!visibleArea.intersects(map->tileToMapRect(key.x, key.y, key.level))) return true;
  return false;
}

void MapScene::pruneStaleTiles()
{
  QMutableSetIterator<TileKey> i(staleTiles);
  while (i.hasNext()) {
    TileKey key = i.next();

    if (shouldPrune(key)) {
      i.remove();
      QGraphicsPixmapItem *item = tileMap[key];
      if (item) delete item;
      tileMap.remove(key);
    }
  }
}


void MapScene::updateBounds(QRect newVisibleArea, int newLevel)
{
  int oldLevel = zoomLevel;
  QRect visibleTiles = map->mapRectToTileRect(visibleArea, zoomLevel);
  QRect newVisibleTiles = map->mapRectToTileRect(newVisibleArea, newLevel);

  // Old tiles still visible
  /*  std::cout << "bounds: " << bounds.x() << " " << bounds.y() << " " <<
    bounds.width() << " " << bounds.height() << std::endl;
  std::cout << "old: " << oldLevel << " " << visibleTiles.x() << " " << visibleTiles.y() << " " <<
    visibleTiles.width() << " " << visibleTiles.height() << std::endl;
  std::cout << "new: " << newLevel << " " << newVisibleTiles.x() << " " << newVisibleTiles.y() << " " <<
    newVisibleTiles.width() << " " << newVisibleTiles.height() << std::endl;
  */

  // Remove or mark stale invisible old tiles
  for (int x = visibleTiles.left(); x <= visibleTiles.right(); x++) {
    for (int y = visibleTiles.top(); y <= visibleTiles.bottom(); y++) {
      TileKey key(x, y, oldLevel);

      if (oldLevel != newLevel || !newVisibleTiles.contains(x, y)) {
        staleTiles.insert(key);
      }
    }
  }

  zoomLevel = newLevel;
  visibleArea = newVisibleArea;

  for (int x = newVisibleTiles.left(); x <= newVisibleTiles.right(); x++) {
    for (int y = newVisibleTiles.top(); y <= newVisibleTiles.bottom(); y++) {
      TileKey key(x, y, newLevel);
      staleTiles.remove(key);
      if (!tileMap.contains(key)) {
        QString filename = map->tilePath(QPoint(key.x, key.y), key.level);
        QPixmap p;
        if (QPixmapCache::find(filename, &p)) {
          // The pixmap is in the cache; display it immediately
          makeTile(key, p);
        } else {
          // Queue the tile for reading
          tileQueueMutex.lock();
          tileQueue.enqueue(QPair<TileKey, QString>(key, filename));
          tileQueueCond.wakeOne();
          tileQueueMutex.unlock();
          tileMap[key] = NULL;
        }
      }
    }
  }
  //  std::cout << "map tiles" << tileMap.size() << " stale tiles: " << staleTiles.size() << std::endl;
  //  drawGrid(bounds);

  pruneStaleTiles();
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
