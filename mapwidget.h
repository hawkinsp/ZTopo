#ifndef MAPWIDGET_H
#define MAPWIDGET_H 1

#include <QAbstractScrollArea>
#include <QMap>
#include <QPair>
#include <QPixmap>

#include "map.h"
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

  // Convert a view coordinate to a map coordinate
  QPoint viewToMap(QPoint v);
  QRect viewToMapRect(QRect v);

  // Convert a viewport coordinate to a map coordinate
  QPoint viewportToMap(QPoint v);

  // Visible area in map coordinates
  QRect visibleArea();

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

  bool smoothScaling;

  qreal minScale, maxScale;

  qreal scaleFactor;
  qreal scaleStep;
  qreal bumpedScale;
  int   bumpedTileSize;
  float scale() { return bumpedScale; }

  // Last detected mouse position
  QPoint lastMousePos;

  int currentLayer;

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
