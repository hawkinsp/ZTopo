#ifndef TILEIOTHREAD_H
#define TILEIOTHREAD_H 1

#include <QThread>
#include <QImage>
#include "map.h"

class MapWidget;

class TileIOThread : public QThread {
  Q_OBJECT;

public:
  TileIOThread(MapWidget *s, QObject *parent = 0);

protected:
  void run();

private:
  MapWidget *view;

signals:
  void tileLoaded(Tile tile, QString filename, QImage img);
};

#endif
