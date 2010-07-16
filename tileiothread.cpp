#include <QImage>
#include "map.h"
#include "mapwidget.h"
#include "tileiothread.h"

TileIOThread::TileIOThread(MapWidget *v, QObject *parent)
  : QThread(parent), view(v)
{

}

void TileIOThread::run()
{
  forever {
    view->tileQueueMutex.lock();
    while (view->tileQueue.isEmpty()) {
      view->tileQueueCond.wait(&view->tileQueueMutex);
    }
    QPair<Tile, QString> pair = view->tileQueue.dequeue();
    view->tileQueueMutex.unlock();
    Tile key = pair.first;
    QString filename = pair.second;
    QImage img;
    if (img.load(filename)) {
      emit(tileLoaded(key, filename, img));
    }
    
  }
}
