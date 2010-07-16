#include <QImage>
#include <QFileInfo>
#include "map.h"
#include "mapwidget.h"
#include "tileiothread.h"

TileIOThread::TileIOThread(MapWidget *v, QObject *parent)
  : QThread(parent), view(v)
{

}

void TileIOThread::run()
{
  QImage img;
  qint64 bytesRead = 0;
  forever {
    view->tileQueueMutex.lock();
    view->bytesRead += bytesRead;
    while (view->tileQueue.isEmpty()) {
      view->tileQueueCond.wait(&view->tileQueueMutex);
    }
    QPair<Tile, QString> pair = view->tileQueue.dequeue();
    view->tileQueueMutex.unlock();

    Tile key = pair.first;
    QString filename = pair.second;
    QFileInfo info(filename);
    bytesRead = info.size();

    if (img.load(filename)) {
      QImage imgRgb = img.convertToFormat(QImage::Format_RGB32);
      emit(tileLoaded(key, filename, imgRgb));
    }
    
  }
}
