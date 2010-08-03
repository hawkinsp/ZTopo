#ifndef PRINTVIEW_H
#define PRINTVIEW_H 1

#include <QGraphicsView>
class QGestureEvent;
class QPinchGesture;

class PrintScene;

class PrintView : public QGraphicsView {
  Q_OBJECT;
public:
  PrintView(PrintScene *scene, bool useGL, QWidget *parent = 0);

  void fitToView();

  void setGL(bool use);

protected:
  virtual bool event(QEvent *event);
  virtual void resizeEvent(QResizeEvent *event);
private:
  bool gestureEvent(QGestureEvent *ev);
  void pinchGestureEvent(QPinchGesture *g);

  qreal scaleFactor;
  qreal scaleStep;

  qreal minScale, fitToViewScale, maxScale;

  bool smoothScaling;

  QPointF gestureViewCenter;

private slots:
  void calculateScales();
};

#endif
