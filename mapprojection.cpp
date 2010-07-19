#include "mapprojection.h"
#include "consts.h"
//#include "ogr_spatialref.h"
#include <iostream>

const char *californiaMapProjection =
  "+proj=aea +lat_1=34 +lat_2=40.5 +lat_0=0 +lon_0=-120 +x_0=0 +y_0=-4000000 +ellps=GRS80 +datum=NAD83 +units=m +no_defs";

void californiaProjToMapTransform(QTransform &t)
{
  t = QTransform();
  int mapResolution = 400;
  int mapScale = 24000;
  float pixelSize = mapScale * metersPerInch / (float)mapResolution;
  t.scale(1/pixelSize, -1/pixelSize);
  // Origin of the map is -525000, 545000 in projection space
  t.translate(525000.0, -545000.0);
}

const QSize californiaMapSize(675000 + 525000, 545000 + 655000);

