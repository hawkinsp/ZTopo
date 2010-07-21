#include <QVariant>
#include <qjson/parser.h>
#include "rootdata.h"
#include "map.h"

QMap<QString, Map *> readRootData(QIODevice &d) {
  QJson::Parser parser;
  bool ok;
 
  d.open(QIODevice::ReadOnly);
  QVariantMap result = parser.parse(&d, &ok).toMap();
  if (!ok) {
    qFatal("An error occurred while parsing the root map index file");
  }
  d.close();

  QMap<QString, Map *> maps;
  foreach (QVariant v, result["maps"].toList()) {
    Map *map = Map::fromVariant(v);
    maps[map->id()] = map;
  }
  return maps;
}
