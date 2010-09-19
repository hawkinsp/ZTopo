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
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPrinter>
#include <QStyleOptionGraphicsItem>
#include "consts.h"
#include "coordformatter.h"
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

  bool loadTiles(qreal scale);

protected:
  virtual void mousePressEvent(QGraphicsSceneMouseEvent *);
  virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *);
  virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *);

private:
  Map *map;
  MapRenderer *renderer;
  QRectF itemRect;
  QRectF borderRect;
  QRectF mapRect;
  qreal borderWidth;
  qreal gridMarginWidth;


  int mapLayer, mapScale;
  QPoint mapCenter;
  
  int dpiX, dpiY;
  QRect mapPixelRect;
  qreal scale, scaleX, scaleY;

  bool gridEnabled;
  Datum gridDatum;
  bool gridUTM;
  qreal gridInterval;
  CoordFormatter *gridFormatter;

  void computeGeometry();

  // Convert a point from item coordinates to map coordinates
  QPointF itemToMap(QPointF);
};


MapItem::MapItem(Map *m, MapRenderer *r, QGraphicsItem *parent)
  : QGraphicsItem(parent), map(m), renderer(r), gridFormatter(NULL)
{
  setAcceptedMouseButtons(Qt::LeftButton);
  setCursor(Qt::OpenHandCursor);
  setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);

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
  gridMarginWidth = gridEnabled ? 20.0 / pointsPerInch * dpiX : 0;
  borderWidth = 0.4 / pointsPerInch * dpiX;

  qreal bg = gridMarginWidth + borderWidth / 2;
  borderRect = itemRect.adjusted(bg, bg, -bg, -bg);
  mapRect = borderRect.adjusted(borderWidth / 2, borderWidth / 2,
                                     -borderWidth / 2, -borderWidth / 2);

  // Size of the page in meters
  QSizeF pagePhysicalArea(qreal(mapRect.width()) / dpiX * metersPerInch, 
                 qreal(mapRect.height()) / dpiY * metersPerInch);

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

  scaleX = qreal(mapRect.width()) / mapPixelArea.width();
  scaleY = qreal(mapRect.height()) / mapPixelArea.height();
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

QPointF MapItem::itemToMap(QPointF p)
{
  return (p / scale) + mapPixelRect.topLeft();
}

bool MapItem::loadTiles(qreal bumpedScale)
{
  if (bumpedScale < 0.0) bumpedScale = scale;
  return renderer->loadTiles(mapLayer, mapPixelRect, bumpedScale);
}

