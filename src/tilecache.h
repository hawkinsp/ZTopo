#ifndef TILECACHE_H 
#define TILECACHE_H 1

#include <boost/intrusive/list.hpp>
#include <QDir>
#include <QMap>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QRect>
#include <QPair>
#include <QPixmap>
#include <QQueue>
#include <QThread>
#include <QWaitCondition>
#include "map.h"

class QNetworkReply;
class TileCache;
class DbEnv;
class Db;

using namespace boost::intrusive;

// Tile cache
typedef list_base_hook<link_mode<auto_unlink> > LRUBaseHook;

enum TileState {
  Disk,           // On disk, not in memory, nothing pending
  Loading,        // On disk, not yet in memory, disk read IO queued.
  DiskAndMemory,  // Present on disk and in memory, nothing pending
  MemoryOnly,     // Present in memory but we gave up trying to save to disk
  Saving,         // Not yet on disk, in memory, disk write IO queued
  NetworkPending, // Not on disk, not in memory, waiting on a network request
  Invalid         // Dummy invalid state
};

// Tile entry in memory
class TileEntry : public LRUBaseHook {
public:
  TileEntry() { }
  TileEntry(const Tile &key);
   
  Tile key;
  QPixmap *pixmap;       // NULL if data in memory
  unsigned int memSize; 
  unsigned int diskSize;

  TileState state;
  bool inUse;   // This tile is being viewed
};

typedef list< TileEntry, base_hook<LRUBaseHook>, constant_time_size<false> > 
  TileList;


// Tile IO threads
/*
class MapTileIOThread : public QThread {
  Q_OBJECT;

public:
  MapTileIOThread(TileCache *s, QDir cachePath, QObject *parent = 0);

protected:
  void run();

private:
  TileCache *view;
  QDir cachePath;

signals:
  void tileLoadedFromDisk(Tile tile, QImage img);
  void tileSavedToDisk(Tile tile, bool success);
};
*/

// Tile cache
class TileCache : public QObject {
  Q_OBJECT;
public:
  TileCache(Map *);
  ~TileCache();

  // Find a tile if present in the cache; do nothing if the tile is not present.
  // Returns true if the tile was found; the pixmap will not be updated if the tile
  // is empty.
  bool getTile(const Tile& key, QPixmap &p) const; 

  // Request that the cache obtain a tile; marks the tile as in use. Does nothing if
  // the tile is already available and in use.
  void requestTile(const Tile& key);

  // Mark as unused all tiles outside the given map rectangles
  void pruneTiles(const QList<QRect> &rects);

  friend class MapTileIOThread;

signals:
  void tileLoaded(Tile key);

private slots:
  void networkReplyFinished(QNetworkReply *);
  void tileLoadedFromDisk(Tile key, QImage img);
  void tileSavedToDisk(Tile tile, bool success);

private:
  Map *map;
  QDir cachePath;
  QString indexFile;

  DbEnv *dbEnv;
  QVector<Db *> layerDbs;
  char *dbBuffer;

  QNetworkAccessManager manager;

  // All currently loaded tiles.
  // Tiles are present but have value NULL if they are queued to be loaded from disk.
  QMap<Tile, TileEntry *> tileMap;

  TileList diskLRU;      // Tiles in state Disk
  qint64 diskLRUSize;    // Compressed size of tiles in state Disk
  void addToDiskLRU(TileEntry &e);
  void removeFromDiskLRU(TileEntry &e);
  void purgeDiskLRU();

  TileList loadingList; // Tiles in state Loading

  // Tiles in state DiskAndMemory or MemoryOnly which are not in use
  TileList memLRU;      
  unsigned int memLRUSize;
  void addToMemLRU(TileEntry &e);
  void removeFromMemLRU(TileEntry &e);
  void purgeMemLRU();

  // Tiles in state DiskAndMemory or MemoryOnly which are in use.
  TileList memInUse;   

  int diskCacheHits, diskCacheMisses, memCacheHits, memCacheMisses;

  // Everything below this point is accessed by tile IO threads
  // Public for thread class
  /*  QMutex tileQueueMutex;
  QWaitCondition tileQueueCond;
  // If tile.level < 0, terminate the thread.
  // If bytearray is null, load a tile
  // If bytearray is non-null, save a tile.
  QQueue<QPair<Tile, QByteArray> > tileQueue;
  QList<MapTileIOThread *> ioThreads;
  */

  // Save the disk cache index
  void saveDiskCacheIndex();


};

#endif
