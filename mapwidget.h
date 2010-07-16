#ifndef MAPWIDGET_H
#define MAPWIDGET_H 1

#include <QAbstractScrollArea>
#include <QMap>
#include <QMutex>
#include <QPair>
#include <QPixmap>
#include <QQueue>
#include <QWaitCondition>

#include "map.h"
class QPaintEvent;
class QGestureEvent;
class QPinchGesture;
class TileIOThread;

class MapWidget : public QAbstractScrollArea {
  Q_OBJECT
public:
  MapWidget(Map *m, QWidget *parent = 0);

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


  // Public for thread class
  QMutex tileQueueMutex;
  QWaitCondition tileQueueCond;
  QQueue<QPair<Tile, QString> > tileQueue;

public slots:
  void tileLoaded(Tile key, QString filename, QImage img);

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
  TileIOThread *ioThread;

  Map *map;

  bool smoothScaling;

  float scaleFactor;
  float scaleStep;
  float bumpedScale;
  int   bumpedTileSize;
  float scale() { return bumpedScale; }

  // Last detected mouse position
  QPoint lastMousePos;

  int currentLayer;

  // All currently loaded tiles.
  // Tiles are present but have value NULL if they are queued to be loaded from disk.
  QMap<Tile, QPixmap> tileMap;

  bool gestureEvent(QGestureEvent *ev);
  void pinchGestureEvent(QPinchGesture *g);


  void positionChanged();
  void tilesChanged();
  void zoomChanged();

  void findTile(Tile key, QPixmap &p, QRect &r);

  int zoomLevel();
  int maxLevel() { return map->maxLevel(); }
};

#endif
