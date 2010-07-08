#include <QChar>
#include <QStringBuilder>
#include <QStringRef>
#include <algorithm>
#include <iostream>
#include "map.h"
#include "mapprojection.h"


Map::Map(MapProjection *p)
  : proj(p), reqSize(p->mapSize())
{
  int size = std::max(reqSize.x(), reqSize.y());
  logSize = 0;
  while (size > 0)
  {
    size >>= 1;
    logSize++;
  }
  logBaseTileSize = 8;
  baseTileSize = 1 << logBaseTileSize;
  vMaxLevel = logSize - logBaseTileSize + 1;
}


QPoint Map::mapToTile(QPoint m, int level)
{
  int logSize = logTileSize(level);
  return QPoint(m.x() >> logSize, m.y() >> logSize);
}

int Map::logTileSize(int level) 
{
  return logBaseTileSize + (vMaxLevel - level);
}

int Map::tileSize(int level)
{
  return 1 << logTileSize(level);
}

QString Map::emptyTile()
{
  return "empty.png";
}

QString Map::tileToQuadKey(QPoint tile, int level)
{
  QString quad;
  int x = tile.x(), y = tile.y();
  for (int i = level; i > 0; i--) {
    char digit = '0';
    int mask = 1 << (i - 1);
    if (x & mask) {
      digit++;
    }
    if (y & mask) {
      digit += 2;
    }
    quad.append(digit);
  }
  return quad;
}

QPoint Map::quadKeyToTile(QString quad)
{
  int x = 0, y = 0, level = quad.length() - 1;
  for (int i = level; i > 0; i--) {
    int mask = 1 << (i - 1);
    QChar c = quad[level - i];
    switch (c.digitValue()) {
    case 0: break;
    case 1: x |= mask; break;
    case 2: y |= mask; break;
    case 3: x |= mask; y |= mask; break;
    default:
      abort();
    }
  }
  return QPoint(x, y);
}


QString Map::tilePath(QPoint t, int level)
{
  QString quadKey = tileToQuadKey(t, level);
  QString path = "tiles/24k/";
  for (int i = 0; i < level; i += tileDirectoryChunk) {
    QStringRef chunk(&quadKey, i, std::min(tileDirectoryChunk, level - i));
    if (i > 0) {
      path.append("/" % chunk);
    } else {
      path.append(chunk);
    }
  }
  return path % "t.png";
}
