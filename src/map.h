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

#include "prefix.h"
#include "projection.h"

static const int tileDirectoryChunk = 3;

// Tile coordinates
class Tile {
public:
  Tile() {}
 Tile(int vx, int vy, int vlevel, int vlayer) 
   : x(vx), y(vy), level(vlevel), layer(vlayer) { } 

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
  Layer(QString i, QString n, int z, int s);

  QString id, name;
  int maxLevel;
  int scale;

  PrefixTree missingTiles;

  static Layer fromVariant(const QVariant &v);
};

class Map {
public:
  Map(const QString &id, const QString &name, const QUrl &baseURL, Datum d, 
      Projection *pj, const QRect &mapArea, QSizeF pixelSize, QVector<Layer> &layers);

  static Map *fromVariant(const QVariant &);

  const QString &id();

  // Conversions from map to projection space
  const QTransform &mapToProj() { return tMapToProj; }
  const QTransform &projToMap() { return tProjToMap; }

  Datum datum() { return dDatum; }
  Projection *projection() { return pjProj; }

  // Bounds in geographic space
  QRect geographicBounds();


  // Size of a map pixel in projection units
  QSizeF mapPixelSize();

  // Find the tile containing a given map point at a given level
  QPoint mapToTile(QPoint m, int level);

  // Size of a tile in map coordinates at a given level
  int baseTileSize();
  int logBaseTileSize();
  int tileSize(int level);
  int logTileSize(int level);

  QSize requestedSize() { return reqSize; }

  // Filename of a given tile
  QString tilePath(Tile t);

  // Filename of the missing tiles file
  QString missingTilesPath(int layer);
  void loadMissingTiles(int layer, QIODevice &d);

  int maxLevel() { return vMaxLevel; }

  // Given a rectangle in map coordinates, produce the smallest rectangle of tiles 
  // that cover it at a given level
  QRect mapRectToTileRect(QRect r, int level);

  // Convert a tile to a map area
  QRect tileToMapRect(Tile t);

  // Map the tile rectangle r in level fromLevel to the smallest rectangle in 
  // toLevel that covers the same area
  QRect rectAtLevel(QRect r, int fromLevel, int toLevel);

  // Best level for viewing at a given scale factor
  int zoomLevel(qreal scale);

  // Best layer to display for a zoom level
  int bestLayerAtLevel(int level);

  int numLayers() { return layers.size(); }
  const Layer &layer(int id) { return layers[id]; }
  bool layerById(QString name, int &layer);

  Tile quadKeyToTile(int layer, QString quadKey);
  QString tileToQuadKey(Tile t);
  unsigned int quadKeyToQuadKeyInt(QString quadKey);
  unsigned int tileToQuadKeyInt(Tile t);
private:

  QString sId, sName;
  QUrl baseUrl;

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
