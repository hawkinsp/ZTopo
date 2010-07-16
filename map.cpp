#include <QChar>
#include <QSet>
#include <QStringBuilder>
#include <QStringRef>
#include <algorithm>
#include <iostream>
#include "consts.h"
#include "map.h"
#include "mapprojection.h"

uint qHash(const Tile& k)
{
  return qHash(k.x) ^ qHash(k.y) ^ qHash(k.level) ^ qHash(k.layer);
}

Map::Map(MapProjection *p)
  : proj(p), reqSize(p->mapSize())
{
  int size = std::max(reqSize.width(), reqSize.height());
  logSize = 0;
  while (size > 0)
  {
    size >>= 1;
    logSize++;
  }
  logBaseTileSz = 8;
  baseTileSz = 1 << logBaseTileSz;
  vMaxLevel = logSize - logBaseTileSz + 1;
  layers.append(Layer("24k", 13, 0));
  layers.append(Layer("100k", 10, 1));
  //  layers.append(Layer("250k", 9, 1));
  layers.append(Layer("base", 8, 2));
}

int Map::bestLayerAtLevel(int level)
{
  if (level > 10) return 0;
  return 1;
  //  if (level > 8) return 1;
  //  return 2;
}


QPoint Map::mapToTile(QPoint m, int level)
{
  int logSize = logTileSize(level);
  return QPoint(m.x() >> logSize, m.y() >> logSize);
}

int Map::baseTileSize()
{
  return 1 << logBaseTileSz;
}

int Map::logBaseTileSize()
{
  return logBaseTileSz;
}

int Map::logTileSize(int level) 
{
  return logBaseTileSz + (vMaxLevel - level);
}

int Map::tileSize(int level)
{
  return 1 << logTileSize(level);
}

QString Map::tileToQuadKey(Tile tile)
{
  QString quad;
  int x = tile.x, y = tile.y;
  for (int i = tile.level; i > 0; i--) {
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

Tile Map::quadKeyToTile(int layer, QString quad)
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
  return Tile(x, y, level, layer);
}


QString Map::tilePath(Tile t)
{
  QString quadKey = tileToQuadKey(t);
  QString path = "tiles/" % layers[t.layer].name % "/";
  for (int i = 0; i < t.level; i += tileDirectoryChunk) {
    QStringRef chunk(&quadKey, i, std::min(tileDirectoryChunk, t.level - i));
    if (i > 0) {
      path.append("/" % chunk);
    } else {
      path.append(chunk);
    }
  }
  return path % "t.png";
}

QRect Map::mapRectToTileRect(QRect r, int level)
{
  int logSize = logTileSize(level);
  int minTileX = std::max(0, r.left() >> logSize);
  int maxTileX = std::min(1 << level, (r.right() >> logSize) + 1);
  int minTileY = std::max(0, r.top() >> logSize);
  int maxTileY = std::min(1 << level, (r.bottom() >> logSize) + 1);
 
  return QRect(minTileX, minTileY, maxTileX - minTileX, maxTileY - minTileY);
}

QRect Map::tileToMapRect(int x, int y, int level)
{
  int logSize = logTileSize(level);
  int size = 1 << logSize;
  return QRect(x << logSize, y << logSize, size, size);
}

QRect Map::rectAtLevel(QRect r, int fromLevel, int toLevel)
{
  if (toLevel < fromLevel) {
    int shift = fromLevel - toLevel;
    int mask = (1 << shift) - 1;
    int x = r.x() >> shift;
    int y = r.y() >> shift;
    int w = r.width() >> shift;
    if (r.width() & mask) w++;
    int h = r.height() >> shift;
    if (r.height() & mask) h++;
    return QRect(x, y, w, h);

  }
  else if (toLevel == fromLevel) {
    return r;
  }
  else {
    int shift = toLevel - fromLevel;
    return QRect(r.x() << shift, r.y() << shift, r.width() << shift, 
                 r.height() << shift);
  }
}
