#ifndef MAP_H
#define MAP_H 1

#include <QPointF>
#include <QString>

class MapProjection;

static const int tileDirectoryChunk = 3;

class Map {
public:
  Map(MapProjection *proj);

  // Find the tile containing a given map point at a given level
  QPoint mapToTile(QPoint m, int level);

  // Size of a tile in map coordinates at a given level
  int tileSize(int level);
  int logTileSize(int level);

  QPoint requestedSize() { return reqSize; }

  // Filename of the empty tile
  QString emptyTile();

  // Filename of a given tile
  QString tilePath(QPoint t, int level);

  int maxLevel() { return vMaxLevel; }

  MapProjection *projection() { return proj; }
private:
  MapProjection *proj;

  // Declared size of the map in pixels; the actual map will be
  // square and sized to the smallest power of 2 containing both axes of the
  // declared size.
  QPoint reqSize; 

  int logSize; // Logarithm of the size of the map space in pixels

  // Size of a map tile in pixels; also the size of tiles in map space at the base
  // of the pyramid.
  int baseTileSize;  
  int logBaseTileSize;
  int vMaxLevel;     // Number of levels of the tile pyramid

  QString tileToQuadKey(QPoint t, int level);
  QPoint quadKeyToTile(QString quadKey);
};

#endif
