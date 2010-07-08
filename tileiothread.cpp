#include <QImage>
#include "mapscene.h"
#include "tileiothread.h"

TileIOThread::TileIOThread(MapScene *s, QObject *parent)
  : QThread(parent), scene(s)
{

}

void TileIOThread::run()
{
  forever {
    scene->tileQueueMutex.lock();
    while (scene->tileQueue.isEmpty()) {
      scene->tileQueueCond.wait(&scene->tileQueueMutex);
    }
    QPair<TileKey, QString> pair = scene->tileQueue.dequeue();
    scene->tileQueueMutex.unlock();
    TileKey key = pair.first;
    QString filename = pair.second;
    QImage img;
    if (img.load(filename)) {
      img.setColor(0, qRgba(255, 255, 255, 255));
      emit(tileLoaded(key.x, key.y, key.level, filename, img));
    }
    
  }
}
