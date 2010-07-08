#ifndef MAPPROJECTION_H
#define MAPPROJECTION_H 1

#include <QPointF>
//#include "ogr_spatialref.h"

class MapProjection
{
 public:
  MapProjection();

  // Convert a point from map space to projection space
  QPointF mapToProj(QPointF m);

  // Convert a point from projection space to map space
  QPointF projToMap(QPointF p);

  QPoint mapSize();

  // Size of a map pixel in meters
  float pixelSize() { return mapPixelSize.x(); }


 private:
  QPointF projOrigin; // Origin of the map space in projection space
  QPointF projExtent; // Extent of the map space in projection space

  int mapResolution; // Resolution of the map in pixels per inch
  int mapScale;      // Scale of the map (1:mapScale)

  QPointF mapPixelSize; // Size of a map pixel in projection space

  //  OGRSpatialReference projSpace;

};

#endif
