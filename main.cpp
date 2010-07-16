#include <QApplication>
#include <QMetaType>
#include <QPixmapCache>
#include "mainwindow.h"
#include "map.h"

int main(int argc, char **argv)
{
  QApplication app(argc, argv);
  MainWindow window;
  QPixmapCache::setCacheLimit(50000);
  qRegisterMetaType<Tile>("Tile");
  window.show();
  return app.exec();
}
