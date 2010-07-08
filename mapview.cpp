#include <algorithm>
#include <cmath>
#include <QEvent>
#include <QGesture>
#include <QGestureEvent>
#include <QGLWidget>
#include <QHBoxLayout>
#include <QSlider>
#include <QPaintEvent>
#include "mapscene.h"
#include "mapview.h"
#include "consts.h"
#include <iostream>



MapView::MapView(MapScene *s, QWidget *parent)
  : QGraphicsView(s, parent), scene(s)
{
  QGLWidget *viewport = new QGLWidget(QGLFormat(QGL::SampleBuffers));
  // QWidget *viewport = new QWidget();
  setViewport(viewport);
  //  setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setMouseTracking(true);
  grabGesture(Qt::PinchGesture);
  maxLevel = s->maxLevel();
  level = maxLevel;
  currentScale = 1.0;
  currentScaleStep = 1.0;
  zoomScene();
}

bool MapView::event(QEvent *ev)
{
  switch (ev->type()) {
  case QEvent::Gesture:
    return gestureEvent((QGestureEvent *)ev);
  default:
    return QGraphicsView::event(ev); 
  }  
}

bool MapView::gestureEvent(QGestureEvent *ev)
{
  QGesture *g = ev->gesture(Qt::PinchGesture);
  if (g) {
    pinchGestureEvent((QPinchGesture *)g);
    return true;
  } else {
    return QGraphicsView::event(ev);
  }
}

void MapView::mouseMoveEvent(QMouseEvent *ev)
{
  QPoint pos = mapToScene(ev->pos()).toPoint();
  emit(positionUpdated(pos));
  QGraphicsView::mouseMoveEvent(ev);
}

void MapView::pinchGestureEvent(QPinchGesture *g)
{
  float oldScale = currentScale * currentScaleStep;
  
  switch (g->state()) {
  case Qt::GestureStarted:
    currentScaleStep = g->scaleFactor();
    break;
  case Qt::GestureUpdated:
    if (g->changeFlags() & QPinchGesture::ScaleFactorChanged) {
      currentScaleStep = g->scaleFactor();
    }
    break;

  case Qt::GestureFinished:
    currentScale = std::max(epsilon, std::min((float)16.0, oldScale));
    currentScaleStep = 1.0;
    break;
    
  case Qt::GestureCanceled:
    currentScaleStep = 1.0;
    break;

  default: abort();
  }

  QPointF screenCenter(mapToScene(rect().center()));
  QPoint pointBeforeScale = mapToScene(g->startCenterPoint().toPoint()).toPoint();
  float deltaScale = currentScale * currentScaleStep / oldScale;
  zoomScene();
  scale(deltaScale, deltaScale);
  QPoint pointAfterScale = mapToScene(g->startCenterPoint().toPoint()).toPoint();
  centerOn(screenCenter + pointBeforeScale - pointAfterScale);
}

void MapView::resizeEvent(QResizeEvent *ev)
{
  QGraphicsView::resizeEvent(ev);
  panOrResizeScene();
}

void MapView::scrollContentsBy(int dx, int dy)
{
  panOrResizeScene();
  QGraphicsView::scrollContentsBy(dx, dy);
}

void MapView::panOrResizeScene()
{
  QPointF topLeft(mapToScene(rect().topLeft()));
  QPointF bottomRight(mapToScene(rect().bottomRight()));
  scene->updateBounds(QRectF(topLeft, bottomRight).toRect(), level);
}

void MapView::zoomScene()
{
  float scale = std::max(std::min(currentScale * currentScaleStep, (float)1.0),
                         epsilon);
  float r = maxLevel + log2f(scale);
  level = (int)ceilf(r);
  emit(scaleChanged(currentScale * currentScaleStep));
}

void MapView::setGridDisplayed(bool display)
{

}
