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

#include <stdint.h>
#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringBuilder>
#include <db.h>
#include "tilecache.h"
#include "consts.h"

using namespace boost::intrusive;

static const int numWorkerThreads = 1;
static const int maxThreadWaitTime = 1000; // Maximum time to wait for a thread to die

static const int maxBufferSize = 500000;

// Maximum number of network requests in flight simultaneously
static const int maxNetworkRequestsInFlight = 6;

namespace Cache {
  typedef QPair<Key, uint32_t> NetworkReqKey;

  static const int kindShift = 56;
  static const int layerShift = 48;
  Kind keyKind(Key k) {
    return Kind(k >> kindShift);
  }
  int keyLayer(Key k) { 
    return int(k >> layerShift) & 0xFF;
  }
  qkey keyQuad(Key k) {
    return qkey(k & ((Key(1) << kindShift) - 1));
  }
  Key tileKey(int layer, qkey k) {
    return (Key(TileKind) << kindShift) | Key(layer) << layerShift | k;
  }
  Key indexKey(int layer, qkey k) {
    return (Key(IndexKind) << kindShift) | Key(layer) << layerShift | k;
  }

  bool isInMemory(State s)
  {
    return (s == DiskAndMemory || s == MemoryOnly || s == Saving);
  }
  
  Entry::Entry(Key aKey) 
    : key(aKey), pixmap(NULL), indexData(NULL), memSize(0), diskSize(0),
      state(Invalid), inUse(false)
  {
  }
  
  IORequest::IORequest(IORequestKind k, Key t, QVariant d)
    : kind(k), tile(t), data(d)
  {
  }


  NetworkRequestBundle::NetworkRequestBundle(Cache *c, Map *m, qkey index, 
                         uint32_t off, Key key, uint32_t len, QObject *parent)
    : QObject(parent), cache(c), map(m), qidx(index), fOffset(off)
  {
    fLayer = keyLayer(key);
    fKind = keyKind(key);
    reqs << NetworkReqKey(key, len);
  }
  
  bool NetworkRequestBundle::lessThan(NetworkRequestBundle const *a,
                                      NetworkRequestBundle const *b)
  {
    return (a->qidx < b->qidx) ||
      (a->qidx == b->qidx && a->fLayer < b->fLayer) ||
      (a->qidx == b->qidx && a->fLayer == b->fLayer && a->fKind < b->fKind) ||
      (a->qidx == b->qidx && a->fLayer == b->fLayer && a->fKind == b->fKind &&
       a->fOffset < b->fOffset);
  }

  uint32_t NetworkRequestBundle::length() const
  {
    uint32_t len = 0;
    foreach (const NetworkReqKey &req, reqs) {
      len += req.second;
    }
    return len;
  }

  bool NetworkRequestBundle::mergeBundle(NetworkRequestBundle *other)
  {
    assert(other != NULL);
    if (qidx != other->qidx || fLayer != other->fLayer || fKind != other->fKind)
      return false;
    
    if (offset() + length() == other->offset()) {
      reqs.append(other->reqs);
      return true;
    } else if (other->offset() + other->length() == offset()) {
      fOffset = other->offset();
      reqs = other->reqs + reqs;
      return true;
    }

    return false;
  }

  void NetworkRequestBundle::makeRequests(QNetworkAccessManager *manager)
  {
    QString baseUrl(map->baseUrl().toString());
    QString url = baseUrl % "/" % map->indexFile(fLayer, qidx) % 
      ((fKind == IndexKind) ? ".idxz" : ".dat");
    QNetworkRequest req;
    req.setUrl(QUrl(url));
    req.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    req.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);

    // qDebug() << "req " << url << " off " << n.offset << " len "<< offset - n.offset << " coalesce " << bundle->size();
    if (length() >= 0) {
      QString rangeHdr = "bytes=" % QString::number(uint(fOffset)) % "-" %
        QString::number(uint(fOffset  + length() - 1));
      req.setRawHeader(QByteArray("Range"), rangeHdr.toLatin1());
    }
    
