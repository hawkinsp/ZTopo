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
#include "maprenderer.h"
#include "mapwidget.h"
#include <iostream>
#include <cstdio>

MapWidget::MapWidget(Map *m, MapRenderer *r, QWidget *parent)
  : QAbstractScrollArea(parent), map(m), renderer(r)
{
  connect(r, SIGNAL(tileUpdated(Tile)), this, SLOT(tileUpdated(Tile)));

  //  setViewport(new QWidget());
  setViewport(new QGLWidget());
  //  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  //  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  setMouseTracking(true);
  smoothScaling = true;
  grabGesture(Qt::PinchGesture);
  scaleFactor = 1.0;
  scaleStep = 1.0;

  minScale = qreal(map->baseTileSize()) / qreal(map->tileSize(0));
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
  renderer->bumpScale(scaleFactor * scaleStep, bumpedScale, bumpedTileSize);

  updateScrollBars();

  currentLayer = map->bestLayerAtLevel(level);

  emit(scaleChanged(scaleFactor * scaleStep));
  tilesChanged();
}

void MapWidget::tileUpdated(Tile key)
{
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
  qreal oldScale = scaleFactor * scaleStep;
  
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

void MapWidget::paintEvent(QPaintEvent *ev)
{
  QAbstractScrollArea::paintEvent(ev);
  QRect mr = visibleArea();
  int level = zoomLevel();
  QRect visibleTiles = map->mapRectToTileRect(mr, level);

  QPainter p(viewport());
  renderer->render(p, currentLayer, mr, bumpedScale, smoothScaling);
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

  renderer->pruneTiles(vis);

  for (int level = std::max(0, zoomLevel() - 1); level <= zoomLevel(); level++) {
    renderer->loadTiles(map->bestLayerAtLevel(level), vis, bumpedScale);
  }
  //  drawGrid(bounds);
}

int MapWidget::zoomLevel()
{
  qreal scale = std::max(std::min(scaleFactor * scaleStep, qreal(1.0)),
                         qreal(epsilon));
  qreal r = maxLevel() + log2(scale);
  return std::max(0, int(ceil(r)));
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
