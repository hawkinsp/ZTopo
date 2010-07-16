#include <algorithm>
#include <cmath>
#include <QPainter>
#include <QPaintEvent>
#include <QPinchGesture>
#include <QPixmapCache>
#include <QScrollBar>
#include <QGLWidget>
#include "consts.h"
#include "map.h"
#include "mapwidget.h"
#include "tileiothread.h"
#include <iostream>
#include <cstdio>

static const int numWorkerThreads = 2;
MapWidget::MapWidget(Map *m, QWidget *parent)
  : QAbstractScrollArea(parent), map(m)
{
  bytesRead = 0;
  for (int i = 0; i < numWorkerThreads; i++) {
    TileIOThread *ioThread = new TileIOThread(this, this);
    ioThreads << ioThread;
    connect(ioThread, SIGNAL(tileLoaded(Tile, QString, QImage)),
            this, SLOT(tileLoaded(Tile, QString, QImage)));
    ioThread->start();
  }

  //  setViewport(new QWidget());
  setViewport(new QGLWidget());
  //  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  //  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  setMouseTracking(true);
  smoothScaling = true;
  grabGesture(Qt::PinchGesture);
  scaleFactor = 1.0;
  scaleStep = 1.0;

  minScale = float(map->baseTileSize()) / float(map->tileSize(0));
  maxScale = 16.0;
  zoomChanged();

  currentLayer = 0;
}

void MapWidget::updateScrollBars()
{
  QSize size = map->requestedSize();
  QRect r = visibleArea();
  //  int w = r.width() / 2;
  //  int h = r.height() / 2;
  horizontalScrollBar()->setRange(0, size.width());
  verticalScrollBar()->setRange(0, size.height());


  QScrollBar *hScroll = horizontalScrollBar();
  QScrollBar *vScroll = verticalScrollBar();
  hScroll->setSingleStep(r.width() / 20);
  hScroll->setPageStep(r.width());
  vScroll->setSingleStep(r.height() / 20);
  vScroll->setPageStep(r.height());
}

void MapWidget::zoomChanged()
{
  // Find the nearest integer scaled tile size and adjust the scale factor to fit
  int level = zoomLevel();
  int tileSize = map->tileSize(level);
  bumpedTileSize = int(tileSize * scaleFactor * scaleStep);
  bumpedScale = float(bumpedTileSize) / float(tileSize);

  updateScrollBars();

  currentLayer = map->bestLayerAtLevel(level);

  emit(scaleChanged(scaleFactor * scaleStep));
  tilesChanged();
}

void MapWidget::tileLoaded(Tile key, QString filename, QImage img)
{
  QPixmap p = QPixmap::fromImage(img);
  QPixmapCache::insert(filename, p);

  //  printf("%lld bytes read\n", bytesRead);

  // Check to see that the tile should still be displayed; it may have already been
  // pruned
  if (tileMap.contains(key)) {
    tileMap[key] = p;
  }
  repaint();
}

bool MapWidget::event(QEvent *ev)
{
  switch (ev->type()) {
  case QEvent::Gesture:
    return gestureEvent((QGestureEvent *)ev);
  default:
    return QAbstractScrollArea::event(ev); 
  }  
}

bool MapWidget::gestureEvent(QGestureEvent *ev)
{
  QGesture *g = ev->gesture(Qt::PinchGesture);
  if (g) {
    pinchGestureEvent((QPinchGesture *)g);
    return true;
  } else {
    return QAbstractScrollArea::event(ev);
  }
}

void MapWidget::mouseMoveEvent(QMouseEvent *ev)
{
  QAbstractScrollArea::mouseMoveEvent(ev);
  lastMousePos = ev->pos();
  positionChanged();
}

void MapWidget::positionChanged()
{
  emit(positionUpdated(viewportToMap(lastMousePos)));
}