    reply = manager->get(req);
    if (reply->error()) {
      requestFinished();
    } else {
      connect(reply, SIGNAL(finished()), this, SLOT(requestFinished()));
    }
  }

  void NetworkRequestBundle::requestFinished()
  {
    QByteArray data;
    bool ok;
    ok = (reply->error() == QNetworkReply::NoError);
    if (ok) {
      data = reply->readAll();
      ok = !data.isEmpty();
    }

    int pos = 0;
    foreach (const NetworkReqKey &r, reqs) {
      Key key = r.first;
      
      assert(r.second != 0 || reqs.size() == 1);
      int len = (r.second == 0) ? data.size() : r.second;

      QByteArray subData;
      QByteArray indexData;
      QImage tileData;

      ok = ok && (pos + len <= data.size());
      if (ok) {
        subData = QByteArray(data.constData() + pos, len);
        Cache::decompressObject(key, subData, indexData, tileData);
        NewDataEvent *ev = new NewDataEvent(key, subData, indexData, tileData);
        QCoreApplication::postEvent(cache, ev, Qt::LowEventPriority);
      } else {
        NewDataEvent *ev = new NewDataEvent(key, reply->errorString());
        QCoreApplication::postEvent(cache, ev);
      }

      cache->networkRequestFinished();
      pos += len;
    }
    reply->deleteLater();
    deleteLater();
  }




  IOThread::IOThread(Cache *v, QObject *parent)
    : QThread(parent), cache(v)
  {
  }

  void IOThread::run()
  {
    forever {
      cache->tileQueueMutex.lock();
      while (cache->tileQueue.isEmpty()) {
        cache->tileQueueCond.wait(&cache->tileQueueMutex);
      }
      IORequest req = cache->tileQueue.dequeue();
      cache->tileQueueMutex.unlock();
      
      
      switch (req.kind) {
      case LoadObject: {
        QByteArray data;
        QByteArray indexData;
        QImage tileData;
        if (cache->objectDb) {
          // Do one Get to retrieve the object size, then another to retrieve the object
          DBT dbKey, dbData;
          memset(&dbKey, 0, sizeof(DBT));
          memset(&dbData, 0, sizeof(DBT));
          dbKey.data = &req.tile;
          dbKey.size = sizeof(Key);
          dbData.data = NULL;
          dbData.ulen = 0;
          dbData.flags = DB_DBT_USERMEM;
          cache->objectDb->get(cache->objectDb, NULL, &dbKey, &dbData, 0);

          data.resize(dbData.size);
          dbData.data = data.data();
          dbData.ulen = dbData.size;
          dbData.flags = DB_DBT_USERMEM;
          int ret = cache->objectDb->get(cache->objectDb, NULL, &dbKey, &dbData, 0);
          if (ret != 0) {
            QString msg = tr("Error loading cached object %1").arg(req.tile);
            qWarning() << msg;
            data.clear();
          }
          else {
            writeMetadata(req.tile, data.size());
            Cache::decompressObject(req.tile, data, indexData, tileData);
          }
        }
        //        emit(objectLoadedFromDisk(req.tile, indexData, tileData));
        QCoreApplication::postEvent(cache, new NewDataEvent(req.tile, data, 
                                                            indexData, tileData),
                                    Qt::LowEventPriority);
        break;
      }
        
      case SaveObject: {
        //        qDebug() << "saving " << req.tile;
        QByteArray data = req.data.value<QByteArray>();
        DBT dbKey, dbData;
        memset(&dbKey, 0, sizeof(DBT));
        memset(&dbData, 0, sizeof(DBT));
        dbKey.data = &req.tile;
        dbKey.size = sizeof(Key);
        dbData.data = (void *)data.constData();
        dbData.size = data.size();
        int ret = -1;
        if (cache->objectDb) {
          ret = cache->objectDb->put(cache->objectDb, NULL, &dbKey, &dbData, 0);
          if (ret != 0) {
            QString msg = tr("Cache DB put failed with return code %1")
              .arg(ret);
            qWarning() << msg;
          } else {
            writeMetadata(req.tile, data.size());
          }
        }
        emit(objectSavedToDisk(req.tile, ret == 0));
        break;
      }
        
      case DeleteObject: {
        // qDebug() << "deleting " << req.tile;
        if (cache->objectDb && cache->timestampDb) {
          DBT dbKey;
          memset(&dbKey, 0, sizeof(DBT));
          dbKey.data = &req.tile;
          dbKey.size = sizeof(Key);

          int ret = cache->objectDb->del(cache->objectDb, NULL, &dbKey, 0);
          if (ret != 0) {
            QString msg = tr("Cache DB delete of %1 failed with %2").arg(req.tile)
              .arg(ret);
            qWarning() << msg;
          }
          ret = cache->timestampDb->del(cache->timestampDb, NULL, &dbKey, 0);
          if (ret != 0) {
            QString msg = tr("Cache timestamp DB delete of %1 failed with %2")
              .arg(req.tile).arg(ret);
            qWarning() << msg;
          }

        }      
        break;
      }
        
      case UpdateObjectMetadata: {
        writeMetadata(req.tile, req.data.toUInt());
        break;
      }

      case ClearCache: {
        uint32_t count;
        if (cache->objectDb) {
          cache->objectDb->truncate(cache->objectDb, NULL, &count, 0);
          cache->objectDb->compact(cache->objectDb, NULL, NULL, NULL, NULL, DB_FREE_SPACE, NULL);
        }
        if (cache->timestampDb) {
          cache->timestampDb->truncate(cache->timestampDb, NULL, &count, 0);
          cache->timestampDb->compact(cache->timestampDb, NULL, NULL, NULL, NULL, DB_FREE_SPACE, NULL);
        }
        break;
      }
        
      case TerminateThread:
        return;  // Thread is done
        
      default:
        qFatal("Unknown IO request type"); // Unknown IO request type
      }
    }
  }

  void IOThread::writeMetadata(Key key, uint32_t size)
  {
    uint32_t timeSize[2] = { time(NULL), size };
    DBT dbKey, dbData;
    memset(&dbKey, 0, sizeof(DBT));
    memset(&dbData, 0, sizeof(DBT));
    dbKey.data = &key;
    dbKey.size = sizeof(Key);
    dbData.data = &timeSize;
    dbData.size = sizeof(timeSize);

    int ret = -1;
    if (cache->timestampDb) {
      ret = cache->timestampDb->put(cache->timestampDb, NULL, &dbKey, &dbData, 0);
      if (ret != 0) {
        QString msg = tr("Timestamp put failed with return code %1").arg(ret);
        qWarning() << msg;
      }
    }
  }
  
  
  Cache::Cache(Map *m, QNetworkAccessManager &mgr, int maxMem, int maxDisk, 
               const QString &cp)
    : map(m), cachePath(cp), dbEnv(NULL), timestampDb(NULL),
      objectDb(NULL),
      manager(mgr), maxMemCache(maxMem), 
      maxDiskCache(maxDisk),  diskLRUSize(0), memLRUSize(0),
      diskCacheHits(0), diskCacheMisses(0), memCacheHits(0), memCacheMisses(0), 
    numNetworkBundles(0), numNetworkReqs(0), networkReqSize(0), requestsInFlight(0)
  {
    do {
      uint32_t dbFlags = DB_CREATE;
      QString objectDbName = map->id() % ".db";
      QString timestampDbName = map->id() % "-timestamp.db";
      
      uint32_t envFlags = DB_CREATE | DB_INIT_MPOOL;

      int ret = db_env_create(&dbEnv, 0);
      if (ret != 0) goto dberror;
      ret = dbEnv->open(dbEnv, cachePath.path().toLatin1().data(), envFlags, 0);
      if (ret != 0) goto dberror;
      ret = db_create(&objectDb, dbEnv, 0);
      if (ret != 0) goto dberror;
      ret = db_create(&timestampDb, dbEnv, 0);
      if (ret != 0) goto dberror;
      ret = objectDb->open(objectDb, NULL, objectDbName.toLatin1().data(), NULL,
                           DB_BTREE, dbFlags, 0);
      if (ret != 0) goto dberror;
      ret = timestampDb->open(timestampDb, NULL, timestampDbName.toLatin1().data(),
                              NULL, DB_BTREE, dbFlags, 0);
      if (ret != 0) goto dberror;
      
      initializeCacheFromDatabase();
      break;

    dberror:

      if (timestampDb) { timestampDb->close(timestampDb, 0); timestampDb = NULL; }
      if (objectDb) { objectDb->close(objectDb, 0); objectDb = NULL; }
      if (dbEnv) { dbEnv->close(dbEnv, 0); dbEnv = NULL; }
      qWarning("Database exception opening tile cache environment %s", 
               cachePath.path().toLatin1().data());
    } while (false);
    

    
    for (int i = 0; i < numWorkerThreads; i++) {
      IOThread *ioThread = new IOThread(this, this);
      ioThreads << ioThread;
      //      connect(ioThread, SIGNAL(objectLoadedFromDisk(Key, QByteArray, QImage)),
      //              this, SLOT(objectLoadedFromDisk(Key, QByteArray, QImage)));
      connect(ioThread, SIGNAL(objectSavedToDisk(Key, bool)),
              this, SLOT(objectSavedToDisk(Key, bool)));
      
      ioThread->start();
    }
  }
  
  Cache::~Cache()
  {
    // Terminate worker threads
    tileQueueMutex.lock();
    for (int i = 0; i < ioThreads.size(); i++) {
      tileQueue.enqueue(IORequest(TerminateThread, 0, QVariant()));
    }
    tileQueueCond.wakeAll();
    tileQueueMutex.unlock();
    foreach (IOThread *t, ioThreads) {
      t->wait(maxThreadWaitTime);
      delete t;
    }
    
    // Report cache statistics
    qDebug() << "Memory cache hits: " << memCacheHits << " misses: " << memCacheMisses
             << " (" << 
      qreal(memCacheHits * 100.0) / qreal(memCacheHits + memCacheMisses) << "%)";
    qDebug() << "Disk cache hits: " << diskCacheHits << " misses: " << diskCacheMisses
             << " (" << 
      qreal(diskCacheHits * 100.0) / qreal(diskCacheHits + diskCacheMisses) << "%)";
    qDebug() << "Network requests: " << numNetworkReqs << " size: " << networkReqSize 
             << " (" << qreal(networkReqSize) / qreal(numNetworkReqs) << " bytes per request)";
    qDebug() << "Network bundles: " << numNetworkBundles << "; " << 
      qreal(numNetworkReqs) / qreal(numNetworkBundles) << " reqs per bundle (" 
             << qreal(networkReqSize) / qreal(numNetworkBundles) 
             << " bytes per bundle)";
    
    // Clear the cache
    foreach (Entry *t, cacheEntries) {
      delete t;
    }
    cacheEntries.clear();
    
    // Close databases
    if (objectDb)    { objectDb->close(objectDb, 0); }
    if (timestampDb) { timestampDb->close(timestampDb, 0); }
    if (dbEnv)       { dbEnv->close(dbEnv, 0); }
  }
  
  void Cache::setCacheSizes(int mem, int disk) {
    maxMemCache = mem;
    maxDiskCache = disk;
    purgeMemLRU();
  }
  
  typedef QPair<uint32_t, Key> EntryTime;
  void Cache::initializeCacheFromDatabase()
  {
    if (!objectDb || !timestampDb) return;
    
    DBC *cursor = NULL;
    DBT key, data;
    Key q;
    int ret;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    
    // Read timestamps to get LRU order
    timestampDb->cursor(timestampDb, NULL, &cursor, 0);
    if (!cursor) {
      qWarning() << "timestampDb.cursor() failed";
      return;
    }
    
    uint32_t timeSize[2];
    key.data = &q;
    key.ulen = sizeof(Key);
    key.flags = DB_DBT_USERMEM;
    data.data = &timeSize;
    data.ulen = sizeof(timeSize);
    data.flags = DB_DBT_USERMEM;
    
    QVector<EntryTime> tiletimes;
    while ((ret = cursor->get(cursor, &key, &data, DB_NEXT)) == 0) {
      assert(key.size == sizeof(Key) && data.size == sizeof(timeSize)); 
      Entry *e = new Entry(q);
      e->diskSize = timeSize[1];
      e->state = Disk;
      cacheEntries[q] = e;
      tiletimes << EntryTime(timeSize[0], q);
    }
    cursor->close(cursor);
    qSort(tiletimes);
    foreach (const EntryTime &t, tiletimes) {
      q = t.second;
      Entry *e = cacheEntries.value(q);
      addToDiskLRU(*e);
    }
  }
  
  void Cache::postIORequest(const IORequest &req)
  {
    tileQueueMutex.lock();
    tileQueue.enqueue(req);
    tileQueueCond.wakeOne();
    tileQueueMutex.unlock();
  }


  void Cache::addToDiskLRU(Entry &e)
  {
    assert(!e.is_linked() && e.state == Disk);
    diskLRUSize += e.diskSize;
    diskLRU.push_back(e);
  }
  
  void Cache::removeFromDiskLRU(Entry &e)
  {
    assert(e.is_linked() && e.state == Disk);
    diskLRUSize -= e.diskSize;
    e.unlink();
  }
  
  void Cache::addToMemLRU(Entry &e)
  {
    assert(!e.is_linked());
    assert(e.state == DiskAndMemory || e.state == MemoryOnly);
    assert(e.pixmap != NULL || !e.indexData.isEmpty());
    assert(!e.inUse);
    memLRUSize += e.memSize;
    memLRU.push_back(e);
  }
  
  void Cache::removeFromMemLRU(Entry &e)
  {
    assert(e.is_linked());
    memLRUSize -= e.memSize;
    e.unlink();
  }
  
  void Cache::purgeDiskLRU()
  {
    qint64 maxDiskLRUSize = qint64(maxDiskCache) * qint64(bytesPerMb);
    while (diskLRUSize > maxDiskLRUSize) {
      assert(!diskLRU.empty());
      Entry &e = diskLRU.front();
      diskLRU.pop_front();
      diskLRUSize -= e.diskSize;
      
      assert(e.state == Disk && e.pixmap == NULL);
      
      postIORequest(IORequest(DeleteObject, e.key, QVariant()));
      cacheEntries.remove(e.key);
      delete &e;
    }
  }

  void Cache::emptyDiskCache()
  {
    postIORequest(IORequest(ClearCache, 0, QVariant()));
    while (!diskLRU.empty()) {
      Entry &e = diskLRU.front();
      diskLRU.pop_front();
      
      assert(e.state == Disk && e.pixmap == NULL);
      
      cacheEntries.remove(e.key);
      delete &e;
    }

    diskLRUSize = 0;
  }
  
  void Cache::purgeMemLRU()
  {
    while (memLRUSize > ((unsigned int)maxMemCache) * bytesPerMb) {
      assert(!memLRU.empty());
      Entry &e = memLRU.front();
      
      memLRU.pop_front();
      memLRUSize -= e.memSize;
      if (e.pixmap) { 
        delete e.pixmap;
        e.pixmap = NULL;
      }
      e.indexData.clear();
      
      if (e.state == DiskAndMemory) {
        e.state = Disk;
        postIORequest(IORequest(UpdateObjectMetadata, e.key, QVariant(e.diskSize)));
        addToDiskLRU(e);
      } else {
        assert(e.state == MemoryOnly);
        cacheEntries.remove(e.key);
        delete &e;
      }
    }
    purgeDiskLRU();
  }
  
  void Cache::decompressObject(Key key, const QByteArray &compressed, QByteArray &indexData, QImage &tileData)
  {
    switch (keyKind(key)) {
    case IndexKind:
      indexData = qUncompress(compressed);
      break;
      
    case TileKind: 
      tileData = QImage::fromData(compressed);
      break;

    default: qFatal("Unknown key kind in decompressObject");
    }
  }
    
  bool Cache::loadObject(Entry *e, const QByteArray &indexData, 
                         const QImage &tileData)
  {
    switch (keyKind(e->key)) {
    case IndexKind: {
      e->indexData = indexData;
      e->memSize = e->indexData.size();

      qkey q = keyQuad(e->key);
      int layer = keyLayer(e->key);

      int numLevels = map->indexNumLevels(layer, q);

      int size = 0;
      for (int i = 1; i <= numLevels; i++) {
        // The formula sums the series 4 * (4^0 + 4^1 + 4^2 + 4^3 + ...) which is the size
        // in bytes of a complete 4-way tree without a root node of 32-bit integers.
        // See the online encyclopedia of integer sequences :-)
        size += (((1 << (2 * (i + 2))) - 1) / 3 - 1);
      }
      //      qDebug() << "got index size " << e->indexData.size() << " expected " << size << " levels " << numLevels;

      if (e->indexData.size() != size) return false;

      return true;
    }

    case TileKind: {
      if (tileData.isNull()) return false;
      QPixmap *p = new QPixmap(QPixmap::fromImage(tileData));
      e->pixmap = p;
      e->memSize = p->size().width() * p->size().height() * p->depth() / 8;
      return true;
    }

    default: 
      qFatal("Unknown kind in decompressObject");
      return false; // Shut up gcc warning
    }
  }

  void Cache::maybeFetchIndexPendingTiles() {
    // There might be tiles waiting on this index. Run over the list of
    // objects waiting for an index.
    //    qDebug() << "Reconsidering index pending list";
    CacheList::iterator it(indexPending.begin()), itend(indexPending.end());
    while (it != itend) {
      Entry &e = *it;
      it++;
      maybeAddNetworkRequest(&e);
    }
    startNetworkRequests();
  }

  static const QEvent::Type newDataEventType 
      = QEvent::Type(QEvent::registerEventType());

  NewDataEvent::NewDataEvent(Key key, const QString &err)
    : QEvent(newDataEventType), fError(err), fKey(key)
  {
  }

  NewDataEvent::NewDataEvent(Key key, const QByteArray &data, 
                             const QByteArray &indexData, const QImage &tileData) 
    : QEvent(newDataEventType), fKey(key), fData(data), fIndexData(indexData), 
      fTileData(tileData)
  {
  }

  bool Cache::event(QEvent *ev)
  {
    if (ev->type() == newDataEventType) {
      NewDataEvent *nev = (NewDataEvent *)ev;

      Key key = nev->key();
      const QByteArray &data = nev->data();
      const QByteArray &indexData = nev->indexData();
      const QImage &tileData = nev->tileData();

      assert(cacheEntries.contains(key));
      Entry *e = cacheEntries.value(key);
      assert(e->state == Loading || e->state == NetworkPending);

      bool ok = !indexData.isEmpty() || !tileData.isNull();
      if (ok) {
        ok = loadObject(e, indexData, tileData);
      }
      
      e->diskSize = data.size();

      switch (e->state) {
      case Loading:
        if (!ok) {
          e->state = Disk;
          if (dbEnv) {
            QString msg = tr("Error reading cached object from disk: %1")
              .arg(nev->errorString());
            emit(ioError(msg));
          }
          
          addToDiskLRU(*e);
        } else {
          // qDebug() << "received " << e->key << " from disk";
          e->state = DiskAndMemory;
          
          if (e->inUse) {
            memInUse.push_back(*e);
          } else {
            addToMemLRU(*e);
            purgeMemLRU();
          }
        }
        break;
      case NetworkPending:
        if (ok) {
          // qDebug() << "received " << e->key << " from network";
          e->state = Saving;
      
          postIORequest(IORequest(SaveObject, key, data));
        } else {
          // We had a network error; we have no way to restore the tile to a valid
          // state so we just dump it. If it is wanted again it will be requested
          // again.
          QString msg = tr("Error reading from network: %1")
            .arg(nev->errorString());
          emit(ioError(msg));

          cacheEntries.remove(key);
          delete e;
        }
        break;

      default: qFatal("Invalid object state in network event");
      }

      if (ok) {
        if (keyKind(key) == IndexKind) {
          maybeFetchIndexPendingTiles();
        }
        if (e->inUse) {
          emit(tileLoaded());
        }
      }
      return true;
    }
    return QObject::event(ev);
  }


