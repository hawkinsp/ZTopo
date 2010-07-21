#include <QApplication>
#include <QMetaType>
#include <QPixmapCache>
#include <QStringBuilder>
#include <QTransform>
#include "mainwindow.h"
#include "map.h"
#include "maprenderer.h"
#include "projection.h"
#include "rootdata.h"
#include "proj_api.h"

#include <QFile>
#include <QUrl>

#ifdef Q_WS_MAC
#include <CoreFoundation/CoreFoundation.h>
#endif

int main(int argc, char **argv)
{
  qRegisterMetaType<Tile>("Tile");
  QCoreApplication::setOrganizationName("Racing Snail Software");
  QCoreApplication::setApplicationName("Topograhic Map Viewer");

  QApplication app(argc, argv);

  QString rootDataName("root.json");
 #ifdef Q_WS_MAC
  CFURLRef appUrlRef = CFBundleCopyBundleURL(CFBundleGetMainBundle());
  CFStringRef macPath = CFURLCopyFileSystemPath(appUrlRef,
                                                kCFURLPOSIXPathStyle);
  const char *pathPtr = CFStringGetCStringPtr(macPath,
                                              CFStringGetSystemEncoding());
  if (!pathPtr) {
    qFatal("Could not find Mac application bundle path!");
  }
  QString rootPath = QString(pathPtr) % "/Contents/Resources/";
  rootDataName = rootPath % rootDataName;
  QByteArray rootPathArray = QString(rootPath % "proj4").toLatin1();
  printf("proj root '%s'\n", rootPathArray.data());

  const char *path[] = { rootPathArray.data() };
  pj_set_searchpath(1, (const char **)&path);
  CFRelease(appUrlRef);
  CFRelease(macPath);  
 #endif

  QFile rootData(rootDataName);
  if (!rootData.exists()) {
    qFatal("Cannot find map root data '%s'", rootDataName.toLatin1().data());
  }
  QMap<QString, Map *> maps = readRootData(rootData);

  if (maps.size() == 0) {
    qFatal("No maps in root data file!");
  }
  Map *map = maps.values()[0];

  for (int layer = 0; layer < map->numLayers(); layer++) {
    QFile missing(map->missingTilesPath(layer));
    map->loadMissingTiles(layer, missing);
  }

  MapRenderer renderer(map);
  MainWindow *window = new MainWindow(map, &renderer);
  QPixmapCache::setCacheLimit(50000);

  window->show();
  return app.exec();
}
