#ifndef PROJECTION_H
#define PROJECTION_H 1

#include <QPointF>
#include <QPolygonF>
#include <QString>
#include <proj_api.h>


enum Datum {
  NAD27 = 0,
  NAD83 = 1,
  numDatums = 2
};
const char *datumName(Datum d);

class Projection {
public:
  Projection(const char *proj, qreal scale = 1.0);
  Projection(QString proj, qreal scale = 1.0);
  ~Projection();

  QString toString(QPointF p);

  QPointF transformFrom(Projection *old, QPointF p);
  QPolygonF transformFrom(Projection *old, QPolygonF p);

private:
  projPJ pj;
  qreal scale;
};

namespace Geographic {
  Projection *getProjection(Datum d);
}


namespace UTM {
  class Zone {
  public:
    int zone;
    char band;
    bool isNorth;
  };

  static const int numZones = 60;

  // Compute minimum and maximum longitudes for a zone
  void zoneLongitudeRange(int zone, int &min, int &max);
  
  // Find the best UTM zone for a given geographic coordinate
  Zone bestZone(QPointF latlong);

  // Get projection for a zone
  Projection *getZoneProjection(Datum d, int z);

  // Get the best zone projection for a coordinate
  Projection *getBestZoneProjection(Datum d, QPointF latlong);
}

#endif
