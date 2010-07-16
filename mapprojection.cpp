#include "mapprojection.h"
#include "consts.h"
//#include "ogr_spatialref.h"
#include <iostream>


MapProjection::MapProjection()
{
  //  projSpace.importFromProj4("+proj=aea +lat_1=34 +lat_2=40.5 +lat_0=0 +lon_0=-120 +x_0=0 +y_0=-4000000 +ellps=GRS80 +datum=NAD83 +units=m +no_defs");
 
  projOrigin = QPointF(-460000.0, 460000.0);
  projExtent = QPointF(640000.0, -640000.0);
  mapResolution = 400;
  mapScale = 24000;

  float pixelSize = mapScale * metersPerInch / (float)mapResolution;
  mapPixelSize = QSizeF(pixelSize, -pixelSize);
  // std::cout << "pixel size" << pixelSize << std::endl;
}

QSize MapProjection::mapSize()
{
  QPoint p = projToMap(projExtent).toPoint();
  return QSize(p.x(), p.y());
}

QPointF MapProjection::mapToProj(QPointF m)
{
  return QPointF(m.x() * mapPixelSize.width() + projOrigin.x(),
                 m.y() * mapPixelSize.height() + projOrigin.y());
}

QPointF MapProjection::projToMap(QPointF p)
{
  return QPointF((p.x() - projOrigin.x()) / mapPixelSize.width(),
                 (p.y() - projOrigin.y()) / mapPixelSize.height());
}

QRectF MapProjection::projToMap(QRectF p)
{
  return QRectF(projToMap(p.topLeft()), projToMap(p.bottomRight()));
}
