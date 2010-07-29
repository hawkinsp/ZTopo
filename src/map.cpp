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

#include <QChar>
#include <QDebug>
#include <QSet>
#include <QStringBuilder>
#include <QStringRef>
#include <QTextStream>
#include <QVariant>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <iostream>
#include "consts.h"
#include "map.h"

uint qHash(const Tile& k)
{
  return qHash(k.x()) ^ qHash(k.y()) ^ qHash(k.level()) ^ qHash(k.layer());
}

Tile::Tile(int vx, int vy, int vlevel, int vlayer) 
   : fx(vx), fy(vy), flevel(vlevel), flayer(vlayer) 
{
} 

Tile::Tile(int l, const QString &quad)
  : fx(0), fy(0), flevel(quad.length()), flayer(l)
{
  for (int i = flevel; i > 0; i--) {
    int mask = 1 << (i - 1);
    QChar c = quad[flevel - i];
    switch (c.digitValue()) {
    case 0: break;
    case 1: fx |= mask; break;
    case 2: fy |= mask; break;
    case 3: fx |= mask; fy |= mask; break;
    default:
      abort();
    }
  }
}

Tile::Tile(int layer, qkey q)
  : flayer(layer)
{
  fx = 0;
  fy = 0;
  flevel = 0;
  while (q > 1) {
    fx <<= 1;
    fy <<= 1;
    fx |= q & 1;
    fy |= (q & 2) >> 1;
    flevel++;
    q >>= 2;
   }
}


QString Tile::toQuadKeyString() const
{
  QString quad;
  for (int i = flevel; i > 0; i--) {
    char digit = '0';
    int mask = 1 << (i - 1);
    if (fx & mask) {
      digit++;
    }
    if (fy & mask) {
      digit += 2;
    }
    quad.append(digit);
  }
  return quad;
}


qkey Tile::toQuadKey() const
{
  qkey quad = 1;
  for (int i = 0; i < flevel; i++) {
    int mask = 1 << i;
    quad <<= 2;
    if (fx & mask) {
      quad |= 1;
    }
    if (fy & mask) {
      quad |= 2;
    }
  }
  return quad;
}

Layer::Layer(QString i, QString n, int z, int s) 
  : fId(i), fName(n), fMaxLevel(z), fScale(s)
{
}

static const QString layerIdField("id");
static const QString layerNameField("name");
static const QString layerMaxLevelField("maxLevel");
static const QString layerScaleField("scale");
static const QString layerStepField("indexLevelStep");

Layer::Layer(const QVariant &v)
{
  QVariantMap m(v.toMap());
  if (!m.contains(layerIdField) || !m.contains(layerNameField) ||
      !m.contains(layerMaxLevelField) || !m.contains(layerScaleField) ||
      !m.contains(layerStepField)) {
    qFatal("Label::Label(QVariant) - missing fields");
  }
  fId = m[layerIdField].toString();
  fName = m[layerNameField].toString();
  fMaxLevel = m[layerMaxLevelField].toInt();
  fScale = m[layerScaleField].toInt();
  fLevelStep = m[layerStepField].toInt();
}

int log2_int(int x) 
{
  int logx = 0;
  while (x > 0)
  {
    x >>= 1;
    logx++;
  }
  return logx;
}

Map::Map(const QString &aId, const QString &aName, const QUrl &aBaseUrl, Datum d, 
         Projection *pj, const QRect &aMapArea, QSizeF aPixelSize, 
         QVector<Layer> &aLayers)
  : sId(aId), sName(aName), fBaseUrl(aBaseUrl), dDatum(d), pjProj(pj), mapArea(aMapArea),
    pixelSize(aPixelSize), layers(aLayers)
{

  tProjToMap = QTransform();
  tProjToMap.scale(1.0 / pixelSize.width(), -1.0 / pixelSize.height());
  tProjToMap.translate(-mapArea.left(), -mapArea.top());

  bool invertible;
  tMapToProj = tProjToMap.inverted(&invertible);
  assert(tProjToMap.type() <= QTransform::TxScale && invertible);

  QPointF projOrigin = mapToProj().map(QPointF(0.0, 0.0));

  QRectF projArea = QRectF(projOrigin, QSizeF(mapArea.width(), -mapArea.height())).normalized();
  geoBounds = 
    Geographic::getProjection(d)->transformFrom(pj, projArea)
    .boundingRect().normalized().toAlignedRect();
                                                          

  reqSize = projToMap().mapRect(QRect(QPoint(0,0), mapArea.size())).size();

  int size = std::max(reqSize.width(), reqSize.height());
  logSize = log2_int(size);;
  logBaseTileSz = 8;
  baseTileSz = 1 << logBaseTileSz;
  vMaxLevel = logSize - logBaseTileSz;

  if (maxLevel() * 2 + 1 + log2_int(numLayers()) > int(sizeof(qkey)) * 8) {
    qFatal("Cannot pack %d levels and %d layers in %d bytes", maxLevel(), numLayers(), int(sizeof(qkey)));
  }

  foreach (const Layer &l, layers) {
    if (l.maxLevel() > maxLevel()) {
      qFatal("Maximum zoom level of layer exceeds maximum zoom level of map");
    }
  }

}



