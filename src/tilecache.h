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

#ifndef TILECACHE_H 
#define TILECACHE_H 1

#include <stdint.h>
#include <time.h>
#include <boost/intrusive/list.hpp>
#include <QDir>
#include <QEvent>
#include <QMap>
#include <QHash>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QRect>
#include <QPair>
#include <QPixmap>
#include <QQueue>
#include <QThread>
#include <QVariant>
#include <QWaitCondition>
#include <db.h>
#include "map.h"

class QNetworkReply;


namespace Cache {
  using namespace boost::intrusive;

  // Cache database keys are 64-bit values; the top byte indicates the kind of
  // object (index or tile).
  typedef uint64_t Key;
  
  enum Kind {
    TileKind = 0,
    IndexKind = 1
  };


  class Cache;

  // Possible states of cache entries
  enum State {
    Disk,           // On disk, not in memory, nothing pending
    Loading,        // On disk, not yet in memory, disk read IO queued.
    DiskAndMemory,  // Present on disk and in memory, nothing pending
    // Present in memory but we gave up on or do not want to save to disk
    MemoryOnly,     
    Saving,         // Not yet on disk, in memory, disk write IO queued
    NetworkPending, // Not on disk, not in memory, waiting on a network request
    IndexPending,   // Waiting for index data
    Invalid         // Dummy invalid state
  };
  
  bool isInMemory(State state);
  
  // Cache object list hook
  typedef list_base_hook<link_mode<auto_unlink> > LRUBaseHook;
  
  // Cache entries
  class Entry : public LRUBaseHook {
  public:
    Entry(Key key);
    
    Key key;
    QPixmap *pixmap;       
    QByteArray indexData;
    unsigned int memSize; 
    unsigned int diskSize;
    
    State state;
    bool inUse;   // This tile is being viewed
  };
  typedef list< Entry, base_hook<LRUBaseHook>, constant_time_size<false> > 
    CacheList;


  // IO requests
  enum IORequestKind {
    LoadObject,
    SaveObject,
    DeleteObject,
    UpdateObjectMetadata,
    ClearCache,
    TerminateThread
  };

  struct IORequest {
    IORequest(IORequestKind k, Key t, QVariant d);
    
    IORequestKind kind;
    Key tile;
    
    QVariant data;
  };
  
  // Tile IO threads
  class IOThread : public QThread {
    Q_OBJECT;
    
  public:
    IOThread(Cache *s, QObject *parent = 0);
    
  protected:
    void run();
    
  private:
    Cache *cache;

    void writeMetadata(Key key, uint32_t size);
    
  signals:
    void objectSavedToDisk(Key key, bool success);
  };
  
  typedef list_base_hook<link_mode<auto_unlink> > BundleBaseHook;
  class NetworkRequestBundle : public QObject, public BundleBaseHook {
    Q_OBJECT;
  public:
    NetworkRequestBundle(Cache *cache, 
                         Map *map, qkey index, uint32_t offset, Key key, uint32_t len,
                         QObject *parent = 0);

    static bool lessThan (NetworkRequestBundle const *a, 
                          NetworkRequestBundle const *b);
    
    int layer() const { return fLayer; }
    int kind() const { return fKind; }
    uint32_t offset() const { return fOffset; }
    uint32_t length() const;

    int numRequests() const { return reqs.size(); }

    // Try to merge another bundle with this one. Returns true on success.
    bool mergeBundle(NetworkRequestBundle *bundle);
    void makeRequests(QNetworkAccessManager *);

  private:
    Cache *cache;
    Map *map;
    int fLayer;
    Kind fKind;
    qkey qidx; // Index object
    uint32_t fOffset;           // Base offset
    // Sequence of requested objects and their lengths   
    QList<QPair<Key, uint32_t> > reqs; 
    

    QNetworkReply *reply;

  private slots:
    void requestFinished();    
  };
  typedef list< NetworkRequestBundle, base_hook<BundleBaseHook>, 
    constant_time_size<false> > BundleList;




 
  class NewDataEvent : public QEvent {
  public:
    NewDataEvent(Key key, const QString &error);
    NewDataEvent(Key key, const QByteArray &data, const QByteArray &indexData, 
                 const QImage &tileData);

