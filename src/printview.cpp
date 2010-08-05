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

#include <QDebug>
#include <QEvent>
#include <QGestureEvent>
#include <QGLWidget>
#include <QPinchGesture>
#include <QScrollBar>
#include "printscene.h"
#include "printview.h"


PrintView::PrintView(PrintScene *scene, bool useGL, QWidget *parent)
  : QGraphicsView(scene, parent)
{
  setGL(useGL);
  
  setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  grabGesture(Qt::PinchGesture);
  
  connect(scene, SIGNAL(sceneRectChanged(const QRectF &)),
          this, SLOT(calculateScales()));

  scaleFactor = scaleStep = 1.0;
  calculateScales();
  scaleFactor = fitToViewScale;
  scale(scaleFactor, scaleFactor);
  setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
}

void PrintView::setGL(bool useGL) 
{
  if (useGL) {
    setViewport(new QGLWidget(QGLFormat(QGL::SampleBuffers)));
  } else {
    setViewport(new QWidget());
  }
}


void PrintView::calculateScales()
{
  qreal oldScale = scaleFactor * scaleStep;
  fitToViewScale = std::min(qreal(rect().width()) / scene()->sceneRect().width(),
                            qreal(rect().height()) / scene()->sceneRect().height());
  minScale = fitToViewScale * 0.25;
  maxScale = 16.0;

  if (scaleFactor < minScale)
    scaleFactor = minScale;
  else if (scaleFactor > maxScale)
    scaleFactor = maxScale;
  qreal delta = scaleFactor * scaleStep / oldScale;
  scale(delta, delta);
}

void PrintView::fitToView()
{
  qreal delta = fitToViewScale / scaleFactor * scaleStep * 0.98;
  scale(delta, delta);
}

bool PrintView::event(QEvent *ev)
{
  switch (ev->type()) {
  case QEvent::Gesture:
    return gestureEvent((QGestureEvent *)ev);
  default:
    return QGraphicsView::event(ev); 
  }  
}

bool PrintView::gestureEvent(QGestureEvent *ev)
{
  QGesture *g = ev->gesture(Qt::PinchGesture);
  if (g) {
    pinchGestureEvent((QPinchGesture *)g);
    return true;
  } else {
    return QGraphicsView::event(ev);
  }
}

void PrintView::pinchGestureEvent(QPinchGesture *g)
{
  qreal oldScale = scaleFactor * scaleStep;
  
  switch (g->state()) {
  case Qt::GestureStarted:
    scaleStep = g->scaleFactor();
    smoothScaling = false;
    setRenderHints(QPainter::Antialiasing);
    break;

  case Qt::GestureUpdated:
    if (g->changeFlags() & QPinchGesture::ScaleFactorChanged) {
      scaleStep = g->scaleFactor();
    }
    break;

  case Qt::GestureFinished:
    scaleFactor = std::max(minScale, std::min(maxScale, oldScale));
    scaleStep = 1.0;
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    break;
    
  case Qt::GestureCanceled:
    scaleStep = 1.0;
    break;

  default: abort();
  }

  if (scaleFactor * scaleStep < minScale) scaleStep = minScale / scaleFactor;
  else if (scaleFactor * scaleStep > maxScale) scaleStep = maxScale / scaleFactor;

  qreal deltaScale = scaleFactor * scaleStep / oldScale;
  scale(deltaScale, deltaScale);
}


void PrintView::resizeEvent(QResizeEvent *event)
{
  QGraphicsView::resizeEvent(event);
  calculateScales();
}

