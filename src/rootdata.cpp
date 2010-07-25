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
