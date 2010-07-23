#include <QDataStream>
#include <QDebug>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringBuilder>
#include <db_cxx.h>
#include "tilecache.h"

using namespace boost::intrusive;

static const int numWorkerThreads = 2;
static const unsigned int maxMemLRUSize = 20000000;
static const int maxThreadWaitTime = 1000; // Maximum time to wait for a thread to die

static const int maxTileSize = 32768;

TileEntry::TileEntry(const Tile& aKey) 
  : key(aKey), memSize(0), diskSize(0), pixmap(NULL), state(Invalid)
{
}

/*

MapTileIOThread::MapTileIOThread(TileCache *v, QDir c, QObject *parent)
  : QThread(parent), view(v), cachePath(c)
{

}

void MapTileIOThread::run()
{
  forever {
    view->tileQueueMutex.lock();
    while (view->tileQueue.isEmpty()) {
      view->tileQueueCond.wait(&view->tileQueueMutex);
    }
    QPair<Tile, QByteArray> pair = view->tileQueue.dequeue();
    view->tileQueueMutex.unlock();

    Tile key = pair.first;
    QByteArray data = pair.second;

    if (key.level < 0) break; // Thread termination marker

    QString tilePath(view->map->tilePath(key));
    if (data.isNull()) {
      // Load request
      QImage img;
      if (img.load(cachePath.filePath(tilePath))) {
        QImage imgRgb = img.convertToFormat(QImage::Format_RGB32);
        emit(tileLoadedFromDisk(key, imgRgb));
      } else {
        emit(tileLoadedFromDisk(key, img));
      }
    } else {
      // Save request
      QFileInfo tilePathInfo(tilePath);
      cachePath.mkpath(tilePathInfo.path());
      QFile f(cachePath.filePath(tilePath));
      bool ok = f.open(QIODevice::WriteOnly);
      if (ok) {
        int ret = f.write(data);
        ok = (ret == data.size());
        f.close();
      }
      emit(tileSavedToDisk(key, ok));
    }
  }
}
*/

TileCache::TileCache(Map *m)
  : map(m), manager(this), memLRUSize(0), diskLRUSize(0), diskCacheHits(0),
    diskCacheMisses(0), memCacheHits(0), memCacheMisses(0)
{
  cachePath = "/Users/hawkinsp/tcache";

  dbBuffer = new char[maxTileSize];
  
  QString dbName;
  try {
    u_int32_t envFlags = DB_CREATE | DB_INIT_MPOOL;
    dbEnv = new DbEnv(0);
    dbEnv->open(cachePath.path().toLatin1().data(), envFlags, 0);

    for (int i = 0; i < map->numLayers(); i++) {
      Db *db = new Db(dbEnv, 0);
      u_int32_t dbFlags = DB_CREATE;
    
      dbName = map->id() % "-" % map->layer(i).id % ".db";

      db->open(NULL, dbName.toLatin1().data(), NULL, DB_BTREE, dbFlags, 0);
      layerDbs << db;
    }
  } catch (DbException &e) {
    delete dbEnv;
    dbEnv = NULL;
    qFatal("Database exception opening tile cache environment %s", 
           cachePath.path().toLatin1().data());
  } catch (std::exception &e) {
    delete dbEnv;
    dbEnv = NULL;
    qFatal("Database exception opening tile cache environment %s", 
           cachePath.path().toLatin1().data());
  }

  /*
  for (int i = 0; i < numWorkerThreads; i++) {
    MapTileIOThread *ioThread = new MapTileIOThread(this, cachePath, this);
    ioThreads << ioThread;
    connect(ioThread, SIGNAL(tileLoadedFromDisk(Tile, QImage)),
            this, SLOT(tileLoadedFromDisk(Tile, QImage)));
    connect(ioThread, SIGNAL(tileSavedToDisk(Tile, bool)),
            this, SLOT(tileSavedToDisk(Tile, bool)));

    ioThread->start();
  }
  */

  connect(&manager, SIGNAL(finished(QNetworkReply*)),
          this, SLOT(networkReplyFinished(QNetworkReply*)));
}

