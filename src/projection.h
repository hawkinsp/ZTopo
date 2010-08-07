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
Datum parseDatum(const QString &);

class Projection {
public:
  Projection(const char *proj, qreal scale = 1.0);
  Projection(QString proj, qreal scale = 1.0);
  ~Projection();

  QString toString(QPointF p);

  QPointF transformFrom(Projection *old, QPointF p);
  QPolygonF transformFrom(Projection *old, QPolygonF p);

private:
  QString initString;
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