void Cache::objectSavedToDisk(Key q, bool success)
{
  assert(cacheEntries.contains(q));
  Entry *e = cacheEntries.value(q);
  assert(e->state == Saving && !e->is_linked());

  if (success) {
    e->state = DiskAndMemory;
  } else {
    // Saving failed. We'll just leave the file in the memory cache
    e->state = MemoryOnly;

    if (dbEnv) {
      qDebug() << "WARNING: Could not save tile to disk " << q;
    }
  }
  if (e->inUse) {
    memInUse.push_back(*e);
  } else {
    addToMemLRU(*e);
    purgeMemLRU();
  }
}


  // Find tiles that were previously in use that are no longer in use.
  void Cache::pruneObjects(const QList<QRect> &rects)
  {
    CacheList::iterator it(memInUse.begin()), itend(memInUse.end());
    
    while (it != itend) {
      Entry &e = *it;
      it++;
      assert((e.state == DiskAndMemory || e.state == MemoryOnly) && e.inUse);
        
      qkey q = keyQuad(e.key);
      Tile tile(keyLayer(e.key), q);
      QRect r = map->tileToMapRect(tile);
      
      bool inUse = false;
      foreach (const QRect &vis, rects) {
        inUse = inUse || r.intersects(vis);
      }
      if (!inUse) {
        e.unlink();
        e.inUse = false;
        addToMemLRU(e);
      }
    }
    purgeMemLRU();
  }


  void Cache::findTileRange(qkey q, Entry *e, uint32_t &offset, uint32_t &len)
  {
    if (e->indexData.isEmpty()) {
      // Dummy index
      offset = 0;
      len = 0;
      return;
    }

    uint32_t *idxData = (uint32_t *)e->indexData.constData();
    int idxLen = e->indexData.size() / 4;

    // Find the start of the tree for the tile level of q
    int level = log2_int(q) / 2;
    int base = 0;
    offset = 0;
    for (int i = 1; i < level; i++) {
      assert(base < idxLen);
      offset += idxData[base];
      // base += (4^0 + 4^2 + ... + 4^i)
      base += (((1 << (2 * (i + 1))) - 1) / 3);
    }
   
    //qDebug() << "tree base " << base << " offset  " << offset;
    int pos = 1;
    // For all levels except the last one...
    for (int l = 1; l <= level - 1; l++) {
      assert(base + pos + 4 <= idxLen);
      int digit = q & 3;
      for (int i = 0; i < digit; i++) {
        offset += idxData[base + pos + i];
      }
      
      q >>= 2;
      pos = 4 * (pos + digit) + 1;
    }
    
    assert(base + pos + 4 <= idxLen);
    // The last level
    int digit = q & 3;
    for (int i = 0; i < digit; i++) {
      offset += idxData[base + pos + i];
    }
    len = idxData[base + pos + digit];
    /* qDebug() << "q " << q0 << " " <<Tile(keyLayer(e->key), q0).toQuadKeyString() << " level " << level << " offset " << offset << " len " << len << " pos " << pos << " digit " << digit;*/
  }

  bool Cache::requestTiles(const QList<Tile> &tiles)
  {
    bool present = true;
    foreach (const Tile& tile, tiles) {
      qkey q = tile.toQuadKey();
      bool tilePresent = requestObject(tileKey(tile.layer(), q));
      //if (!tilePresent) { qDebug() << "tile " << tile.x << " " << tile.y << " " << tile.level << " " << 
      //    tileKey(q) << " present " << tilePresent; }
      present = present && tilePresent;
    }
    startNetworkRequests();
    return present;
  }

  void Cache::startNetworkRequests() {
    while (requestsInFlight < maxNetworkRequestsInFlight &&
           !networkRequestQueue.empty()) {
      NetworkRequestBundle &b = networkRequestQueue.front();
      networkRequestQueue.pop_front();
      networkRequests.removeOne(&b);
      
      numNetworkReqs += b.numRequests();
      numNetworkBundles++;
      networkReqSize += b.length();
      
      requestsInFlight++;
      b.makeRequests(&manager);
    }
  }

  void Cache::networkRequestFinished()
  {
    requestsInFlight--;
    startNetworkRequests();
  }

  void Cache::maybeAddNetworkRequest(Entry *e)
  {
    assert(e->state == IndexPending && e->is_linked());

    int layer = keyLayer(e->key);
    qkey q = keyQuad(e->key);
    qkey qidx, qtile;    
    uint32_t offset, len;

    // Ensure the parent index (if any) is loaded
    if (map->parentIndex(layer, q, qidx, qtile)) {
      Key idxKey = indexKey(layer, qidx);
      //      qDebug() << "qidx " << qidx << " qtile " << qtile << " idxkey " << idxKey  << " file " << map->indexFile(layer, qidx);

      requestObject(idxKey);
      Entry *idx = cacheEntries.value(idxKey);
    
      if (!isInMemory(idx->state)) {
        return;
      }

      // Find the tile range of the immediate parent (even for indices).
      // If it's empty, we're done here.
      findTileRange(qtile, idx, offset, len);
      if (len == 0) {
        // qDebug() << "request " << e->key << " out of range";
        // Object doesn't exist (it's outside the map data), so we're done
        e->state = MemoryOnly;
        e->pixmap = new QPixmap();
        e->unlink();
        memInUse.push_back(*e);
        return;
      }
    } else {
      assert(keyKind(e->key) == IndexKind);
    }

    e->unlink();
    e->state = NetworkPending;


    if (keyKind(e->key) == IndexKind) {
      // Fetch the whole of every index.
      offset = 0;
      len = 0;
      qidx = q;
    }

    /*    qDebug() << "Req list before insert";
    for (int i = 0; i<networkRequests.size(); i++) {
      qDebug() << networkRequests[i]->kind() << " " << networkRequests[i]->layer() << " " << networkRequests[i]->offset() << " " << networkRequests[i]->length();
      }*/

    // First enqueue the item
    NetworkRequestBundle *bundle =
      new NetworkRequestBundle(this, map, qidx, offset, e->key, len, this);
    QList<NetworkRequestBundle *>::iterator cur, next =
      qLowerBound(networkRequests.begin(), networkRequests.end(), bundle, 
                  NetworkRequestBundle::lessThan);

    // Try to merge the bundle with the preceding bundle
    if (next != networkRequests.begin()) {
      QList<NetworkRequestBundle *>::iterator prev = next - 1;
      if ((*prev)->mergeBundle(bundle)) {
        delete bundle;
        bundle = *prev;
        cur = prev;
      } else {
        cur = networkRequests.insert(next, bundle);
        next = cur + 1;
        networkRequestQueue.push_back(*bundle);
      }
    } else {
      cur = networkRequests.insert(next, bundle);
      next = cur + 1;
      networkRequestQueue.push_back(*bundle);
    }

    // Try to merge the (possibly) combined bundle with the next bundle
    if (next != networkRequests.end()) {
      if ((*next)->mergeBundle(bundle)) {
        networkRequests.erase(cur);
        delete bundle;
      }
    }

    /*qDebug() << "Req list after insert";
    for (int i = 0; i<networkRequests.size(); i++) {
      qDebug() << networkRequests[i]->kind() << " " << networkRequests[i]->layer() << " " << networkRequests[i]->offset() << " " << networkRequests[i]->length();
      }*/

  }

  bool Cache::requestObject(Key key)
  {
    bool present;
    if (cacheEntries.contains(key)) {
      Entry *e = cacheEntries.value(key);
      switch (e->state) {
      case Disk: {
        // qDebug() << "request " << key << " disk cache hit";
        diskCacheHits++;
        memCacheMisses++;
        removeFromDiskLRU(*e);
        e->state = Loading;  
        e->inUse = true;
        
        postIORequest(IORequest(LoadObject, key, QVariant()));
        present = false;
        break;
      }      
      case Loading:
      case IndexPending:
      case NetworkPending:
        // qDebug() << "request " << key << " in transition";
        e->inUse = true;
        present = false;
        break;

      case Saving:
        // qDebug() << "request " << key << " in transition";
        e->inUse = true;
        present = true;
        break;
        
      case DiskAndMemory:
      case MemoryOnly:
        // qDebug() << "request " << key << " memory hit";
        if (!e->inUse) {
          memCacheHits++;
          removeFromMemLRU(*e);
          memInUse.push_back(*e);
          e->inUse = true;
        }
        present = true;
        break;
        
      default: abort(); // Unreachable
      }
    } else {
      // Load object from the network.
      memCacheMisses++;
      diskCacheMisses++;
      // qDebug() << "request " << key << " cache miss";
      Entry *e = new Entry(key);
      cacheEntries[key] = e;
      e->state = IndexPending;
      e->inUse = true;
      present = false;
      indexPending.push_back(*e);
      maybeAddNetworkRequest(e);
    } 
    return present;
  }


bool Cache::getTile(const Tile &tile, QPixmap &p) const
{
  Key key = tileKey(tile.layer(), tile.toQuadKey());
  if (cacheEntries.contains(key)) {
    Entry *e = cacheEntries.value(key);
    assert(keyKind(e->key) == TileKind);
    if (isInMemory(e->state)) {
      p = *e->pixmap;
      return true;
    }
  } 
  return false;
}

} // namespace "Cache"