TileCache::~TileCache()
{
  /*
  // Terminate worker threads
  tileQueueMutex.lock();
  for (int i = 0; i < ioThreads.size(); i++) {
    tileQueue.enqueue(QPair<Tile, QByteArray>(Tile(-1, -1, -1, -1), QByteArray()));
  }
  tileQueueCond.wakeAll();
  tileQueueMutex.unlock();
  foreach (MapTileIOThread *t, ioThreads) {
    t->wait(maxThreadWaitTime);
    delete t;
  }
  */
  
  qDebug() << "Memory cache hits: " << memCacheHits << " misses: " << memCacheMisses
           << " (" << 
    qreal(memCacheHits * 100.0) / qreal(memCacheHits + memCacheMisses) << "%)";
  qDebug() << "Disk cache hits: " << diskCacheHits << " misses: " << diskCacheMisses
           << " (" << 
    qreal(diskCacheHits * 100.0) / qreal(diskCacheHits + diskCacheMisses) << "%)";

  try {
    foreach (Db *db, layerDbs) {
      db->close(0);
      delete db;
    }
    dbEnv->close(0);
    delete dbEnv;
  } catch (DbException &e) {
    qWarning() << "Database exception closing tile cache";
    delete dbEnv;
  } catch (std::exception &e) {
    qWarning() << "IO exception closing tile cache";
    delete dbEnv;
  }

  delete[] dbBuffer;
}

void TileCache::saveDiskCacheIndex()
{
}

void TileCache::addToDiskLRU(TileEntry &e)
{
  assert(!e.is_linked() && e.state == Disk);
  diskLRUSize += e.diskSize;
  diskLRU.push_back(e);
}

void TileCache::removeFromDiskLRU(TileEntry &e)
{
  assert(e.is_linked() && e.state == Disk);
  diskLRUSize -= e.diskSize;
  e.unlink();
}

void TileCache::addToMemLRU(TileEntry &e)
{
  assert(!e.is_linked());
  assert(e.state == DiskAndMemory || e.state == MemoryOnly);
  assert(e.pixmap != NULL);
  assert(!e.inUse);
  memLRUSize += e.memSize;
  memLRU.push_back(e);
}

void TileCache::removeFromMemLRU(TileEntry &e)
{
  assert(e.is_linked());
  memLRUSize -= e.memSize;
  e.unlink();
}

void TileCache::purgeDiskLRU()
{

}

void TileCache::purgeMemLRU()
{
  while (memLRUSize > maxMemLRUSize) {
    assert(!memLRU.empty());
    TileEntry &e = memLRU.front();
    
    memLRU.pop_front();
    memLRUSize -= e.memSize;
    delete e.pixmap;
    e.pixmap = NULL;
    e.state = Disk;

    addToDiskLRU(e);
    purgeDiskLRU();
  }
}

void TileCache::tileLoadedFromDisk(Tile key, QImage img)
{
  assert(tileMap.contains(key));
  TileEntry *e = tileMap.value(key);
  assert(e->state == Loading);

  e->unlink(); // Should be on loadingList
  if (img.isNull()) {
    e->state = Disk;
    qDebug() << "WARNING: Could not load tile from disk " << key.x << "," << key.y 
             << "," << key.level;
    addToDiskLRU(*e);
  } else {
    e->state = DiskAndMemory;
    QPixmap *p = new QPixmap(QPixmap::fromImage(img));
    e->pixmap = p;
    e->memSize = p->size().width() * p->size().height() * p->depth() / 8;

    if (e->inUse) {
      memInUse.push_back(*e);
      emit(tileLoaded(key));
    } else {
      addToMemLRU(*e);
      purgeMemLRU();
    }
  }
}

void TileCache::tileSavedToDisk(Tile key, bool success)
{
  assert(tileMap.contains(key));
  TileEntry *e = tileMap.value(key);
  assert(e->state == Saving && !e->is_linked());

  if (success) {
    e->state = DiskAndMemory;
  } else {
    // Saving failed. We'll just leave the file in the memory cache
    e->state = MemoryOnly;

    qDebug() << "WARNING: Could not save tile to disk " << key.x << "," << key.y 
             << "," << key.level;
  }
  if (e->inUse) {
    memInUse.push_back(*e);
  } else {
    addToMemLRU(*e);
    purgeMemLRU();
  }
}

