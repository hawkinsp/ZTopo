#include <QObject>
#include <QStringBuilder>
#include "coordformatter.h"
#include "consts.h";


DecimalDegreeFormatter::DecimalDegreeFormatter()
{
  sName = QObject::trUtf8("DDD.MMM\xc2\xb0");

  QString pattern(QString::fromUtf8("(-?)(\\d+)(.\\d+)?\xc2\xb0?([ nNsS]) *(-?)(\\d+)(.\\d+)?\xc2\xb0?([ wWeE]?)"));
  re.setPattern(pattern);
}
bool DecimalDegreeFormatter::parse(Datum, QPointF, const QString &s, QPointF &p)
{
  if (!re.exactMatch(s)) return false;

  QString latMinus = re.cap(1);
  QString latVal = re.cap(2) + re.cap(3);
  QString latSuffix = re.cap(4);

  QString lonMinus = re.cap(5);
  QString lonVal = re.cap(6) + re.cap(7);
  QString lonSuffix = re.cap(8);

  qreal lat = latVal.toDouble();
  if (latMinus == "-") lat *= -1.0;
  if (latSuffix[0] == 's' || latSuffix[0] == 'S') lat *= -1.0;

  qreal lon = lonVal.toDouble();
  if (lonMinus == "-") lon *= -1.0;
  if (lonSuffix[0] == 'w' || lonSuffix[0] == 'W') lon *= -1.0;

  
  p = QPointF(lon, lat);
  return true;
}

QString DecimalDegreeFormatter::format(Datum, QPointF g)
{
  QString s = 
    QString::number(fabs(g.y()), 'f', 5) % degree % (g.y() >= 0 ? 'N' : 'S')
    % " " % 
    QString::number(fabs(g.x()), 'f', 5) % degree % (g.x() >= 0 ? 'E' : 'W');
  return s;
}

DMSFormatter::DMSFormatter()
{
  sName = QObject::trUtf8("DDD\xc2\xb0MM'SS\"");
  QString pattern(QString::fromUtf8("(-?)(\\d+)[\xc2\xb0 ] *(\\d+)[' ] *(\\d+)\"? *([nNsS]?) *(-?)(\\d+)[\xc2\xb0 ] *(\\d+)[' ] *(\\d+)\"? *([wWeE]?)"));
  re.setPattern(pattern);
}

bool DMSFormatter::parse(Datum, QPointF, const QString &s, QPointF &p)
{
  if (!re.exactMatch(s)) return false;

  QString latMinus = re.cap(1);
  QString latDeg = re.cap(2);
  QString latMin = re.cap(3);
  QString latSec = re.cap(4);
  QString latSuffix = re.cap(5);

  QString lonMinus = re.cap(6);
  QString lonDeg = re.cap(7);
  QString lonMin = re.cap(8);
  QString lonSec = re.cap(9);
  QString lonSuffix = re.cap(10);

  qreal lat = latDeg.toDouble() + latMin.toDouble() / 60.0 + 
    latSec.toDouble() / 3600.0;
  if (latMinus == "-") lat *= -1.0;
  if (latSuffix[0] == 's' || latSuffix[0] == 'S') lat *= -1.0;

  qreal lon = lonDeg.toDouble() + lonMin.toDouble() / 60.0 + 
    lonSec.toDouble() / 3600.0;
  if (lonMinus == "-") lon *= -1.0;
  if (lonSuffix[0] == 'w' || lonSuffix[0] == 'W') lon *= -1.0;

  p = QPointF(lon, lat);
  return true;
}

QString DMSFormatter::format(Datum, QPointF g)
{
  qreal x = fabs(g.x()), y = fabs(g.y());
  int yd = int(y);
  int ym = int(60.0 * (y - yd));
  int ys = int(3600.0 * (y - qreal(yd) - ym / 60.0));
  int xd = int(x);
  int xm = int(60.0 * (x - xd));
  int xs = int(3600.0 * (x - qreal(xd) - xm / 60.0));
  
  QString s = 
    QString::number(yd) % degree % 
    QString::number(ym).rightJustified(2, '0') % '\'' % 
    QString::number(ys).rightJustified(2, '0') % '"' 
    % (g.y() >= 0 ? 'N' : 'S') % " " % 
    QString::number(xd) % degree % 
    QString::number(xm).rightJustified(2, '0') % '\'' % 
    QString::number(xs).rightJustified(2, '0') % '"' 
    % (g.x() >=0 ? 'E' : 'W');
  return s;
}

UTMFormatter::UTMFormatter()
{
  sName = QObject::tr("UTM");
  QString pattern("(\\d{1,2}[a-zA-Z]?)? *(\\d+)m?[eE]? +(\\d+)m?[nN]?");
  re.setPattern(pattern);
}

bool UTMFormatter::parse(Datum d, QPointF c, const QString &s, QPointF &p)
{
  if (!re.exactMatch(s)) return false;

  QString easting, northing;
  int zone;
  if (re.captureCount() == 2) {
    zone = UTM::bestZone(c).zone;
    easting = re.cap(1);
    northing = re.cap(2);
  } else {
    QString z = re.cap(1);
    easting = re.cap(2);
    northing = re.cap(3);

    if (!z[z.size() - 1].isDigit()) z = z.left(z.size() - 1);
    zone = z.toInt();
    if (zone < 1 || zone > UTM::numZones) return false;
  }

  QPointF q(easting.toDouble(), northing.toDouble());
  Projection *pj = UTM::getZoneProjection(d, zone);
  p = Geographic::getProjection(d)->transformFrom(pj, q);
  return true;
}

QString UTMFormatter::format(Datum d, QPointF g)
{
  UTM::Zone z = UTM::bestZone(g);
  Projection *pjUTM = UTM::getZoneProjection(d, z.zone);
  QPointF p = pjUTM->transformFrom(Geographic::getProjection(d), g);

  QString s = QString::number(z.zone) % z.band % " " %
    QString::number(p.x(), 'f', 0) % "mE " %
    QString::number(p.y(), 'f', 0) % "mN";

  return s;
}
