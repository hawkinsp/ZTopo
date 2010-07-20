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
  selectedLayer = -1;
  scaleFactor = 1.0;
  scaleStep = 1.0;

  minScale = qreal(map->baseTileSize()) / qreal(map->tileSize(0));
  maxScale = 16.0;

  panning = false;

  gridEnabled = false;
  showRuler = true;


  connect(r, SIGNAL(tileUpdated(Tile)), this, SLOT(tileUpdated(Tile)));

  //  setViewport(new QWidget());
  setViewport(new QGLWidget(QGLFormat(QGL::SampleBuffers)));
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  setMouseTracking(true);
  smoothScaling = true;
  grabGesture(Qt::PinchGesture);
  zoomChanged();
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
  int level = map->zoomLevel(scaleFactor * scaleStep);
  int layer = selectedLayer < 0 ? map->bestLayerAtLevel(level) : selectedLayer;
  int bumpedTileSize;
  renderer->bumpScale(layer, scaleFactor * scaleStep, bumpedScale, bumpedTileSize);

  updateScrollBars();

  emit(scaleChanged(scaleFactor * scaleStep));
  tilesChanged();
}

void MapWidget::tileUpdated(Tile)
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

void MapWidget::mousePressEvent(QMouseEvent *ev)
{
  QAbstractScrollArea::mousePressEvent(ev);
  lastMousePos = ev->pos();
}

void MapWidget::mouseMoveEvent(QMouseEvent *ev)
{
  QAbstractScrollArea::mouseMoveEvent(ev);

  if (ev->buttons() & Qt::LeftButton) {
    QPoint before = viewToMap(lastMousePos);
    QPoint after = viewToMap(ev->pos());
    QPoint delta = after - before;
    centerOn(center() - delta);
  }
  lastMousePos = ev->pos();
  positionChanged();
}

void MapWidget::positionChanged()
{
  emit(positionUpdated(viewToMap(lastMousePos)));
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
  QPoint pointBeforeScale = viewToMap(g->startCenterPoint().toPoint());
  zoomChanged();
  QPoint pointAfterScale = viewToMap(g->startCenterPoint().toPoint());
  centerOn(screenCenter + pointBeforeScale - pointAfterScale);

  repaint();
}

int MapWidget::currentLayer()
{
  int level = map->zoomLevel(currentScale());
  int layer = selectedLayer < 0 ? map->bestLayerAtLevel(level) : selectedLayer;
  return layer;
}

void MapWidget::tilesChanged()
{
  QRect vis(visibleArea());
  renderer->pruneTiles(vis);
  renderer->loadTiles(currentLayer(), vis, currentScale());
}

void MapWidget::paintEvent(QPaintEvent *ev)
{
  QAbstractScrollArea::paintEvent(ev);
  QRect vr = viewport()->rect();
  QRect mr = visibleArea();

  QPainter p(viewport());
  p.setBackground(Qt::white);
  p.eraseRect(p.viewport());
  p.setRenderHint(QPainter::SmoothPixmapTransform, smoothScaling);
  renderer->render(p, currentLayer(), mr, currentScale());
  if (gridEnabled) {
    if (gridUTM) {
      renderer->renderUTMGrid(p, mr, currentScale(), gridDatum, gridInterval);
    } else {
      renderer->renderGeographicGrid(p, mr, currentScale(), gridDatum, 
                                     gridInterval);
    }
  }
  if (showRuler) {
    p.save();
    p.translate(5, vr.height() - 30);
    renderer->renderRuler(p, vr.width() / 3, currentScale());
    p.restore();
  }
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


void MapWidget::centerOn(QPoint p)
{
  horizontalScrollBar()->setValue(p.x());
  verticalScrollBar()->setValue(p.y());
}

void MapWidget::setScale(qreal scale)
{  
  if (scale < minScale)      { scaleFactor = minScale; scaleStep = 1.0; }
  else if (scale < maxScale) { scaleFactor = scale;    scaleStep = 1.0; }
  else if (scale > maxScale) { scaleFactor = maxScale; scaleStep = 1.0; }
  // NaN scale -> no changed

  zoomChanged();
  repaint();
}

void MapWidget::setRulerVisible(bool v)
{
  showRuler = v;
  repaint();
}


QPoint MapWidget::center()
{
  return QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());
}


QPoint MapWidget::viewToMap(QPoint p)
{
  return viewTopLeft() + QPoint(p.x() / currentScale(), p.y() / currentScale());
}

QRect MapWidget::viewToMapRect(QRect r)
{
  return QRect(r.x() / currentScale(), r.y() / currentScale(), 
               r.width() / currentScale(), 
               r.height() / currentScale()).translated(viewTopLeft());
}

// Visible area in map coordinates
QRect MapWidget::visibleArea()
{
  int mw = int(width() / currentScale());
  int mh = int(height() / currentScale());
  QPoint c = center();

  return QRect(center() - QPoint(mw / 2, mh / 2), QSize(mw, mh));
}

QPoint MapWidget::viewTopLeft()
{
  return visibleArea().topLeft();
}


void MapWidget::setLayer(int l)
{
  selectedLayer = l;
  tilesChanged();
  repaint();
}

void MapWidget::showGrid(Datum d, bool utm, qreal interval)
{
  gridEnabled = true;
  gridDatum = d;
  gridUTM = utm;
  gridInterval = interval;
  repaint();
}

void MapWidget::hideGrid()
{
  gridEnabled = false;
  repaint();
}
