#include <QChar>
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
  return qHash(k.x) ^ qHash(k.y) ^ qHash(k.level) ^ qHash(k.layer);
}



Layer::Layer(QString i, QString n, int z, int s) 
  : id(i), name(n), maxLevel(z), scale(s)
{
}

static const QString layerIdField("id");
static const QString layerNameField("name");
static const QString layerMaxLevelField("maxLevel");
static const QString layerScaleField("scale");
Layer Layer::fromVariant(const QVariant &v)
{
  QVariantMap m(v.toMap());
  if (!m.contains(layerIdField) || !m.contains(layerNameField) ||
      !m.contains(layerMaxLevelField) || !m.contains(layerScaleField)) {
    qFatal("Label::fromVariant - missing fields");
  }
    return Layer(m[layerIdField].toString(), 
                 m[layerNameField].toString(), 
                 m[layerMaxLevelField].toInt(),
                 m[layerScaleField].toInt());

}

Map::Map(const QString &aId, const QString &aName, const QUrl &aBaseUrl, Datum d, 
         Projection *pj, const QRect &aMapArea, QSizeF aPixelSize, 
         QVector<Layer> &aLayers)
  : sId(aId), sName(aName), baseUrl(aBaseUrl), dDatum(d), pjProj(pj), mapArea(aMapArea),
    pixelSize(aPixelSize), layers(aLayers)
{

  tProjToMap = QTransform();
  tProjToMap.scale(1.0 / pixelSize.width(), -1.0 / pixelSize.height());
  tProjToMap.translate(-mapArea.left(), -mapArea.top());

  bool invertible;
  tMapToProj = tProjToMap.inverted(&invertible);
  assert(tProjToMap.type() <= QTransform::TxScale && invertible);

  QPointF projOrigin = mapToProj().map(QPointF(0.0, 0.0));
  printf("proj origin %f %f\n", projOrigin.x(), projOrigin.y());

  QRectF projArea = QRectF(projOrigin, QSizeF(mapArea.width(), -mapArea.height())).normalized();
  printf("proj area %f %f %f %f\n", projArea.left(), projArea.top(),
         projArea.right(), projArea.bottom());
  geoBounds = 
    Geographic::getProjection(d)->transformFrom(pj, projArea)
    .boundingRect().normalized().toAlignedRect();
                                                          

  reqSize = projToMap().mapRect(QRect(QPoint(0,0), mapArea.size())).size();

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
    layers << Layer::fromVariant(v);
  }

  return new Map(id, name, baseUrl, datum, pj, mapArea, pixelSize, layers);
}

const QString &Map::id() { 
  return sId;
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
  int i = 0;
  while (i < layers.size() && level > layers[i].maxLevel)
    i++;
  
  return (i >= layers.size()) ? layers.size() - 1 : i;
}


bool Map::layerById(QString id, int &layer)
{
  for (int i = 0; i < layers.size(); i++) {
    if (layers[i].id == id) {
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

unsigned int Map::tileToQuadKeyInt(Tile tile)
{
  unsigned int quad = 1;
  int x = tile.x, y = tile.y;
  for (int i = 0; i < tile.level; i++) {
    int mask = 1 << i;
    quad <<= 2;
    if (x & mask) {
      quad |= 1;
    }
    if (y & mask) {
      quad |= 2;
    }
  }
  return quad;
}

unsigned int Map::quadKeyToQuadKeyInt(QString quad)
{
  unsigned int q = 1;
  for (int i = quad.length() - 1; i >= 0; i--) {
    int c = quad[i].digitValue();
    assert((c & ~3) == 0);
    q = (q << 2) | c;
  }
  return q;
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


QString Map::missingTilesPath(int layer) {
  return baseUrl.toString() % "/" % layers[layer].id % "/missing.txtz";
}

void Map::loadMissingTiles(int layer, QIODevice &d)
{
  d.open(QIODevice::ReadOnly);
  QByteArray missingCompressed(d.readAll());
  d.close();
  if (missingCompressed.isEmpty()) {
    qWarning("Empty missing tile data for layer %s!\n", layers[layer].id.toLatin1().data());
    return;
  }

  QByteArray mData(qUncompress(missingCompressed));
  if (mData.isEmpty()) {
    qWarning("Bad missing tile data for layer %s!\n", layers[layer].id.toLatin1().data());
    return;
  }
  QTextStream mStream(mData);
  
  while (true) {
    QString l = mStream.readLine();
    if (l.isNull()) break;
    unsigned int q = quadKeyToQuadKeyInt(l);
    layers[layer].missingTiles.add(q);
  }
}

QString Map::tilePath(Tile t)
{
  QString quadKey = tileToQuadKey(t);
  QString path = baseUrl.toString() % "/" % layers[t.layer].id % "/";
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
