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

#include <cassert>
#include <iostream>
#include <QDebug>
#include <QStringBuilder>
#include "projection.h"
#include "consts.h"

const char *datumName(Datum d)
{
  switch (d) {
  case NAD27: return "NAD27";
  case NAD83: return "NAD83";
  default: qFatal("Invalid map datum in datumName"); abort();
  }
}

Datum parseDatum(const QString &d)
{
  if (d == QString("NAD27")) return NAD27;
  else if (d == QString("NAD83")) return NAD83;
  else { qFatal("Unknown map datum in parseDatum"); abort(); }
}

Projection::Projection(const char *proj, qreal s)
  : scale(s)
{
  pj = pj_init_plus(proj);
  if (!pj) {
    std::cerr << "Could not create projection " << proj << std::endl;
    exit(-1);
  }
  initString = proj;
}

Projection::Projection(QString proj, qreal s)
  : scale(s)
{
  pj = pj_init_plus(proj.toLatin1().data());
  if (!pj) {
    std::cerr << "Could not create projection " << proj.toLatin1().data() 
              << std::endl;
    exit(-1);
  }
  initString = proj;
}

Projection::~Projection()
{
  pj_free(pj);
}

QPointF Projection::transformFrom(Projection *pjOther, QPointF p)
{
  projUV uv;
  double z = 0.0;
  uv.u = p.x() / pjOther->scale;
  uv.v = p.y() / pjOther->scale;
  
  pj_transform(pjOther->pj, pj, 1, 0, &uv.u, &uv.v, &z);
  return QPointF(uv.u, uv.v) * scale;
}

QPolygonF Projection::transformFrom(Projection *pjOther, QPolygonF in)
{
  QPolygonF out;

  for (int i = 0; i < in.size(); i++) {
    out << transformFrom(pjOther, in[i]);
  }
  return out;
}


QString Projection::toString(QPointF p) {
  return QString::number(p.x(), 'f', 3) % ", " % QString::number(p.y(), 'f', 3);
}

namespace Geographic {
  static Projection *pjNAD27 = NULL, *pjNAD83 = NULL;

  Projection *getProjection(Datum d)
  {
    switch (d) {
    case NAD27:
      if (pjNAD27) return pjNAD27;
      pjNAD27 = new Projection("+proj=latlong +datum=NAD27", degreesPerRadian);
      return pjNAD27;
    case NAD83: 
      if (pjNAD83) return pjNAD83;
      pjNAD83 = new Projection("+proj=latlong +datum=NAD83", degreesPerRadian);
      return pjNAD83;
    default: return NULL;
    }
  }
}


namespace UTM {
  static Projection *projections[numDatums][numZones] = { };

  void zoneLongitudeRange(int zone, int &min, int &max) {
    min = (zone - 1) * 6 - 180;
    max = zone * 6 - 180;
  }

  Zone bestZone(QPointF p)
  {
    qreal lon = p.x(), lat = p.y();

    assert(lon >= -180.0 && lon < 180.0);
    assert(lat >= -90.0 && lat <= 90.0);
    
    // Compute Zone
    qreal x = lon + 180.0;
    int zone = int(x/6.0) + 1;
    
    // Zone 32 is special
    if (lat >= 56.0 && lat < 64.0 && lon >= 3.0 && lon < 12.0)
      zone = 32;
    
    // Special zones for Svalbard
    if (lat >= 72.0 && lat < 84.0) {
      if (lon >= 0.0 && lon < 9.0) {
        zone = 31;
      } else if (lon >= 9.0 && lon < 21.0) {
        zone = 33;
      } else if (lon >= 21.0 && lon < 33.0) {
        zone = 35;
      } else if (lon >= 33.0 && lon < 42.0) {
        zone = 37;
      }
    }
    
    // Compute Band
    char band;
    if (lat < -80.0 || lat > 84.0) {
      band = 'Z';
    } else if (lat >= 72.0) {
      band = 'Z';
    } else if (lat >= -80.0 && lat < -32.0) {
      int y = int((lat + 80.0)/8.0);
      band = 'C' + y;
    } else if (lat >= -32.0 && lat < 8.0) {
      int y = int((lat + 32.0)/8.0);
      band = 'J' + y;
    } else {
      int y = int((lat - 8.0)/8.0);
      band = 'P' + y;
    }
    
    UTM::Zone z;
    z.zone = zone;
    z.band = band;
    z.isNorth = (lat >= 0.0);
    return z;
  }

  Projection *getZoneProjection(Datum d, int zone)
  {
    if (!projections[d][zone - 1]) {
      QString s = "+proj=utm +zone=" % QString::number(zone) 
        % " +datum=" % datumName(d);
      projections[d][zone - 1] = new Projection(s);
    }

    return projections[d][zone - 1];
  }

  Projection *getBestZoneProjection(Datum d, QPointF p)
  {
    return getZoneProjection(d, bestZone(p).zone);
  }
  
}