QRect variantToQRect(const QVariant &v)
{
  QVariantMap m(v.toMap());
  int x = m["x"].toInt();
  int y = m["y"].toInt();
  int w = m["w"].toInt();
  int h = m["h"].toInt();
  return QRect(x, y, w, h);
}

QSizeF variantToQSizeF(const QVariant &v)
{
  QVariantMap m(v.toMap());
  qreal w = m["w"].toReal();
  qreal h = m["h"].toReal();
  return QSizeF(w, h);
}

Map *Map::fromVariant(const QVariant &v)
{
  QVariantMap m(v.toMap());
  QString id(m["id"].toString());
  QString name(m["name"].toString());
  QUrl baseUrl(m["baseURL"].toString());
  QString datumStr(m["datum"].toString());
  Datum datum = parseDatum(datumStr);
  Projection *pj = new Projection(m["projection"].toString());
  QSizeF pixelSize(variantToQSizeF(m["pixelSize"]));
  QRect mapArea(variantToQRect(m["mapArea"]));

  QVector<Layer> layers;
  foreach (QVariant v, m["layers"].toList()) {
    layers << v;
  }

  return new Map(id, name, baseUrl, datum, pj, mapArea, pixelSize, layers);
}

const QString &Map::id() const { 
  return sId;
}

QRect Map::geographicBounds() const
{
  return geoBounds;
}

QSizeF Map::mapPixelSize() const
{
  QSizeF s = mapToProj().mapRect(QRectF(0, 0, 1, 1)).size();
  return QSizeF(s.width(), -s.height());
}

int Map::bestLayerAtLevel(int level) const
{
  int i = 0;
  while (i < layers.size() && level > layers[i].maxLevel())
    i++;
  
  return (i >= layers.size()) ? layers.size() - 1 : i;
}


bool Map::layerById(QString id, int &layer) const
{
  for (int i = 0; i < layers.size(); i++) {
    if (layers[i].id() == id) {
      layer = i;
      return true;
    }
  }
  return false;
}

QPoint Map::mapToTile(QPoint m, int level) const
{
  int logSize = logTileSize(level);
  return QPoint(m.x() >> logSize, m.y() >> logSize);
}

int Map::baseTileSize() const
{
  return 1 << logBaseTileSz;
}

int Map::logBaseTileSize() const
{
  return logBaseTileSz;
}

int Map::logTileSize(int level) const
{
  return logBaseTileSz + (vMaxLevel - level);
}

int Map::tileSize(int level) const
{
  return 1 << logTileSize(level);
}




/*
qkey Map::quadKeyToQuadKeyInt(QString quad) const
{
  qkey q = 1;
  for (int i = quad.length() - 1; i >= 0; i--) {
    int c = quad[i].digitValue();
    assert((c & ~3) == 0);
    q = (q << 2) | c;
  }
  return q;
}
*/


QString Map::tilePath(Tile t) const
{
  QString quadKey = t.toQuadKeyString();
  QString path = layers[t.layer()].id() % "/";
  for (int i = 0; i < t.level(); i += tileDirectoryChunk) {
    QStringRef chunk(&quadKey, i, std::min(tileDirectoryChunk, t.level() - i));
    if (i > 0) {
      path.append("/" % chunk);
    } else {
      path.append(chunk);
    }
  }
  return path % "t.png";
}

QString Map::indexFile(int layerId, qkey q) const
{
  Tile t(layerId, q);
  return layer(layerId).id() % "/t" % t.toQuadKeyString();
}

QRect Map::mapRectToTileRect(QRect r, int level) const
{
  int logSize = logTileSize(level);
  int minTileX = std::max(0, r.left() >> logSize);
  int maxTileX = std::min(1 << level, (r.right() >> logSize) + 1);
  int minTileY = std::max(0, r.top() >> logSize);
  int maxTileY = std::min(1 << level, (r.bottom() >> logSize) + 1);
 
  return QRect(minTileX, minTileY, maxTileX - minTileX, maxTileY - minTileY);
}

QRect Map::tileToMapRect(Tile t) const
{
  int logSize = logTileSize(t.level());
  int size = 1 << logSize;
  return QRect(t.x() << logSize, t.y() << logSize, size, size);
}

QRect Map::rectAtLevel(QRect r, int fromLevel, int toLevel) const
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

int Map::zoomLevel(qreal scaleFactor) const
{
  qreal scale = std::max(std::min(scaleFactor, 1.0), epsilon);
  qreal r = maxLevel() + log2(scale);
  return std::max(1, int(ceil(r)));
}


bool Map::parentIndex(int layerId, qkey q, qkey &index, qkey &tile) const
{
  int level = log2_int(q) / 2;
  if (level == 0) return false;
  
  int step = layer(layerId).indexLevelStep();
  int idxLevel;
  if (level <= step)
    idxLevel = 0;
  else
    idxLevel = step;

  index = (q & ((1 << (idxLevel * 2)) - 1)) | (1 << (idxLevel * 2));
  tile = q >> (idxLevel * 2);
  
  return true;
}

int Map::indexNumLevels(int layerId, qkey q) const
{
  int level = log2_int(q) / 2;
  int step = layer(layerId).indexLevelStep();
  assert(level == 0 || level == step);
  if (level < step) 
    return step;
  else 
    return layer(layerId).maxLevel() - step;
}
