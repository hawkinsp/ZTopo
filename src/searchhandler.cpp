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
#include <QDebug>
#include <QHash>
#include "searchhandler.h"

void SearchResult::clear()
{
  featureName.clear();
  location = QPointF(0.0, 0.0);
  countyName.clear();
  cellName.clear();
  featureType.clear();
  elevation = 0;
}


QHash<QString, SearchElement> searchElements;
bool initSearchElements() {
  searchElements.insert("USGS", USGS);
  searchElements.insert("FEATURE_NAME", FeatureName);
  searchElements.insert("FEAT_LATITUDE_NMBR", Latitude);
  searchElements.insert("FEAT_LONGITUDE_NMBR", Longitude);
  searchElements.insert("CNTY_NAME", CountyName);
  searchElements.insert("CELL_NAME", CellName);
  searchElements.insert("FEATURE_TYPE", FeatureType);
  searchElements.insert("ELEVATION", Elevation);

  searchElements.insert("USGSLIST", IgnoredElement);
  searchElements.insert("FEATURE_ID_NMBR", IgnoredElement);
  searchElements.insert("STATE_EQUIVALENT_NAME", IgnoredElement);
  searchElements.insert("FEAT_LATITUDE_CHAR", IgnoredElement);
  searchElements.insert("FEAT_LONGITUDE_CHAR", IgnoredElement);
  return true;
}
static bool searchElementsInitialized = initSearchElements();


SearchHandler::SearchHandler()
  : QXmlDefaultHandler()
{
  currentElem = IgnoredElement;
  errors = false;
}

bool SearchHandler::startElement(const QString &, const QString &, 
                                 const QString &name, const QXmlAttributes &)
{
  assert(searchElementsInitialized);

  if (!searchElements.contains(name)) {
    qDebug() << "Unknown XML tag in search response " << name;
    return true;
  }

  currentElem = searchElements.value(name);
  elemData.clear();

  if (currentElem == USGS) {
    currentResult.clear();
  }
  
  return true;
}

bool SearchHandler::characters(const QString &ch)
{
  elemData.append(ch);
  return true;
}

bool SearchHandler::endElement(const QString &, const QString &, const QString &name)
{
  if (!searchElements.contains(name)) {
    qDebug() << "Unknown XML tag in search response " << name;
    return true;
  }
  SearchElement elem = searchElements.value(name);

  switch (elem) {
  case FeatureName:
    currentResult.featureName = elemData;
    break;
  case Latitude: {
    bool ok;
    qreal lat = elemData.toDouble(&ok);
    if (ok) {
      currentResult.location.setY(lat);
    } else {
      qWarning() << "Could not parse latitude " << elemData;
    }
    break;
  }
  case Longitude: {
    bool ok;
    qreal lon = elemData.toDouble(&ok);
    if (ok) {
      currentResult.location.setX(lon);
    } else {
      qWarning() << "Could not parse longitude " << elemData;
    }
    break;
  }
  case CountyName:
    currentResult.countyName = elemData;
    break;
  case CellName:
    currentResult.cellName = elemData;
    break;
  case FeatureType:
    currentResult.featureType = elemData;
    break;
  case Elevation: {
    bool ok;
    int elevation = elemData.toInt(&ok);
    if (ok) {
      currentResult.elevation = elevation;
    } else {
      qWarning() << "Could not parse elevation  " << elemData;
    }
    break;
  }

  case USGS:
    fResults.append(currentResult);
    break;

  case IgnoredElement:
    break;
  }
  return true;
}

bool SearchHandler::fatalError (const QXmlParseException & exception)
 {
     qWarning() << "Fatal error on line" << exception.lineNumber()
                << ", column" << exception.columnNumber() << ":"
                << exception.message();

     return false;
 }

