#ifndef TILEIOTHREAD_H
#define TILEIOTHREAD_H 1

#include <QThread>
#include <QImage>

class MapScene;

class TileIOThread : public QThread {
  Q_OBJECT;

public:
  TileIOThread(MapScene *s, QObject *parent = 0);

protected:
  void run();

private:
  MapScene *scene;

signals:
  void tileLoaded(int x, int y, int level, QString filename, QImage img);
};

#endif
