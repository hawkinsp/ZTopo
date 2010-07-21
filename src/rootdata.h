#ifndef ROOTDATA_H
#define ROOTDATA_H 1

#include <QIODevice>
#include <QMap>
#include <QString>
class Map;

QMap<QString, Map *> readRootData(QIODevice &d);

#endif
