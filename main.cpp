#include <QApplication>
#include <QMetaType>
#include <QPixmapCache>
#include <QTransform>
#include "mainwindow.h"
#include "map.h"
#include "mapprojection.h"
#include "projection.h"

#include <QFile>

int main(int argc, char **argv)
{
  qRegisterMetaType<Tile>("Tile");
  QCoreApplication::setOrganizationName("Racing Snail Software");
  QCoreApplication::setApplicationName("Topograhic Map Viewer");

  QApplication app(argc, argv);

  QTransform ctr;
  Projection pj(californiaMapProjection);
  californiaProjToMapTransform(ctr);
  Map map(NAD83, &pj, ctr, californiaMapSize);
  for (int layer = 0; layer < map.numLayers(); layer++) {
    QFile missing(map.missingTilesPath(layer));
    map.loadMissingTiles(layer, missing);
  }

  MainWindow *window = new MainWindow(&map);
  QPixmapCache::setCacheLimit(50000);

  window->show();
  return app.exec();
}
