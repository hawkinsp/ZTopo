#ifndef MAPWIDGET_H
#define MAPWIDGET_H 1

#include <QAbstractScrollArea>
#include <QMap>
#include <QPair>
#include <QPixmap>

#include "map.h"
#include "projection.h"
class QPaintEvent;
class QGestureEvent;
class QPinchGesture;
class MapRenderer;

class MapWidget : public QAbstractScrollArea {
  Q_OBJECT
public:
  MapWidget(Map *m, MapRenderer *r, QWidget *parent = 0);

  // Top left of the view in map coordinates
  QPoint viewTopLeft();

  // Center of the view in map coordinates
  QPoint center();

  // Center on a point in map coordinates
  void centerOn(QPoint p);

  // Convert view coordinates to map coordinates
  QPoint viewToMap(QPoint v);
  QRect viewToMapRect(QRect v);

  // Visible area in map coordinates
  QRect visibleArea();

  // Layer choice
  void setLayer(int layer);

  // Set grid projection
  void showGrid(Datum d, bool utm, qreal interval);
  void hideGrid();

public slots:
  void tileUpdated(Tile key);

signals:
  void positionUpdated(QPoint pos);
  void scaleChanged(float scale);

protected:
  virtual void paintEvent(QPaintEvent *event);
  virtual bool event(QEvent *event);
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
  qreal currentScale() { return bumpedScale; }
  int currentLayer(); // Current map layer

  // Last mouse position observed in a mouse move event
  QPoint lastMousePos;
  bool panning; // Are we in the middle of a panning event?

  // Current layer; negative means choose automatically
  int selectedLayer;


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

  int zoomLevel();
  int maxLevel() { return map->maxLevel(); }
};

#endif
