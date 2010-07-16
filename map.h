#ifndef MAP_H
#define MAP_H 1

#include <QPointF>
#include <QRect>
#include <QSizeF>
#include <QString>
#include <QVector>

class MapProjection;

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
  Layer(QString n, int z, int tc) : name(n), maxZoom(z), transparentColor(tc) { }

  QString name;
  int maxZoom;

  int transparentColor;
};

class Map {
public:
  Map(MapProjection *proj);

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

  int maxLevel() { return vMaxLevel; }

  // Given a rectangle in map coordinates, produce the smallest rectangle of tiles 
  // that cover it at a given level
  QRect mapRectToTileRect(QRect r, int level);

  // Convert a tile to a map area
  QRect tileToMapRect(Tile t);

  // Map the tile rectangle r in level fromLevel to the smallest rectangle in 
  // toLevel that covers the same area
  QRect rectAtLevel(QRect r, int fromLevel, int toLevel);

  MapProjection *projection() { return proj; }

  // Best level for viewing at a given scale factor
  int zoomLevel(float scale);

  // Best layer to display for a zoom level
  int bestLayerAtLevel(int level);

  const Layer &layer(int id) { return layers[id]; }
  bool layerByName(QString name, int &layer);

  Tile quadKeyToTile(int layer, QString quadKey);
private:
  QVector<Layer> layers;

  MapProjection *proj;

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

  QString tileToQuadKey(Tile t);
};

#endif
