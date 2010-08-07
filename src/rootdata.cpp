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

#include <QFile>
#include <QIODevice>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QVariant>
#include <qjson/parser.h>
#include "rootdata.h"
#include "map.h"

static const QString rootUrl = "http://ztopo.s3.amazonaws.com/maps/root.json";
static const QLatin1String defaultRootDataName(":/config/root.json");

RootData::RootData(QNetworkAccessManager *m)
  : manager(m)
{
  QNetworkReply *reply;
  QNetworkRequest req;

  if (manager) {
    req.setUrl(QUrl(rootUrl));
    // Try to fetch the root data from the cache
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, 
                     QNetworkRequest::AlwaysCache);
    reply = manager->get(req);
  }

  if (manager && reply->isFinished() && reply->error() == QNetworkReply::NoError) {
    QByteArray data = reply->readAll();
    parseRootData(data);
  } else {

    QFile defaultRootData(defaultRootDataName);
    if (!defaultRootData.exists()) {
      qFatal("Cannot find map root data '%s'", defaultRootDataName.latin1());
    }
    defaultRootData.open(QIODevice::ReadOnly);
    QByteArray data = defaultRootData.readAll();
    defaultRootData.close();

    parseRootData(data);

    // Try fetching updated root data from the network for the next run.
    if (manager) {
      req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, 
                       QNetworkRequest::PreferNetwork);
      manager->get(req);
    }
  }
}

void RootData::parseRootData(QByteArray data) {
  QJson::Parser parser;
  bool ok;
 
  QVariantMap result = parser.parse(data, &ok).toMap();
  if (!ok) {
    qFatal("An error occurred while parsing the root map index file");
  }

  QVariantMap version = result["currentVersion"].toMap();
  fMajorVersion = version["major"].toInt();
  fMinorVersion = version["minor"].toInt();
  fGnisUrl = result["gnisURL"].toString();
  fHomePageUrl = result["homePageURL"].toString();
  foreach (QVariant v, result["maps"].toList()) {
    Map *map = Map::fromVariant(v);
    fMaps[map->id()] = map;
  }
}
