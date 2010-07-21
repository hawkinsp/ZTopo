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
  qreal currentScale() { return bumpedScale; }
  void setScale(qreal scale);

  // Show/hide ruler
  void setRulerVisible(bool vis);

  // Set grid projection
  void showGrid(Datum d, bool utm, qreal interval);
  void hideGrid();


  // Top left of the view in map coordinates
  QPoint viewTopLeft();

  // Center of the view in map coordinates
  QPoint center();


  // Convert view coordinates to map coordinates
  QPoint viewToMap(QPoint v);
  QRect viewToMapRect(QRect v);

  // Visible area in map coordinates
  virtual QRect visibleArea();
  virtual int currentLayer(); // Current map layer

  // Layer choice
  void setLayer(int layer);

public slots:
  void tileUpdated(Tile key);

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

  int zoomLevel();
  int maxLevel() { return map->maxLevel(); }
};

#endif
