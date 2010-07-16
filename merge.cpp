#include <QImage>
#include <QPainter>
#include <QVector>
#include <cstdio>
#include <cstdlib>
#include "map.h"
#include "mapprojection.h"

using namespace std;

int main(int argc, char **argv)
{
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <series> <max level> <base tile key>\n", argv[0]);
    return -1;
  }
  QString layerName(argv[1]);
  int maxLevel = atoi(argv[2]);
  QString rootTileKey("");
  if (argc == 4) {
    rootTileKey = QString(argv[3]);
  }


  MapProjection mapProj;
  Map map(&mapProj);

  int layer;
  if (!map.layerByName(layerName, layer)) {
    fprintf(stderr, "Unknown layer %s\n", layerName.toLatin1().data());
    return -1;
  }
  Tile baseTile = map.quadKeyToTile(layer, rootTileKey);
  int minLevel = baseTile.level;

  if (maxLevel <= baseTile.layer || maxLevel > map.layer(layer).maxZoom) {
    fprintf(stderr, "Invalid maximum level %d\n", maxLevel);
    return -1;    
  }

  printf("Merging layer %s from (%d, %d)@%d to %d\n", 
         map.layer(layer).name.toLatin1().data(),
         baseTile.x, baseTile.y, baseTile.level, maxLevel);
         
  QRect baseRect(baseTile.x, baseTile.y, 1, 1);
  int tileSize = map.baseTileSize();
  
  for (int level = maxLevel - 1; level >= minLevel; level--) {
    printf("level %d\n", level);
    QRect tiles = map.rectAtLevel(baseRect, baseTile.level, level);
    
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
            QString tilePath = map.tilePath(from);
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
          indexImage.save(map.tilePath(to), "png");
        }
      }
    }

  }

  return 0;
}
