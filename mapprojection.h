#ifndef MAPPROJECTION_H
#define MAPPROJECTION_H 1

#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QTransform>
#include "projection.h"

extern const char *californiaMapProjection;
extern const QSize californiaMapSize;
void californiaProjToMapTransform(QTransform &t);

#endif
