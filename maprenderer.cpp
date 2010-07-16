#include <algorithm>
#include <cmath>
#include <QFileInfo>
#include <QPixmapCache>
#include "consts.h"
#include "map.h"
#include "maprenderer.h"
#include <iostream>
#include <cstdio>

static const int numWorkerThreads = 2;

MapTileIOThread::MapTileIOThread(MapRenderer *v, QObject *parent)
  : QThread(parent), view(v)
{

}

void MapTileIOThread::run()
{
  QImage img;
  qint64 bytesRead = 0;
  forever {
    view->tileQueueMutex.lock();
    view->bytesRead += bytesRead;
    while (view->tileQueue.isEmpty()) {
      view->tileQueueCond.wait(&view->tileQueueMutex);
    }
    QPair<Tile, QString> pair = view->tileQueue.dequeue();
    view->tileQueueMutex.unlock();

    Tile key = pair.first;
    QString filename = pair.second;
    QFileInfo info(filename);
    bytesRead = info.size();

    if (img.load(filename)) {
      QImage imgRgb = img.convertToFormat(QImage::Format_RGB32);
      emit(tileLoaded(key, filename, imgRgb));
    }
    
  }
}


MapRenderer::MapRenderer(Map *m, QObject *parent)
  : QObject(parent), map(m)
{
  bytesRead = 0;
  for (int i = 0; i < numWorkerThreads; i++) {
    MapTileIOThread *ioThread = new MapTileIOThread(this, this);
    ioThreads << ioThread;
    connect(ioThread, SIGNAL(tileLoaded(Tile, QString, QImage)),
            this, SLOT(tileLoaded(Tile, QString, QImage)));
    ioThread->start();
  }
}

void MapRenderer::tileLoaded(Tile key, QString filename, QImage img)
{
  QPixmap p = QPixmap::fromImage(img);
  QPixmapCache::insert(filename, p);

  //  printf("%lld bytes read\n", bytesRead);

  // Check to see that the tile should still be displayed; it may have already been
  // pruned
  if (tileMap.contains(key)) {
    tileMap[key] = p;
  }
  emit(tileUpdated(key));
}

// Find the tile we requested, or identify a section of an ancestor tile that
// we can use until the correct tile is loaded.
void MapRenderer::findTile(Tile key, QPixmap &p, QRect &r)
{
  for (int level = key.level; level >= 0; level--) {
    int deltaLevel = key.level - level;
    Tile t(key.x >> deltaLevel, key.y >> deltaLevel, level, key.layer);

    if (tileMap.contains(t) && !tileMap[t].isNull()) {
      int logSubSize = map->logBaseTileSize() - deltaLevel;
      int mask = (1 << deltaLevel) - 1;
      int subX = (key.x & mask) << logSubSize;
      int subY = (key.y & mask) << logSubSize;
      int size = 1 << logSubSize;
      p = tileMap[t];
      r = QRect(subX, subY, size, size);
      return;
    }
  }
}

void MapRenderer::bumpScale(qreal scale, qreal &bumpedScale, int &bumpedTileSize)
{
  int level = map->zoomLevel(scale);
  int tileSize = map->tileSize(level);
  bumpedTileSize = int(tileSize * scale);
  bumpedScale = float(bumpedTileSize) / float(tileSize);
}

void MapRenderer::render(QPainter &p, int currentLayer, QRect mr, qreal scale, bool smoothScaling)
{
  int level = map->zoomLevel(scale);
  int tileSize = map->tileSize(level);
  int bumpedTileSize;
  qreal bumpedScale;
  bumpScale(scale, bumpedScale, bumpedTileSize);


  QRect visibleTiles = map->mapRectToTileRect(mr, level);

  p.save();
  p.setRenderHint(QPainter::SmoothPixmapTransform, smoothScaling);
  p.setCompositionMode(QPainter::CompositionMode_Source);
  
  int mx = int(mr.x() * bumpedScale);
  int my = int(mr.y() * bumpedScale);

  for (int x = visibleTiles.left(); x <= visibleTiles.right(); x++) {
    for (int y = visibleTiles.top(); y <= visibleTiles.bottom(); y++) {
      Tile key(x, y, level, currentLayer);
      QPixmap px;
      QRect r;

      findTile(key, px, r);

      if (!px.isNull()) {
        int vx = x * bumpedTileSize - mx;
        int vy = y * bumpedTileSize - my;
        p.drawPixmap(QRect(vx, vy, bumpedTileSize, bumpedTileSize), px, r);
      }
    }
  }

  p.restore();
}

void MapRenderer::pruneTiles(QRect vis)
{
  QMutableMapIterator<Tile, QPixmap> i(tileMap);
  while (i.hasNext()) {
    i.next();
    Tile t = i.key();
    QRect r = map->tileToMapRect(t);
    if (!r.intersects(vis)) {
      i.remove();
    }
  }
}

void MapRenderer::loadTiles(int layer, QRect vis, qreal scale)
{
  int level = map->zoomLevel(scale);

  QRect newVisibleTiles = map->mapRectToTileRect(vis, level);

  for (int x = newVisibleTiles.left(); x <= newVisibleTiles.right(); x++) {
    for (int y = newVisibleTiles.top(); y <= newVisibleTiles.bottom(); y++) {
      Tile key(x, y, level, map->bestLayerAtLevel(level));
      if (!tileMap.contains(key)) {
        QString filename = map->tilePath(key);
        QPixmap p;
        if (!QPixmapCache::find(filename, &p)) {
          // Queue the tile for reading
          
          int size = 0;
          tileQueueMutex.lock();
          tileQueue.enqueue(QPair<Tile, QString>(key, filename));
          size = tileQueue.size();
          tileQueueCond.wakeOne();
          tileQueueMutex.unlock();
        }
        tileMap[key] = p;
      }
    }
  }
}

