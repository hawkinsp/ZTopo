#include <QApplication>
#include <QMetaType>
#include <QPixmapCache>
#include "mainwindow.h"
#include "map.h"

int main(int argc, char **argv)
{
  qRegisterMetaType<Tile>("Tile");
  QApplication app(argc, argv);

  MainWindow window;
  QPixmapCache::setCacheLimit(50000);
  window.show();
  return app.exec();
}
