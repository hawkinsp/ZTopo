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

#ifndef MAP_H
#define MAP_H 1

#include <QIODevice>
#include <QPointF>
#include <QRect>
#include <QSizeF>
#include <QString>
#include <QTransform>
#include <QUrl>
#include <QVector>

#include "projection.h"

static const int tileDirectoryChunk = 3;

int log2_int(int x);


// x, y, level, layer tuple packed as an integer
// Layout:
// 00LL001YXYXYXYX
typedef u_int32_t qkey;

void quadKeyUnpack(int maxLevel, qkey key, qkey &q, int &layer);
qkey quadKeyPack(int maxLevel, qkey q, int layer);

// Tile coordinates
class Tile {
public:
  Tile() {}
  Tile(qkey q, int maxLevel);
  Tile(int layer, const QString &);
  Tile(int vx, int vy, int vlevel, int vlayer);

  // Convert a tile to a quad key; requires the maximum zoom level of the map
  qkey toQuadKey(int maxLayer) const;

  // Convert a tile to a quad key specific for a map layer
  qkey toLayerQuadKey() const;
  
  QString toQuadKeyString() const; // Return tile as a quad key string, no layer

  int x;
  int y;
  int level;
  int layer;

  bool operator== (const Tile& other) const {
    return x == other.x && y == other.y && level == other.level &&
      layer == other.layer; 
  }
  bool operator<  (const Tile& other) const {
    return (x < other.x) ||
      (x == other.x && y < other.y) ||
      (x == other.x && y == other.y && level < other.level) ||  
      (x == other.x && y == other.y && level == other.level && layer < other.layer);
  }
};

uint qHash(const Tile& k);


class Layer
{
 public:
  Layer() { }
  Layer(const QVariant &v);
  Layer(QString i, QString n, int z, int s);

  const QString &id() const { return fId; }
  const QString &name() const { return fName; }
  const int maxLevel() const { return fMaxLevel; }
  const int indexLevelStep() const { return fLevelStep; }
  const int scale() const { return fScale; }

 private:
  QString fId, fName; // Short and descriptive names
  int fMaxLevel;  // Maximum tile zoom level 
  int fLevelStep;  // Maximum tile zoom level 
  int fScale;     // Map scale, e.g. 1:24000
};

class Map {
public:
  Map(const QString &id, const QString &name, const QUrl &baseURL, Datum d, 
      Projection *pj, const QRect &mapArea, QSizeF pixelSize, QVector<Layer> &layers);

  static Map *fromVariant(const QVariant &);

  const QString &id() const;

  // Conversions from map to projection space
  const QTransform &mapToProj() const { return tMapToProj; }
  const QTransform &projToMap() const { return tProjToMap; }

  Datum datum() const { return dDatum; }
  Projection *projection() const { return pjProj; }

  // Bounds in geographic space
  QRect geographicBounds() const;


  // Size of a map pixel in projection units
  QSizeF mapPixelSize() const;

  // Find the tile containing a given map point at a given level
  QPoint mapToTile(QPoint m, int level) const;

  // Size of a tile in map coordinates at a given level
  int baseTileSize() const;
  int logBaseTileSize() const;
  int tileSize(int level) const;
  int logTileSize(int level) const;

  QSize requestedSize() const { return reqSize; }

  QUrl baseUrl() const { return fBaseUrl; }

  // Filename of a given tile
  QString tilePath(Tile t) const;
  QString indexFile(qkey q) const;

  // Filename of the missing tiles file
  //  QString missingTilesPath(int layer) const;
  // void loadMissingTiles(int layer, QIODevice &d);

  int minLevel() const { return 1; }
  int maxLevel() const { return vMaxLevel; }

  // Given a rectangle in map coordinates, produce the smallest rectangle of tiles 
  // that cover it at a given level
  QRect mapRectToTileRect(QRect r, int level) const;

  // Convert a tile to a map area
  QRect tileToMapRect(Tile t) const;

  // Map the tile rectangle r in level fromLevel to the smallest rectangle in 
  // toLevel that covers the same area
  QRect rectAtLevel(QRect r, int fromLevel, int toLevel) const;

  // Best level for viewing at a given scale factor
  int zoomLevel(qreal scale) const;

  // Best layer to display for a zoom level
  int bestLayerAtLevel(int level) const;

  int numLayers() const { return layers.size(); }
  const Layer &layer(int id) const { return layers[id]; }
  bool layerById(QString name, int &layer) const;

  //  Tile quadKeyToTile(int layer, QString quadKey) const;
  //  QString tileToQuadKey(Tile t) const;
  //  qkey quadKeyToQuadKeyInt(QString quadKey) const;
  //  qkey tileToQuadKeyInt(Tile t) const;
  //  Tile quadKeyIntToTile(int layer, unsigned int q) const;
private:

  QString sId, sName;
  QUrl fBaseUrl;

  Datum dDatum;
  // Projection of projection space
  Projection *pjProj;

  QRect mapArea; // Map area in projection space
  QSizeF pixelSize; // Size of map pixels in projection space

  // Transforms from projection space to map space and vice versa
  QTransform tProjToMap;
  QTransform tMapToProj;

  QRect geoBounds;

  QVector<Layer> layers;

  // Declared size of the map in pixels; the actual map will be
  // square and sized to the smallest power of 2 containing both axes of the
  // declared size.
  QSize reqSize; 

  int logSize; // Logarithm of the size of the map space in pixels

  // Size of a map tile in pixels; also the size of tiles in map space at the base
  // of the pyramid.
  int baseTileSz;  
  int logBaseTileSz;
  int vMaxLevel;     // Number of levels of the tile pyramid

};

#endif