void MapItem::paint(QPainter *painter, const QStyleOptionGraphicsItem * option, 
                     QWidget * widget)
{
  qreal detail = QStyleOptionGraphicsItem::levelOfDetailFromTransform(
                      painter->worldTransform());
  //  qDebug() << "painter" << option->exposedRect << gridEnabled << detail;
  painter->setBackground(Qt::white);
  painter->eraseRect(itemRect);


  QPen borderPen(Qt::black);
  borderPen.setWidthF(borderWidth);
  painter->setPen(borderPen);
  painter->drawRect(borderRect);



  // Portion of mapPixelRect that is exposed
  QRectF exposedMapRect = option->exposedRect.intersected(mapRect);

  qreal exposedLeft = (exposedMapRect.left() - mapRect.left()) / mapRect.width() 
    * mapPixelRect.width() + mapPixelRect.left();
  qreal exposedTop = (exposedMapRect.top() - mapRect.top()) / mapRect.height() 
    * mapPixelRect.height() + mapPixelRect.top();
  qreal exposedWidth = exposedMapRect.width() / mapRect.width() 
    * mapPixelRect.width();
  qreal exposedHeight = exposedMapRect.height() / mapRect.height() 
    * mapPixelRect.height();
  QRectF exposedMapArea(exposedLeft, exposedTop, exposedWidth, exposedHeight);

  //  qDebug() << mapPixelRect << exposedMapArea;

  qreal bumpedScaleDetail;
  int bumpedTileSize;
  renderer->bumpScale(mapLayer, scale * detail, bumpedScaleDetail, bumpedTileSize);
  qreal bumpedDetail = bumpedScaleDetail / scale;
  qreal bumpedScale = scale;
  painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
  painter->setRenderHint(QPainter::TextAntialiasing, true);

  loadTiles(bumpedScaleDetail);

  painter->save();
  painter->setClipRect(mapRect);
  painter->translate(exposedMapRect.topLeft());
  painter->scale(1.0 / bumpedDetail, 1.0 / bumpedDetail);
  renderer->render(*painter, mapLayer, exposedMapArea.toAlignedRect(), 
                   bumpedScaleDetail);
  painter->restore();


  if (gridEnabled) {
    QPen pen(Qt::blue);
    pen.setWidthF(borderWidth * detail);



    painter->save();
    painter->setClipRect(mapRect);
    painter->translate(mapRect.topLeft());
    painter->scale(1.0 / bumpedDetail, 1.0 / bumpedDetail);
    painter->setPen(pen);
    QList<GridTick> ticks;
    if (gridUTM) {
      renderer->renderUTMGrid(*painter, mapPixelRect, bumpedScaleDetail, gridDatum,
                              gridInterval, &ticks);
    } else {
      renderer->renderGeographicGrid(*painter, mapPixelRect, bumpedScaleDetail, 
                                     gridDatum, gridInterval, &ticks);
    }
    painter->restore();
    painter->translate(mapRect.topLeft());

    pen.setColor(Qt::black);
    pen.setWidthF(borderWidth);
    painter->setPen(pen);
    QFont font;
    font.setPixelSize(9.0 / pointsPerInch * dpiX);
    painter->setFont(font);
    QFontMetrics fm(painter->fontMetrics());
    qreal tickLen = 5.0 / pointsPerInch * dpiX;
    foreach (const GridTick &tick, ticks) {
      switch (tick.side) {
      case Left: {
        qreal y = (tick.mapPos - mapPixelRect.top()) * bumpedScale;
        QPointF p1(-borderWidth, y);
        QPointF p2(-tickLen - borderWidth, y);
        painter->drawLine(p1, p2);

        painter->save();
        painter->translate(-tickLen - borderWidth - fm.height() / 2.0, y);
        painter->rotate(-90.0);
        QString s = gridFormatter->formatY(tick.gridPos);
        qreal w = fm.width(s);
        QRectF tr(-w / 2.0, -fm.height() / 2.0, w, fm.height());
        painter->drawText(tr, 0, s);
        painter->restore();
        break;
      }
      case Top: {
        qreal x = (tick.mapPos - mapPixelRect.left()) * bumpedScale;
        QPointF p1(x, -borderWidth);
        QPointF p2(x, -tickLen - borderWidth);
        painter->drawLine(p1, p2);

        painter->save();
        painter->translate(x, -tickLen - borderWidth - fm.height() / 2.0);
        QString s = gridFormatter->formatX(tick.gridPos);
        qreal w = fm.width(s);
        QRectF tr(-w / 2.0, -fm.height() / 2.0, w, fm.height());
        painter->drawText(tr, 0, s);
        painter->restore();

        break;
      }
      case Right: {
        qreal y = (tick.mapPos - mapPixelRect.top()) * bumpedScale;
        QPointF p1(mapRect.width() + borderWidth, y);
        QPointF p2(mapRect.width() + tickLen + borderWidth, y);
        painter->drawLine(p1, p2);

        painter->save();
        painter->translate(mapRect.width() + tickLen + borderWidth  + 
                           fm.height() / 2.0, y);
        painter->rotate(90.0);
        QString s = gridFormatter->formatY(tick.gridPos);
        qreal w = fm.width(s);
        QRectF tr(-w / 2.0, -fm.height() / 2.0, w, fm.height());
        painter->drawText(tr, 0, s);
        painter->restore();

        break;
      }
      case Bottom: {
        qreal x = (tick.mapPos - mapPixelRect.left()) * bumpedScale;
        QPointF p1(x, mapRect.height() + borderWidth);
        QPointF p2(x, mapRect.height() + tickLen + borderWidth);
        painter->drawLine(p1, p2);


        painter->save();
        painter->translate(x, mapRect.height() + tickLen + borderWidth);
        QString s = gridFormatter->formatX(tick.gridPos);
        qreal w = fm.width(s);
        QRectF tr(-w / 2.0, 0.0, w, fm.height());
        painter->drawText(tr, 0, s);
        painter->restore();

        break;
      }
      }
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
  if (gridFormatter) delete gridFormatter;
  if (utm) {
    gridFormatter = new UTMFormatter();
  } else {
    gridFormatter = new DMSFormatter();
  }
  computeGeometry();
}

void MapItem::hideGrid()
{
  gridEnabled = false;
  if (gridFormatter) { delete gridFormatter; gridFormatter = NULL; }
  computeGeometry();
}

void MapItem::mouseMoveEvent(QGraphicsSceneMouseEvent *ev)
{
  if (ev->buttons() & Qt::LeftButton) {
    QPointF before = itemToMap(ev->lastPos());
    QPointF after = itemToMap(ev->pos());
    QPointF delta = after - before;
    centerOn((mapPixelRect.center() - delta).toPoint());
  }
}

void MapItem::mousePressEvent(QGraphicsSceneMouseEvent *ev)
{
  QGraphicsItem::mousePressEvent(ev);
  setCursor(Qt::ClosedHandCursor);
  ev->accept();
}

void MapItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *ev)
{
  QGraphicsItem::mouseReleaseEvent(ev);
  setCursor(Qt::OpenHandCursor);
}


// Print scene ----------------------------------------------------------------
PrintScene::PrintScene(Map *m, MapRenderer *r, const QPrinter &printer)
{
  setBackgroundBrush(Qt::gray);

  paperRectItem = addRect(printer.paperRect(), QColor(Qt::black), Qt::white);
  pageRectItem = new QGraphicsRectItem(paperRectItem);
  pageRectItem->setPen(QColor(Qt::white));
  pageRectItem->setBrush(Qt::white);

  mapItem = new MapItem(m, r, pageRectItem);

  setPageMetrics(printer);

  connect(&r->getCache(), SIGNAL(tileLoaded()),
          this, SLOT(tileLoaded()));
}

void PrintScene::setPageMetrics(const QPrinter &printer)
{
  QRectF pageRect(printer.pageRect());
  paperRectItem->setRect(printer.paperRect());
  pageRectItem->setPos(pageRect.topLeft());
  QRectF mapRect(0, 0, printer.pageRect().width(), printer.pageRect().height());
  pageRectItem->setRect(mapRect);
  mapItem->setRect(mapRect);
  //  mapItem->setRect(printer.pageRect());
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
  return mapItem->loadTiles(-1.0);
}
