#include <QApplication>
#include <QPixmapCache>
#include "mainwindow.h"

int main(int argc, char **argv)
{
  QApplication app(argc, argv);
  MainWindow window;
  QPixmapCache::setCacheLimit(50000);
  window.show();
  return app.exec();
}