// Find tiles that were previously in use that are no longer in use.
void TileCache::pruneTiles(const QList<QRect> &rects)
{
  TileList::iterator it(memInUse.begin()), itend(memInUse.end());

  while (it != itend) {
    TileEntry &e = *it;
    it++;
    assert((e.state == DiskAndMemory || e.state == MemoryOnly) 
           && e.inUse && e.pixmap != NULL);

    QRect r = map->tileToMapRect(e.key);
  
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

static const QNetworkRequest::Attribute tileAttr(QNetworkRequest::User);
static const QNetworkRequest::Attribute layerAttr(QNetworkRequest::Attribute(QNetworkRequest::User + 1));

void TileCache::requestTile(const Tile& key)
{
  if (tileMap.contains(key)) {
    TileEntry *e = tileMap.value(key);
    switch (e->state) {
    case Disk: {
      diskCacheHits++;
      memCacheMisses++;
      removeFromDiskLRU(*e);
      e->state = Loading;  
      e->inUse = true;
      loadingList.push_back(*e);

      QImage img;
      unsigned int q = map->tileToQuadKeyInt(key);
      Db *db = layerDbs[key.layer];
      Dbt dbKey(&q, sizeof(unsigned int));
      Dbt dbData;
      dbData.set_data(dbBuffer);
      dbData.set_ulen(maxTileSize);
      dbData.set_flags(DB_DBT_USERMEM);
      int ret = db->get(NULL, &dbKey, &dbData, 0);

      if (ret == 0) {
        img.loadFromData((const uchar *)dbBuffer, int(dbData.get_size()),
                                    "png");
      } else {
        qWarning() << "Error loading tile " << q << " layer " << key.layer;
      }
      tileLoadedFromDisk(key, img);
      /*
      tileQueueMutex.lock();
      tileQueue.enqueue(QPair<Tile, QByteArray>(key, QByteArray()));
      tileQueueCond.wakeOne();
      tileQueueMutex.unlock();*/
    }
      break;
      
    case Loading:
    case NetworkPending:
    case Saving:
      e->inUse = true;
      break; 

    case DiskAndMemory:
    case MemoryOnly:
      if (!e->inUse) {
        memCacheHits++;
        removeFromMemLRU(*e);
        memInUse.push_back(*e);
        e->inUse = true;
      }
      break;

    default: abort(); // Unreachable
    }
  } else {
    unsigned int q = map->tileToQuadKeyInt(key);
    if (map->layer(key.layer).missingTiles.containsPrefix(q)) {
      return; // This tile doesn't exist
    }

    // Load tile from the network
    memCacheMisses++;
    diskCacheMisses++;
    TileEntry *e = new TileEntry(key);
    tileMap[key] = e;
    e->state = NetworkPending;
    e->inUse = true;
    QString baseUrl(map->baseUrl().toString());
    QNetworkRequest req(QUrl(QString(baseUrl % "/" % map->tilePath(key))));
    req.setAttribute(tileAttr, QVariant(q));
    req.setAttribute(layerAttr, QVariant(key.layer));
    manager.get(req);
  }
}


bool TileCache::getTile(const Tile &key, QPixmap &p) const
{
  if (tileMap.contains(key)) {
    TileEntry *e = tileMap.value(key);
    if (e->state == DiskAndMemory || e->state == MemoryOnly || e->state == Saving) {
      p = *e->pixmap;
      return true;
    }
  } 

  unsigned int q = map->tileToQuadKeyInt(key);
  if (map->layer(key.layer).missingTiles.containsPrefix(q)) {
    return true; // This tile doesn't exist
  }
  return false;
}

void TileCache::networkReplyFinished(QNetworkReply *reply)
{
  //  qDebug() << "got network reply " << reply->size() << " for " << reply->request().url().toString() << " " << reply->error();

  QNetworkRequest req = reply->request();
  unsigned int q = req.attribute(tileAttr).toUInt();
  int layer = req.attribute(layerAttr).toInt();
  Tile key = map->quadKeyIntToTile(layer, q);

  assert(tileMap.contains(key));
  TileEntry *e = tileMap.value(key);
  assert(e->state == NetworkPending);

  QImage img;
  QByteArray imgData;
  bool ok = false;
  if (reply->error() == QNetworkReply::NoError) {
    imgData = reply->readAll();
    img = QImage::fromData(imgData);
    ok = !img.isNull();
  }

  if (ok) {
    e->state = Saving;

    QPixmap *p = new QPixmap(QPixmap::fromImage(img));
    e->pixmap = p;
    e->memSize = p->size().width() * p->size().height() * p->depth() / 8;
    e->diskSize = imgData.size();

    /*    tileQueueMutex.lock();
    tileQueue.enqueue(QPair<Tile, QByteArray>(key, data));
    tileQueueCond.wakeOne();
    tileQueueMutex.unlock();*/

    Dbt dbKey(&q, sizeof(unsigned int));
    Dbt dbData((void *)imgData.constData(), imgData.size());
    Db *db = layerDbs[layer];
    int ret = db->put(NULL, &dbKey, &dbData, 0);
    if (ret != 0) {
      qWarning() << "Tile database put returned " << ret;
    }
    tileSavedToDisk(key, ret == 0);

    if (e->inUse) {
      emit(tileLoaded(key));
    }
  } else {
    // We had a network error; we have no way to restore the tile to a valid
    // state so we just dump it. If it is wanted again it will be requested again.
    qDebug() << "WARNING: Network error reading " << key.x << "," << key.y 
             << "," << key.level << ": " << reply->error();
    
    tileMap.remove(key);
    delete e;
  }
}
