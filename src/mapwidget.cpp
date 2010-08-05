/*
  ZTopo --- a viewer for topographic maps
  Copyright (C) 2010 Peter Hawkins
  
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <algorithm>
#include <cmath>
#include <QDebug>
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

MapWidget::MapWidget(Map *m, MapRenderer *r, bool useGL, QWidget *parent)
  : QAbstractScrollArea(parent), map(m), renderer(r)
{
  renderer->addClient(this);
  selectedLayer = -1;

  minScale = qreal(map->baseTileSize()) / qreal(map->tileSize(0));
  maxScale = 16.0;

  scaleFactor = minScale * 3.0;
  scaleStep = 1.0;

  setDpi(0);

  panning = false;

  gridEnabled = false;
  showRuler = true;

  setGL(useGL);

  connect(&r->getCache(), SIGNAL(tileLoaded()), viewport(), SLOT(update()));

  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  setMouseTracking(true);
  smoothScaling = true;
  grabGesture(Qt::PinchGesture);
  zoomChanged();

  flagPixmap = QPixmap(":/images/flag.png");
}

MapWidget::~MapWidget()
{
  renderer->removeClient(this);
}

void MapWidget::setDpi(int aDpi)
{
  if (dpi <= 0) {
    dpi = logicalDpiX();
  }
  dpi = aDpi;
  emit(mapScaleChanged(currentMapScale()));
}

void MapWidget::setGL(bool useGL) 
{
  if (useGL) {
    setViewport(new QGLWidget(QGLFormat(QGL::SampleBuffers)));
  } else {
    setViewport(new QWidget());
  }
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
  //  bumpedScale = scaleFactor * scaleStep;

  updateScrollBars();

  emit(mapScaleChanged(currentMapScale()));
  tilesChanged();
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

void MapWidget::mouseDoubleClickEvent(QMouseEvent *ev)
{
  QAbstractScrollArea::mouseDoubleClickEvent(ev);
  QPoint screenCenter(center());
  QPoint pointBeforeScale = viewToMap(ev->pos());
  zoomIn();
  QPoint pointAfterScale = viewToMap(ev->pos());
  centerOn(screenCenter + pointBeforeScale - pointAfterScale);
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

  viewport()->update();
}

int MapWidget::currentLayer() const
{
  int level = map->zoomLevel(currentScale());
  int layer = selectedLayer < 0 ? map->bestLayerAtLevel(level) : selectedLayer;
  return layer;
}

qreal MapWidget::currentMapScale() const
{
  return (map->mapPixelSize().width() * dpi / metersPerInch)
    / currentScale();
}

void MapWidget::tilesChanged()
{
  QRect vis(visibleArea());
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

  // Draw the grid
  if (gridEnabled) {
    QPen pen(QColor(qRgb(0, 0, 255)));
    pen.setWidth(0);
    p.setPen(pen);
    if (gridUTM) {
      renderer->renderUTMGrid(p, mr, currentScale(), gridDatum, gridInterval);
    } else {
      renderer->renderGeographicGrid(p, mr, currentScale(), gridDatum, 
                                     gridInterval);
    }
  }

  // Draw search results
  if (searchResultsVisible) {
    foreach (QPoint mp, searchResults) {
      if (mr.contains(mp)) {
        QPoint v = mapToView(mp);
        QPointF origin(v.x() - flagPixmap.width() / 2, v.y() - flagPixmap.height());
        p.drawPixmap(origin, flagPixmap);
      }
    }
  }

  // Draw the ruler
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
  viewport()->update();
}

static const qreal zoomIncrement = 1.333;
void MapWidget::zoomIn()
{
  setScale(currentScale() * zoomIncrement);
}

void MapWidget::zoomOut()
{
  setScale(currentScale() / zoomIncrement);
}

void MapWidget::setRulerVisible(bool v)
{
  showRuler = v;
  viewport()->update();
}


QPoint MapWidget::center() const
{
  return QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());
}

QPoint MapWidget::mapToView(QPoint p) const
{
  return (QPointF(p - viewTopLeft()) * currentScale()).toPoint();
}


QPoint MapWidget::viewToMap(QPoint p) const
{
  return viewTopLeft() + QPoint(p.x() / currentScale(), p.y() / currentScale());
}

QRect MapWidget::viewToMap(QRect r) const
{
  return QRect(r.x() / currentScale(), r.y() / currentScale(), 
               r.width() / currentScale(), 
               r.height() / currentScale()).translated(viewTopLeft());
}

// Visible area in map coordinates
QRect MapWidget::visibleArea() const
{
  int mw = int(width() / currentScale());
  int mh = int(height() / currentScale());
  QPoint c = center();

  return QRect(center() - QPoint(mw / 2, mh / 2), QSize(mw, mh));
}

QPoint MapWidget::viewTopLeft() const
{
  return visibleArea().topLeft();
}


void MapWidget::setLayer(int l)
{
  selectedLayer = l;
  tilesChanged();
  viewport()->update();
}

void MapWidget::showGrid(Datum d, bool utm, qreal interval)
{
  gridEnabled = true;
  gridDatum = d;
  gridUTM = utm;
  gridInterval = interval;
  viewport()->update();
}

void MapWidget::hideGrid()
{
  gridEnabled = false;
  viewport()->update();
}

void MapWidget::setSearchResults(const QList<QPoint> &ps)
{
  searchResults = ps;
  if (searchResultsVisible) viewport()->update();
}

void MapWidget::setSearchResultsVisible(bool vis)
{
  searchResultsVisible = vis;
  viewport()->update();
}
