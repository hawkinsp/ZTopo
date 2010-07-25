/*
  ZTopo --- a viewer for topographic maps
  Copyright (C) 2010 Peter Hawkins
  
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <QApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QMetaType>
#include <QSettings>
#include <QStringBuilder>
#include "mainwindow.h"
#include "map.h"
#include "maprenderer.h"
#include "projection.h"
#include "rootdata.h"
#include "proj_api.h"
#include "tilecache.h"

#include <QFile>
#include <QUrl>

#ifdef Q_WS_MAC
#include <CoreFoundation/CoreFoundation.h>
#endif

int main(int argc, char **argv)
{
  qRegisterMetaType<Cache::Key>("Key");
  qRegisterMetaType<Tile>("Tile");
  qRegisterMetaType<qkey>("qkey");

  QCoreApplication::setOrganizationName("ZTopo");
  QCoreApplication::setApplicationName("ZTopo");

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

  QSettings settings;
  QString cachePath = settings.value("cachePath", 
                      QDesktopServices::storageLocation(QDesktopServices::CacheLocation)).toString();
  QDir::current().mkpath(cachePath);

  MapRenderer renderer(map, cachePath);
  MainWindow *window = new MainWindow(map, &renderer);
  //  QPixmapCache::setCacheLimit(50000);

  window->show();
  return app.exec();
}
