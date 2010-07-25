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

#ifndef MAPWIDGET_H
#define MAPWIDGET_H 1

#include <QAbstractScrollArea>
#include <QMap>
#include <QPair>
#include <QPixmap>

#include "map.h"
#include "maprenderer.h"
#include "projection.h"
class QPaintEvent;
class QGestureEvent;
class QPinchGesture;


class MapWidget : public QAbstractScrollArea, public MapRendererClient {
  Q_OBJECT
public:
  MapWidget(Map *m, MapRenderer *r, QWidget *parent = 0);
  ~MapWidget();


  // Center on a point in map coordinates
  void centerOn(QPoint p);

  // Set the current scale factor
  qreal currentScale() const { return bumpedScale; }
  void setScale(qreal scale);

  // Show/hide ruler
  void setRulerVisible(bool vis);

  // Set grid projection
  void showGrid(Datum d, bool utm, qreal interval);
  void hideGrid();


  // Top left of the view in map coordinates
  QPoint viewTopLeft() const;

  // Center of the view in map coordinates
  QPoint center() const;


  // Convert view coordinates to map coordinates
  QPoint viewToMap(QPoint v) const;
  QRect viewToMapRect(QRect v) const;

  // Visible area in map coordinates
  virtual QRect visibleArea() const;
  virtual int currentLayer() const; // Current map layer

  // Layer choice
  void setLayer(int layer);

signals:
  void positionUpdated(QPoint pos);
  void scaleChanged(float scale);

protected:
  virtual void paintEvent(QPaintEvent *event);
  virtual bool event(QEvent *event);
  virtual void mousePressEvent(QMouseEvent *event);
  virtual void mouseMoveEvent(QMouseEvent *event);
  virtual void resizeEvent(QResizeEvent *event);
  virtual void scrollContentsBy(int dx, int dy);

private:
  Map *map;
  MapRenderer *renderer;

  // Display map tiles using smooth scaling? We turn smoothing off while zooming
  // for speed, but we turn it on when we have a motionless map view.
  bool smoothScaling;

  qreal minScale, maxScale;

  qreal scaleFactor;
  qreal scaleStep;
  qreal bumpedScale;

  // Last mouse position observed in a mouse move event
  QPoint lastMousePos;
  bool panning; // Are we in the middle of a panning event?

  // Current layer; negative means choose automatically
  int selectedLayer;

  bool showRuler; // Show scale ruler

  // Map grid
  bool gridEnabled;
  Datum gridDatum;
  bool gridUTM;
  qreal gridInterval;

  bool gestureEvent(QGestureEvent *ev);
  void pinchGestureEvent(QPinchGesture *g);

  void positionChanged();
  void tilesChanged();
  void updateScrollBars();
  void zoomChanged();

  int zoomLevel() const;
  int maxLevel() const { return map->maxLevel(); }
};

#endif
