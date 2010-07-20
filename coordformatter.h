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

  virtual QString format(Datum, QPointF) = 0;

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
  virtual bool parse(Datum, QPointF, const QString &, QPointF &);
};

class DMSFormatter : public CoordFormatter {
public:
  DMSFormatter();
  virtual QString format(Datum, QPointF);
  virtual bool parse(Datum, QPointF, const QString &, QPointF &);
};

class UTMFormatter : public CoordFormatter {
public:
  UTMFormatter();
  virtual QString format(Datum, QPointF);
  virtual bool parse(Datum, QPointF, const QString &, QPointF &);
};

#endif
