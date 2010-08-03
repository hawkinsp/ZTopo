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

#include <QDebug>
#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QPainter>
#include <QPrinter>
#include "consts.h"
#include "printscene.h"
#include "map.h"
#include "maprenderer.h"
#include "projection.h"

class MapItem : public QGraphicsItem, public MapRendererClient
{
public:
  MapItem(Map *m, MapRenderer *r, QGraphicsItem *parent);
  ~MapItem();

  virtual QRectF boundingRect() const;
  virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem * option, 
                     QWidget * widget = 0);

  void centerOn(QPoint center) { mapCenter = center; computeGeometry(); }

  // Set printer resolution
  void setDpi(int dpiX, int dpiY);

  // Set page metrics
  void setRect(const QRectF &r);

  // Set the current map layer
  void setMapLayer(int layer) { mapLayer = layer; update(); }

  // Set the current map scale; 24000 means 1:24000.
  void setMapScale(int scale) { mapScale = scale; computeGeometry(); }

  // Set grid projection
  void showGrid(Datum d, bool utm, qreal interval);
  void hideGrid();

  // Map Renderer client methods
  virtual int currentLayer() const;
  virtual QRect visibleArea() const;

  bool loadTiles();

private:
  Map *map;
  MapRenderer *renderer;
  QRectF itemRect;

  int mapLayer, mapScale;
  QPoint mapCenter;
  
  int dpiX, dpiY;
  QRect mapPixelRect;
  qreal scale, scaleX, scaleY;

  bool gridEnabled;
  Datum gridDatum;
  bool gridUTM;
  qreal gridInterval;

  void computeGeometry();
};


MapItem::MapItem(Map *m, MapRenderer *r, QGraphicsItem *parent)
  : QGraphicsItem(parent), map(m), renderer(r)
{
  renderer->addClient(this);
  mapLayer = 0;
  mapScale = 24000;
  dpiX = dpiY = 72;
  gridEnabled = false;
}

MapItem::~MapItem()
{
  renderer->removeClient(this);
}

void MapItem::setDpi(int adpiX, int adpiY)
{
  dpiX = adpiX; dpiY = adpiY;
  computeGeometry();
}

void MapItem::setRect(const QRectF &r)
{
  prepareGeometryChange();
  itemRect = r;
  computeGeometry();
}

QRectF MapItem::boundingRect() const
{
  return itemRect;
}

void MapItem::computeGeometry()
{
  // Size of the page in meters
  QSizeF pagePhysicalArea(qreal(itemRect.width()) / dpiX * metersPerInch, 
                 qreal(itemRect.height()) / dpiY * metersPerInch);

  // Size of the map area in meters
  QSizeF mapPhysicalArea = pagePhysicalArea * mapScale;

  // Size of the map area in pixels
  // QSize mapPixelArea = 
  // map->mapToProj().mapRect(QRectF(QPointF(0, 0), mapPhysicalArea)).size().toSize();
  QSizeF mapPixelSize = map->mapPixelSize();
  QSize mapPixelArea(mapPhysicalArea.width() / mapPixelSize.width(), 
                     mapPhysicalArea.height() / -mapPixelSize.height());

  QPoint mapPixelTopLeft = mapCenter - QPoint(mapPixelArea.width() / 2, 
                                              mapPixelArea.height() / 2);
  mapPixelRect = QRect(mapPixelTopLeft, mapPixelArea);

  scaleX = qreal(itemRect.width()) / mapPixelArea.width();
  scaleY = qreal(itemRect.height()) / mapPixelArea.height();
  scale = std::max(scaleX, scaleY);

  /*  qDebug("Print mapScale %d; scale %f", mapScale, scale);
  qDebug("Logical dpi is %d %d\n", dpiX, dpiY);
  qDebug("Page is %f x %f = %f m x %f m\n", itemRect.width(), itemRect.height(), 
         pagePhysicalArea.width(),
         pagePhysicalArea.height());
  qDebug("Map is %d x %d = %f m x %f m\n", mapPixelArea.width(), mapPixelArea.height(), mapPhysicalArea.width(),
         mapPhysicalArea.height());
         qDebug() << mapCenter;*/
  update();
  
}

bool MapItem::loadTiles()
{
  return renderer->loadTiles(mapLayer, mapPixelRect, scale);
}

void MapItem::paint(QPainter *painter, const QStyleOptionGraphicsItem * option, 
                     QWidget * widget)
{
  loadTiles();
  painter->setBackground(Qt::white);
  painter->eraseRect(itemRect);

  qreal borderWidth = 0.4 / pointsPerInch * dpiX;
  QRectF borderRect = itemRect.adjusted(borderWidth / 2, borderWidth / 2, 
                                        -borderWidth / 2, -borderWidth / 2);
  QPen borderPen(Qt::black);
  borderPen.setWidthF(borderWidth);
  painter->setPen(borderPen);
  painter->drawRect(borderRect);


  QRectF mapRect = itemRect.adjusted(borderWidth, borderWidth,
                                     -borderWidth, -borderWidth);
  painter->setClipRect(mapRect);

  painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
  painter->scale(scaleX / scale, scaleY / scale);
  renderer->render(*painter, mapLayer, mapPixelRect, scale);

  if (gridEnabled) {
    QPen pen(Qt::blue);
    pen.setWidthF(borderWidth/2);
    painter->setPen(pen);
    if (gridUTM) {
      renderer->renderUTMGrid(*painter, mapPixelRect, scale, gridDatum, gridInterval);
    } else {
      renderer->renderGeographicGrid(*painter, mapPixelRect, scale, gridDatum, 
                                     gridInterval);
    }
  }
}

int MapItem::currentLayer() const {
  return mapLayer;
}

QRect MapItem::visibleArea() const {
  return mapPixelRect;
}

void MapItem::showGrid(Datum d, bool utm, qreal interval)
{
  gridEnabled = true;
  gridDatum = d;
  gridUTM = utm;
  gridInterval = interval;
  update();
}

void MapItem::hideGrid()
{
  gridEnabled = false;
  update();
}


// Print scene ----------------------------------------------------------------
PrintScene::PrintScene(Map *m, MapRenderer *r, const QPrinter &printer)
{
  setBackgroundBrush(Qt::gray);

  paperRect = addRect(printer.paperRect(), QColor(Qt::black), Qt::white);
  pageRect = new QGraphicsRectItem(paperRect);
  pageRect->setPen(QColor(Qt::white));
  pageRect->setBrush(Qt::white);

  mapItem = new MapItem(m, r, pageRect);

  setPageMetrics(printer);

  connect(&r->getCache(), SIGNAL(tileLoaded()),
          this, SLOT(tileLoaded()));
}

void PrintScene::setPageMetrics(const QPrinter &printer)
{
  paperRect->setRect(printer.paperRect());
  pageRect->setRect(printer.pageRect());
  mapItem->setRect(printer.pageRect());
  mapItem->setDpi(printer.logicalDpiX(), printer.logicalDpiY());
}

void PrintScene::setMapLayer(int layer)
{
  mapItem->setMapLayer(layer);
}

void PrintScene::setMapScale(int scale)
{
  mapItem->setMapScale(scale);
}

void PrintScene::centerMapOn(QPoint c)
{
  mapItem->centerOn(c);
}

void PrintScene::tileLoaded()
{
  mapItem->update();
}

void PrintScene::showGrid(Datum d, bool utm, qreal interval)
{
  mapItem->showGrid(d, utm, interval);
}

void PrintScene::hideGrid()
{
  mapItem->hideGrid();
}

bool PrintScene::tilesFinishedLoading()
{
  return mapItem->loadTiles();
}
