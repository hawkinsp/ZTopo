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

#ifndef COORDFORMATTER_H
#define COORDFORMATTER_H 1

#include <QPointF>
#include <QRegExp>
#include <QString>
#include "projection.h"

class CoordFormatter {
public:
  QRegExp &getRegExp() { return re; }
  QString &name() { return sName; }

  // Format a geographic point 
  virtual QString format(Datum, QPointF) = 0;

  // Format X and Y coordinates, e.g. UTM Y coordinate 1234 becomes 1234mN 
  virtual QString formatX(qreal) = 0;
  virtual QString formatY(qreal) = 0;

  // Given a datum d and "current" point c, parse a string into a geographic
  // coordinate
  virtual bool parse(Datum d, QPointF c, const QString &, QPointF &) = 0;
protected:
  QRegExp re;
  QString sName;
};

class DecimalDegreeFormatter : public CoordFormatter {
public:
  DecimalDegreeFormatter();
  virtual QString format(Datum, QPointF);
  virtual QString formatX(qreal);
  virtual QString formatY(qreal);

  virtual bool parse(Datum, QPointF, const QString &, QPointF &);
};

class DMSFormatter : public CoordFormatter {
public:
  DMSFormatter();
  virtual QString format(Datum, QPointF);
  virtual QString formatX(qreal);
  virtual QString formatY(qreal);

  virtual bool parse(Datum, QPointF, const QString &, QPointF &);
};

class UTMFormatter : public CoordFormatter {
public:
  UTMFormatter();
  virtual QString format(Datum, QPointF);
  virtual QString formatX(qreal);
  virtual QString formatY(qreal);

  virtual bool parse(Datum, QPointF, const QString &, QPointF &);
};

#endif
