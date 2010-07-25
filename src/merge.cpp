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

#include <QFile>
#include <QImage>
#include <QPainter>
#include <QVector>
#include <cstdio>
#include <cstdlib>
#include "map.h"
#include "rootdata.h"

using namespace std;

int main(int argc, char **argv)
{
  if (argc < 5) {
    fprintf(stderr, "Usage: %s <maps.json> <map> <series> <max level> <base tile key>\n", argv[0]);
    return -1;
  }
  QFile rootFile(argv[1]);
  QString mapId(argv[2]);
  QString layerName(argv[3]);
  int maxLevel = atoi(argv[4]);
  QString rootTileKey("");
  if (argc == 5) {
    rootTileKey = QString(argv[5]);
  }


  QMap<QString, Map *> maps = readRootData(rootFile);
  Map *map = maps[mapId];


  int layer;
  if (!map->layerById(layerName, layer)) {
    fprintf(stderr, "Unknown layer %s\n", layerName.toLatin1().data());
    return -1;
  }
  Tile baseTile(layer, rootTileKey);
  int minLevel = baseTile.level;

  if (maxLevel <= baseTile.layer || maxLevel > map->layer(layer).maxLevel()) {
    fprintf(stderr, "Invalid maximum level %d\n", maxLevel);
    return -1;    
  }

  printf("Merging layer %s from (%d, %d)@%d to %d\n", 
         map->layer(layer).name().toLatin1().data(),
         baseTile.x, baseTile.y, baseTile.level, maxLevel);
         
  QRect baseRect(baseTile.x, baseTile.y, 1, 1);
  int tileSize = map->baseTileSize();
  
  for (int level = maxLevel - 1; level >= minLevel; level--) {
    printf("level %d\n", level);
    QRect tiles = map->rectAtLevel(baseRect, baseTile.level, level);
    
    for (int y = tiles.top(); y <= tiles.bottom(); y++) {
      for (int x = tiles.left(); x <= tiles.right(); x++) {
        Tile to(x, y, level, layer);
        QImage image = QImage(256, 256, QImage::Format_RGB32);
        image.fill(QColor(255, 255, 255).rgb());
        QPainter p;
        bool isNull = true;
        QVector<QRgb> colorTable;
        p.begin(&image);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        for (int dy = 0; dy <= 1; dy++) {
          for (int dx = 0; dx <= 1; dx++) {
            Tile from(x * 2 + dx, y * 2 + dy, level + 1, layer);
            QString tilePath = map->tilePath(from);
            QImage tile(tilePath);
            if (!tile.isNull()) {
              isNull = false;
              QRect target(dx * tileSize / 2, dy * tileSize / 2, 
                           tileSize / 2, tileSize / 2);
              p.drawImage(target, tile);
              colorTable = tile.colorTable();
            }
          }
        }
        p.end();
        if (!isNull) {
          QImage indexImage = image.convertToFormat(QImage::Format_Indexed8, 
                                                    colorTable, Qt::ThresholdDither);
          indexImage.save(map->tilePath(to), "png");
        }
      }
    }

  }

  return 0;
}
