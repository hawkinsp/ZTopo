#include <QChar>
#include <QSet>
#include <QStringBuilder>
#include <QStringRef>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <iostream>
#include "consts.h"
#include "map.h"
#include "mapprojection.h"

uint qHash(const Tile& k)
{
  return qHash(k.x) ^ qHash(k.y) ^ qHash(k.level) ^ qHash(k.layer);
}

Map::Map(Datum d, Projection *pj, const QTransform &ptm, QSize mapSize)
  : dDatum(d), pjProj(pj)
{
  bool invertible;
  tProjToMap = ptm;
  tMapToProj = ptm.inverted(&invertible);
  assert(tProjToMap.type() <= QTransform::TxScale && invertible);

  QPointF projOrigin = mapToProj().map(QPointF(0.0, 0.0));
  printf("proj origin %f %f\n", projOrigin.x(), projOrigin.y());

  QRectF projArea = QRectF(projOrigin, QSizeF(mapSize.width(), -mapSize.height())).normalized();
  printf("proj area %f %f %f %f\n", projArea.left(), projArea.top(),
         projArea.right(), projArea.bottom());
  geoBounds = 
    Geographic::getProjection(d)->transformFrom(pj, projArea)
    .boundingRect().normalized().toAlignedRect();
                                                          

  reqSize = projToMap().mapRect(QRect(QPoint(0,0), mapSize)).size();

  int size = std::max(reqSize.width(), reqSize.height());
  logSize = 0;
  while (size > 0)
  {
    size >>= 1;
    logSize++;
  }
  logBaseTileSz = 8;
  baseTileSz = 1 << logBaseTileSz;
  vMaxLevel = logSize - logBaseTileSz;



  layers.append(Layer("100k", "1:100k Quad", 9));
  layers.append(Layer("24k", "1:24k Quad", 12));
  //  layers.append(Layer("250k", 9, 1));
  //layers.append(Layer("base", 8));
}

QRect Map::geographicBounds()
{
  return geoBounds;
}

QSizeF Map::mapPixelSize()
{
  QSizeF s = mapToProj().mapRect(QRectF(0, 0, 1, 1)).size();
  return QSizeF(s.width(), -s.height());
}

int Map::bestLayerAtLevel(int level)
{
  if (level > 9) return 1;
  return 0;
  //  if (level > 8) return 1;
  //  return 2;
}


bool Map::layerByName(QString name, int &layer)
{
  for (int i = 0; i < layers.size(); i++) {
    if (layers[i].name == name) {
      layer = i;
      return true;
    }
  }
  return false;
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
  int x = 0, y = 0, level = quad.length();
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

QRect Map::tileToMapRect(Tile t)
{
  int logSize = logTileSize(t.level);
  int size = 1 << logSize;
  return QRect(t.x << logSize, t.y << logSize, size, size);
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

int Map::zoomLevel(qreal scaleFactor)
{
  qreal scale = std::max(std::min(scaleFactor, 1.0), epsilon);
  qreal r = maxLevel() + log2(scale);
  return std::max(0, int(ceil(r)));
}
