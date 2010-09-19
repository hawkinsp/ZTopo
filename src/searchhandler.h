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

#ifndef SEARCHHANDLER_H
#define SEARCHHANDLER_H 1

#include <QPointF>
#include <QXmlDefaultHandler>

enum SearchElement {
  USGS,
  FeatureName,
  Latitude,
  Longitude,
  CountyName,
  CellName,
  FeatureType,
  Elevation,
  IgnoredElement
};

struct SearchResult {
  QString featureName;
  QPointF location;
  QString countyName;
  QString cellName;
  QString featureType;
  int elevation;
  
  void clear();
};


class SearchHandler : public QXmlDefaultHandler
{
public:
  SearchHandler();

  virtual bool startElement(const QString & namespaceURI, const QString & localName,
                            const QString & qName, const QXmlAttributes & atts);
  virtual bool characters(const QString & ch);
  virtual bool endElement(const QString & namespaceURI, const QString & localName, 
                          const QString & qName) ;
  virtual bool fatalError(const QXmlParseException & exception);

  bool hasErrors() const { return errors; }
  const QList<SearchResult> &results() const { return fResults; }

private:
  SearchResult currentResult;
  SearchElement currentElem;
  QString elemData;
  QList<SearchResult> fResults;
  bool errors;
};

#endif // SEARCHHANDLER_H
