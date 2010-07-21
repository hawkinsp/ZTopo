#include <algorithm>
#include <cassert>
#include <cmath>
#include <QApplication>
#include <QFileInfo>
#include <QFontMetrics>
#include <QPixmapCache>
#include <QLine>
#include <QLineF>
#include <QPolygonF>
#include <QRectF>
#include "consts.h"
#include "map.h"
#include "mapprojection.h"
#include "maprenderer.h"
#include "projection.h"
#include <iostream>
#include <cstdio>

static const int numWorkerThreads = 2;

static const int zoneBoundaryPoints = 10;
static const int maxGridLines = 100;


MapTileIOThread::MapTileIOThread(MapRenderer *v, QObject *parent)
  : QThread(parent), view(v)
{

}

void MapTileIOThread::run()
{
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

    QImage img;
    if (img.load(filename)) {
      QImage imgRgb = img.convertToFormat(QImage::Format_RGB32);
      emit(tileLoaded(key, filename, imgRgb));
    } else {
      emit(tileLoaded(key, filename, img));
    }
    
  }
}


MapRenderer::MapRenderer(Map *m, QObject *parent)
  : QObject(parent), map(m)
{
  for (int d = 0; d < numDatums; d++) {
    for (int z = 0; z < UTM::numZones; z++) {
      zoneBoundaries[d][z] = NULL;
    }
  }

  bytesRead = 0;
  outstandingTiles = 0;
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
  outstandingTiles--;
  if (!img.isNull()) {
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
}

void MapRenderer::bumpScale(int layer, qreal scale, qreal &bumpedScale,
                            int &bumpedTileSize)
{
  int level = std::min(map->zoomLevel(scale), map->layer(layer).maxLevel);
  int tileSize = map->tileSize(level);
  bumpedTileSize = int(tileSize * scale);
  bumpedScale = float(bumpedTileSize) / float(tileSize);
}

// Find the tile we requested, or identify a section of an ancestor tile that
// we can use until the correct tile is loaded.
/*void MapRenderer::findTile(Tile key, QPixmap &p, QRect &r)
{
  for (int level = key.level; level >= 0; level--) {
    int deltaLevel = key.level - level;
    for (int layer = key.layer; layer >= 0; layer--) {
      Tile t(key.x >> deltaLevel, key.y >> deltaLevel, level, layer);

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
  }*/

void MapRenderer::drawTile(Tile key, QPainter &p, const QRect &dstRect)
{
  int logTileSize = map->logBaseTileSize();

  // Look in the current level. If found, we're done and we need not draw
  // anything else
  for (int layer = key.layer; layer >= 0; layer--) {
    Tile t(key.x, key.y, key.level, layer);
    if (tileMap.contains(t) && !tileMap[t].isNull()) {
      p.drawPixmap(dstRect, tileMap[t], 
                   QRect(0, 0, 1 << logTileSize, 1 << logTileSize));
      return;
    }
  }

  // Look at all levels above us
  bool doneAbove = false;
  for (int level = key.level - 1; !doneAbove && level >= 0; level--) {
    int deltaLevel = key.level - level;
    for (int layer = key.layer; !doneAbove && layer >= 0; layer--) {
      Tile t(key.x >> deltaLevel, key.y >> deltaLevel, level, layer);

      if (tileMap.contains(t) && !tileMap[t].isNull()) {
        // Size of the destination tile in the source space
        int logSubSize = logTileSize - deltaLevel;
        int mask = (1 << deltaLevel) - 1;
        int subX = (key.x & mask) << logSubSize;
        int subY = (key.y & mask) << logSubSize;
        int size = 1 << logSubSize;
        p.drawPixmap(dstRect, tileMap[t], QRect(subX, subY, size, size));
        doneAbove = true;
      }
    }
  }

  // Look one level below us. Overdraw whatever we can find on whatever we already
  // drew.
  int level = key.level + 1;
  int deltaLevel = 1;
  int deltaSize = 1 << deltaLevel;
  for (int x = 0; x < deltaSize; x++) {
    for (int y = 0; y < deltaSize; y++) {
      bool found = false;
      for (int layer = map->numLayers(); !found && layer >= 0; layer--) {
        Tile t((key.x << deltaLevel) + x, (key.y << deltaLevel) + y, level, layer);
        if (tileMap.contains(t) && !tileMap[t].isNull()) {
          // Size of the source tile in the destination space
          qreal dstSizeX = qreal(dstRect.width()) / qreal(1 << deltaLevel);
          qreal dstSizeY = qreal(dstRect.height()) / qreal(1 << deltaLevel);
          QRectF dstSubRect(dstRect.left() + dstSizeX * x, 
                            dstRect.top() + dstSizeY * y, dstSizeX, dstSizeY);
          p.drawPixmap(dstSubRect, tileMap[t], 
                        QRectF(0, 0, 1 << logTileSize, 1 << logTileSize));
          found = true;
        }
      }
    }
  }

}


void MapRenderer::render(QPainter &p, int layer, QRect mr, qreal scale)
{
  int level = std::min(map->zoomLevel(scale), map->layer(layer).maxLevel);
  int bumpedTileSize;
  qreal bumpedScale;
  bumpScale(layer, scale, bumpedScale, bumpedTileSize);


  QRect visibleTiles = map->mapRectToTileRect(mr, level);

  p.save();
  p.setCompositionMode(QPainter::CompositionMode_Source);
  
  int mx = int(mr.x() * bumpedScale);
  int my = int(mr.y() * bumpedScale);

  for (int x = visibleTiles.left(); x <= visibleTiles.right(); x++) {
    for (int y = visibleTiles.top(); y <= visibleTiles.bottom(); y++) {
      Tile key(x, y, level, layer);
      //      QPixmap px;
      //      QRect r;
      int vx = x * bumpedTileSize - mx;
      int vy = y * bumpedTileSize - my;
      QRect dstRect(vx, vy, bumpedTileSize, bumpedTileSize);

      drawTile(key, p, dstRect);

      /*      if (!px.isNull()) {
        p.drawPixmap(QRect(vx, vy, bumpedTileSize, bumpedTileSize), px, r);
        }*/
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

void MapRenderer::loadTiles(int layer, QRect vis, qreal scale, bool wait)
{
  int level = std::min(map->zoomLevel(scale), map->layer(layer).maxLevel);

  QRect newVisibleTiles = map->mapRectToTileRect(vis, level);

  for (int x = newVisibleTiles.left(); x <= newVisibleTiles.right(); x++) {
    for (int y = newVisibleTiles.top(); y <= newVisibleTiles.bottom(); y++) {
      Tile key(x, y, level, layer);
      if (!tileMap.contains(key)) {
        QString filename = map->tilePath(key);
        QPixmap p;
        if (!QPixmapCache::find(filename, &p) && 
            !map->layer(layer).missingTiles.containsPrefix(map->tileToQuadKeyInt(key))) {
          // Queue the tile for reading
          
          int size = 0;
          outstandingTiles++;
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

  if (wait) {
    while (outstandingTiles > 0) {
      QApplication::processEvents();
    }
  }
}

QPointF MapRenderer::mapToView(QPoint origin, qreal scale, QPointF p)
{
  return (p - QPointF(origin)) * scale;
}

void MapRenderer::renderGeographicGrid(QPainter &p, QRect area, qreal scale, 
                                       Datum d, qreal interval)
{
  Projection *pj = Geographic::getProjection(d);

  p.save();
  p.scale(scale, scale);
  p.translate(-QPointF(area.topLeft()));
  p.setTransform(map->projToMap(), true);
  renderGrid(p, area, scale, pj, interval);
  p.restore();
}

void MapRenderer::renderUTMGrid(QPainter &p, QRect mRect, qreal scale, 
                                       Datum d, qreal interval)
{
  Projection *pjGeo = Geographic::getProjection(d);
  QRectF pRect = map->mapToProj().mapRect(mRect);

  QPolygonF gPoly = pjGeo->transformFrom(map->projection(), QPolygonF(pRect));

  int minZone = UTM::numZones, maxZone = 0;
  for (int i = 0; i < gPoly.size(); i++) {
    UTM::Zone z = UTM::bestZone(gPoly[i]);
    minZone = std::min(z.zone, minZone);
    maxZone = std::max(z.zone, maxZone);
  }

  p.save();
  p.scale(scale, scale);
  p.translate(-QPointF(mRect.topLeft()));
  p.setTransform(map->projToMap(), true);

  for (int zone = minZone; zone <= maxZone; zone++) {
    Projection *pj = UTM::getZoneProjection(d, zone);

    p.save();
    p.setClipPath(*getUTMZoneBoundary(d, zone));
    renderGrid(p, mRect, scale, pj, interval);
    p.restore();
  }
  p.restore();

}

void MapRenderer::renderGrid(QPainter &p, QRect area, qreal scale, 
                             Projection *pjGrid, qreal interval)
{
  Projection *pjMap = map->projection();
  QRectF parea = map->mapToProj().mapRect(QRectF(area));
         
  QRectF gridBounds = 
    pjGrid->transformFrom(pjMap, QPolygonF(parea)).boundingRect();
  QVector<QLineF> lines;

  int gridMinX = std::floor(gridBounds.left() / interval) - 1;
  int gridMinY = std::floor(gridBounds.top() / interval) - 1;
  int gridMaxX = std::ceil(gridBounds.right() / interval) + 1;
  int gridMaxY = std::ceil(gridBounds.bottom() / interval) + 1;

  if (gridMaxX - gridMinX > maxGridLines || gridMaxY - gridMinY > maxGridLines)
    return;

  //  printf("grid bounds %f %f %f %f\n", gridBounds.left(), gridBounds.top(),
  //       gridBounds.right(), gridBounds.bottom());


  for (int x = gridMinX; x <= gridMaxX; x++) {
    for (int y = gridMinY; y <= gridMaxY; y++) {
      QPointF p =
        pjMap->transformFrom(pjGrid, QPointF(x * interval, y * interval));
      QPointF pr =
        pjMap->transformFrom(pjGrid, QPointF((x + 1) * interval, y * interval));
      QPointF pu =
        pjMap->transformFrom(pjGrid, QPointF(x * interval, (y + 1) * interval));

      //      printf("adding lines at %f %f %f\n", x * interval, y * interval, interval);


      //      printf("adding line %f %f to %f %f and %f %f to %f %f\n",
      //      v.x(), v.y(), vr.x(), vr.y(), v.x(), v.y(), vu.x(), vu.y());
      //      lines << QLine(v, vr) << QLine(v, vu);
      lines << QLineF(p, pr) << QLineF(p, pu);
    }
  }


  QPen pen(QColor(qRgb(0, 0, 255)));
  pen.setWidth(0);
  p.save();
  //  p.setOpacity(0.7);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(pen);

  p.drawLines(lines);
  p.restore();
}

void MapRenderer::rulerInterval(qreal length, qreal &interval, int &tlog)
{
  qreal l = floor(log10(length / 2));
  tlog = int(l);
  qreal tick = pow(10.0, l);
  int ticks = int(length / tick);
  if (ticks >= 2 && ticks < 8) { interval = tick; return; }
  int ticks2 = int(length / (tick * 2));
  if (ticks2 >= 2 && ticks2 < 8) { interval = tick * 2; return; }
  interval = tick * 5;
}

void MapRenderer::renderRuler(QPainter &p, int lengthPx, qreal scale)
{
  qreal metersPerPixel = map->mapPixelSize().width() / scale;
  qreal lengthMeter = qreal(lengthPx) * metersPerPixel;
  qreal lengthMile = lengthMeter / metersPerMile;

  qreal tickMeter, tickMile;
  int tickMeterLog10, tickMileLog10;
  rulerInterval(lengthMeter, tickMeter, tickMeterLog10);
  int tickMeterPixels = tickMeter / metersPerPixel;
  rulerInterval(lengthMile, tickMile, tickMileLog10);
  int tickMilePixels = tickMile * metersPerMile / metersPerPixel;

  //  printf("Ruler %d pixels -> %f m, %f mi, %f m ticks, %f miles ticks, %d m px, %d mi px\n", lengthPx, lengthMeter, lengthMile, tickMeter, tickMile, tickMeterPixels, tickMilePixels); 

  QString meterUnits = tr("m");
  if (tickMeterLog10 >= 3) {
    meterUnits = tr("km");
    tickMeterLog10 -= 3;
    tickMeter /= 1000.0;
  }

  QString mileUnits = tr("mi");


  p.save();

  QFontMetrics fm(p.fontMetrics());

  int tickLen = fm.height() / 2;
  int textSepAbove = fm.leading() + tickLen - fm.descent();
  int textSepBelow = fm.leading() + tickLen + fm.descent();
  int textSep = fm.width("00");

  int lrMargin = textSep;
  int wMeterUnits = fm.width(meterUnits);
  int wMileUnits = fm.width(mileUnits);

  QPen pen(QColor(qRgb(0, 0, 0)));
  pen.setWidth(0);
  p.setPen(pen);
  p.setBrush(QColor(qRgb(255, 255, 255)));

  
  QRect border(0, -textSepAbove - fm.height() - fm.descent(), 
               lengthPx + lrMargin * 2 + textSep + 
                 std::max(wMeterUnits, wMileUnits),
               textSepAbove + textSepBelow + fm.height() * 2);
  p.drawRect(border);

  p.drawLine(QLine(QPoint(lrMargin, 0), QPoint(lrMargin + lengthPx, 0)));

  for (int x = 0, i = 0; x < lengthPx; x += tickMeterPixels, i++) {
    p.drawLine(QLine(QPoint(x + lrMargin, 0), QPoint(x + lrMargin, -tickLen)));
    QString s = 
      QString::number(tickMeter * i, 'f', std::max(0, -tickMeterLog10));
    int w = fm.width(s);
    QRect r(x + lrMargin - w / 2 + 1, -textSepAbove - fm.height(), w, 
            fm.height());
    p.drawText(r, 0, s);
  }

  for (int x = 0, i = 0; x < lengthPx; x += tickMilePixels, i++) {
    p.drawLine(QLine(QPoint(x + lrMargin, 0), QPoint(x + lrMargin, tickLen)));
    QString s = QString::number(tickMile * i, 'f', std::max(0, -tickMileLog10));
    int w = fm.width(s);
    QRect r(x + lrMargin - w / 2 + 1, textSepBelow, w, fm.height());
    p.drawText(r, 0, s);
  }
  
  int x = lengthPx + lrMargin + textSep;
  QRect r(x, -textSepAbove - fm.height(), wMeterUnits, fm.height());
  p.drawText(r, 0, meterUnits);


  r = QRect(x, textSepBelow, wMileUnits, fm.height());
  p.drawText(r, 0, mileUnits);

  //  pen.setWidth(2);
  //  p.setPen(pen);
  p.restore();
}


QPainterPath *MapRenderer::getUTMZoneBoundary(Datum d, int zone)
{
  if (zoneBoundaries[d][zone-1]) return zoneBoundaries[d][zone-1];

  // We ignore the (minor) datum shift on the coarse map bounds
  QRectF mapGeoBounds(map->geographicBounds());
  int minLon, maxLon;
  UTM::zoneLongitudeRange(zone, minLon, maxLon);

  QRectF gridBounds = mapGeoBounds.intersected(QRectF(QPointF(minLon, -90), 
                                                      QPointF(maxLon, 90)));
  QPolygonF gridPoly(gridBounds);

  int size = gridPoly.size();
  assert(size = 4);
  
  Projection *pjGeo = Geographic::getProjection(d);
  QPointF r0 = map->projection()->transformFrom(pjGeo, gridPoly[0]);
  QPainterPath *path = new QPainterPath(r0);  
  for (int i = 0; i < size; i++) {
    int j = ((i + 1) == size) ? 0 : i + 1;

    QPointF p(gridPoly[i]), q(gridPoly[j]);
    for (int pos = 1; pos <= zoneBoundaryPoints; pos++) {
      QPointF r = p * (qreal(zoneBoundaryPoints - pos) / zoneBoundaryPoints) +
                   q * (qreal(pos) / zoneBoundaryPoints);
      QPointF mr = map->projection()->transformFrom(pjGeo, r);
      path->lineTo(mr);

    }
  }
  path->closeSubpath();
  zoneBoundaries[d][zone-1] = path;
  return path;

}