void MapWidget::pinchGestureEvent(QPinchGesture *g)
{
  float oldScale = scaleFactor * scaleStep;
  
  switch (g->state()) {
  case Qt::GestureStarted:
    scaleStep = g->scaleFactor();
    smoothScaling = false;
    break;

  case Qt::GestureUpdated:
    if (g->changeFlags() & QPinchGesture::ScaleFactorChanged) {
      scaleStep = g->scaleFactor();
    }
    break;

  case Qt::GestureFinished:
    scaleFactor = std::max(minScale, std::min(maxScale, oldScale));
    scaleStep = 1.0;
    smoothScaling = true;
    break;
    
  case Qt::GestureCanceled:
    scaleStep = 1.0;
    break;

  default: abort();
  }

  if (scaleFactor * scaleStep < minScale) scaleStep = minScale / scaleFactor;
  else if (scaleFactor * scaleStep > maxScale) scaleStep = maxScale / scaleFactor;

  QPoint screenCenter(center());
  QPoint pointBeforeScale = viewportToMap(g->startCenterPoint().toPoint());
  zoomChanged();
  QPoint pointAfterScale = viewportToMap(g->startCenterPoint().toPoint());
  centerOn(screenCenter + pointBeforeScale - pointAfterScale);

  repaint();
}

// Find the tile we requested, or identify a section of an ancestor tile that
// we can use until the correct tile is loaded.
void MapWidget::findTile(Tile key, QPixmap &p, QRect &r)
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

void MapWidget::paintEvent(QPaintEvent *ev)
{
  QRect mr = visibleArea();

  QAbstractScrollArea::paintEvent(ev);
  int level = zoomLevel();
  QRect visibleTiles = map->mapRectToTileRect(mr, level);

  QPainter p(viewport());
  p.setRenderHint(QPainter::SmoothPixmapTransform, smoothScaling);
  p.setCompositionMode(QPainter::CompositionMode_Source);
  p.save();
  
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

void MapWidget::resizeEvent(QResizeEvent *ev)
{
  QAbstractScrollArea::resizeEvent(ev);
  updateScrollBars();
  tilesChanged();
}

void MapWidget::scrollContentsBy(int dx, int dy)
{
  QAbstractScrollArea::scrollContentsBy(dx, dy);
  tilesChanged();
  positionChanged();
}


void MapWidget::tilesChanged()
{
  QRect vis(visibleArea());

  QMutableMapIterator<Tile, QPixmap> i(tileMap);
  while (i.hasNext()) {
    i.next();
    Tile t = i.key();
    QRect r = map->tileToMapRect(t);
    if (!r.intersects(vis)) {
      i.remove();
    }
  }

  for (int level = std::max(0, zoomLevel() - 1); level <= zoomLevel(); level++) {
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
  //  drawGrid(bounds);

  //  pruneStaleTiles();
}

int MapWidget::zoomLevel()
{
  float scale = std::max(std::min(scaleFactor * scaleStep, (float)1.0),
                         epsilon);
  float r = maxLevel() + log2f(scale);
  return std::max(0, int(ceilf(r)));
}

QPoint MapWidget::center()
{
  return QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());
}

void MapWidget::centerOn(QPoint p)
{
  horizontalScrollBar()->setValue(p.x());
  verticalScrollBar()->setValue(p.y());
}

QPoint MapWidget::viewToMap(QPoint p)
{
  return QPoint(p.x() / scale(), p.y() / scale());
}

QRect MapWidget::viewToMapRect(QRect r)
{
  return QRect(r.x() / scale(), r.y() / scale(), r.width() / scale(), r.height() / scale());
}

// Visible area in map coordinates
QRect MapWidget::visibleArea()
{
  int mw = int(width() / scale());
  int mh = int(height() / scale());
  QPoint c = center();

  return QRect(center() - QPoint(mw / 2, mh / 2), QSize(mw, mh));
}

QPoint MapWidget::viewTopLeft()
{
  return visibleArea().topLeft();
}

QPoint MapWidget::viewportToMap(QPoint p)
{
  QPoint tl = viewTopLeft();
  int x = tl.x() + int(p.x() / bumpedScale);
  int y = tl.y() + int(p.y() / bumpedScale);
  return QPoint(x, y);
}