    Key key() const { return fKey; }
    const QByteArray &data() const { return fData; }
    const QByteArray &indexData() const { return fIndexData; }
    const QImage &tileData() const { return fTileData; }
    const QString &errorString() { return fError; }
  private:
    QString fError;
    Key fKey;
    QByteArray fData, fIndexData;
    QImage fTileData;
  };

// Tile cache
class Cache : public QObject {
  Q_OBJECT;
public:
  Cache(Map *map, QNetworkAccessManager &mgr, int maxMem, int maxDisk, 
        const QString &cachePath);
  ~Cache();

  int getMemCacheSize() { return maxMemCache; }
  int getDiskCacheSize() { return maxDiskCache; }

  void setCacheSizes(int memMb, int diskMb);

  void emptyDiskCache();


  // Find a tile if present in the cache; do nothing if the tile is not present.
  // Returns true if the tile was found; the pixmap will not be updated if the tile
  // is empty.
  bool getTile(const Tile& key, QPixmap &p) const; 

  // Request that the cache obtain a tile; marks the tile as in use. Does nothing if
  // the tile is already available and in use.
  // Returns true if all the requested tiles are present in memory
  bool requestTiles(const QList<Tile> & key);

  // Mark as unused all tiles outside the given map rectangles
  void pruneObjects(const QList<QRect> &rects);

  friend class IOThread;
  friend class NetworkRequestBundle;

  virtual bool event(QEvent *e);
  

signals:
  void tileLoaded();
  void ioError(const QString &msg);

private slots:
  void objectSavedToDisk(Key key, bool success);

private:
  Map *map;
  QDir cachePath;

  DB_ENV *dbEnv;
  DB *timestampDb, *objectDb;

  QNetworkAccessManager &manager;

  int maxMemCache;
  int maxDiskCache;

  // All currently loaded tiles. All tile entries must be non-NULL.
  QHash<Key, Entry *> cacheEntries;

  CacheList diskLRU;      // Tiles in state Disk
  qint64 diskLRUSize;    // Compressed size of tiles in state Disk
  void addToDiskLRU(Entry &e);
  void removeFromDiskLRU(Entry &e);
  void purgeDiskLRU();

  CacheList indexPending; // Objects in state IndexPending

  // Tiles in state DiskAndMemory or MemoryOnly which are not in use
  CacheList memLRU;      
  unsigned int memLRUSize;
  void addToMemLRU(Entry &e);
  void removeFromMemLRU(Entry &e);
  void purgeMemLRU();

  // Tiles in state DiskAndMemory or MemoryOnly which are in use.
  CacheList memInUse;   

  unsigned int diskCacheHits, diskCacheMisses, memCacheHits, memCacheMisses;
  unsigned int numNetworkBundles, numNetworkReqs, networkReqSize;


  // Everything below this point is accessed by tile IO threads
  // Public for thread class
  QMutex tileQueueMutex;
  QWaitCondition tileQueueCond;
  // If tile.level < 0, terminate the thread.
  // If bytearray is null, load a tile
  // If bytearray is non-null, save a tile.
  QQueue<IORequest> tileQueue;
  QList<IOThread *> ioThreads;
  void postIORequest(const IORequest &req);
  

  // Save the disk cache index
  void initializeCacheFromDatabase();


  void findTileRange(qkey q, Entry *idx, uint32_t &offset, uint32_t &len);

  // Request an object. Returns true if the object is present in memory right now.
  bool requestObject(const Key key);
  static void decompressObject(Key key, const QByteArray &compressed, QByteArray &indexData, QImage &tileData);
  bool loadObject(Entry *e, const QByteArray &indexData, const QImage &tileData);

  void maybeFetchIndexPendingTiles();
  void maybeAddNetworkRequest(Entry *e);

  // Network request that haven't yet been posted, awaiting coalescing


  void networkRequestFinished();
  int requestsInFlight;

  // Requests ordered by the bundle less than operator
  QList<NetworkRequestBundle *> networkRequests;

  // Requests in FIFO order
  BundleList networkRequestQueue;
  void startNetworkRequests();
};

}

#endif
