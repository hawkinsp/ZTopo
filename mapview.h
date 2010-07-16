#ifndef MAPVIEW_H
#define MAPVIEW_H 1

#include <QGraphicsView>

class QGestureEvent;
class QPinchGesture;
class MapScene;

class MapView : public QGraphicsView {
  Q_OBJECT
public:
  MapView(MapScene *s, QWidget *parent = 0);

  public slots:
  void setGridDisplayed(bool display);

protected:
  virtual bool event(QEvent *event);
  virtual void mouseMoveEvent(QMouseEvent *event);
  virtual void resizeEvent(QResizeEvent *event);
  virtual void scrollContentsBy(int dx, int dy);

private:
  MapScene *scene;

  void positionChanged();
  void panOrResizeScene();
  void zoomScene();
  bool gestureEvent(QGestureEvent *ev);
  void pinchGestureEvent(QPinchGesture *g);

  int maxLevel;
  int level;

  float currentScale;
  float currentScaleStep;

  QPoint lastMousePos;

signals:
  void positionUpdated(QPoint pos);
  void scaleChanged(float scale);
};

#endif
