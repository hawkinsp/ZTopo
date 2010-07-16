#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cmath>
#include <iostream>
#include <gdal_priv.h>
#include <ogr_spatialref.h>
#include <QByteArray>
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include "consts.h"
#include "map.h"
#include "mapprojection.h"

using namespace std;


static const qreal quadSize = 0.125;
static const int numSidePoints = 4;

// Return the boundary of a quad in NAD27 geographical coordinates
void quadBoundary(QFileInfo filename,  QRectF &bounds, int &series)
{
  QByteArray name = filename.baseName().toLatin1();
  QSizeF size;
  if (name.size() < 8) goto bad;
  switch (name[0]) {
  case 'o':
    // 1:24000 series
    size = QSizeF(quadSize, -quadSize); // 7.5' quads
    series = 0;
    break;
  case 'f':
    // 1:100000 series
    size = QSizeF(1.0, -0.5); // 60' x 30' quads
    series = 1;
    break;
  default:
    goto bad;
  }
  {
    if (!isdigit(name[1]) || !isdigit(name[2])) goto bad;
    qreal y = qreal(10 * (name[1] - '0') + (name[2] - '0'));
    
    if (!isdigit(name[3]) || !isdigit(name[4]) || !isdigit(name[5])) goto bad;
    qreal x = qreal(100 * (name[3] - '0') + 10 * (name[4] - '0') + (name[5] - '0'));
    
    if (!isalpha(name[6]) || !isdigit(name[7])) goto bad;
    int suby = tolower(name[6]) - 'a';
    int subx = name[7] - '1';
    
    x += quadSize * subx + size.width();
    y += quadSize * suby - size.height();
    
    bounds = QRectF(QPointF(-x, y), size);
  } 
  return;
 bad:
  fprintf(stderr, "Bad quad identifier '%s'\n", name.data());
  exit(-1);
}

void addPolygonSide(OGRCoordinateTransformation *ct, 
                    QPolygonF &p, QPointF from, QPointF to)
{
  for (int i = 0; i < numSidePoints; i++) {
    double pos = double(i) / double(numSidePoints);
    double x = (1.0 - pos) * from.x() + pos * to.x();
    double y = (1.0 - pos) * from.y() + pos * to.y();
    ct->Transform(1, &x, &y);
    p << QPointF(x, y);
  }
}

// Transform quad boundary into projection space
QPolygonF projectQuadBoundary(OGRCoordinateTransformation *ct, QRectF quad)
{
  QPolygonF p;
  addPolygonSide(ct, p, quad.topLeft(), quad.topRight());
  addPolygonSide(ct, p, quad.topRight(), quad.bottomRight());
  addPolygonSide(ct, p, quad.bottomRight(), quad.bottomLeft());
  addPolygonSide(ct, p, quad.bottomLeft(), quad.topLeft());
  return p;
}

int main(int argc, char **argv)
{
  MapProjection mapProj;
  Map map(&mapProj);

  if (argc < 2) {
    fprintf(stderr, "import file\n");
    return -1;
  }
  GDALAllRegister();

  QFileInfo filename(argv[1]);

  GDALDataset *ds = (GDALDataset *)GDALOpen(filename.filePath().toLatin1().data(),
                                            GA_ReadOnly);
  const char *proj = ds->GetProjectionRef();

  // Read projection
  OGRSpatialReference srs;
  srs.importFromWkt((char **)&proj);

  // Read geotransform
  double geotransform[6];
  ds->GetGeoTransform(geotransform);

  QPointF fProjTopLeft = QPointF(geotransform[0], geotransform[3]);
  QSizeF fPixelSize = QSizeF(geotransform[1], geotransform[5]);

  // Check Y pixel size is the negation of the X pixel size
  if (fabs(fPixelSize.width() + fPixelSize.height()) >= epsilon) {
    fprintf(stderr, "Invalid pixel sizes\n");
    return -1;
  }

  if (fabs(geotransform[2]) >= epsilon || fabs(geotransform[4]) >= epsilon) {
    fprintf(stderr, "Geotransform has shear component\n");
    return -1;
  }

  QSizeF fProjSize = QSizeF(qreal(ds->GetRasterXSize()) * fPixelSize.width(),
                            qreal(ds->GetRasterYSize()) * fPixelSize.height());
  QRectF projRect = QRectF(fProjTopLeft, fProjSize);

  // Rectangle covered by the entire map image
  QRectF mapRect = mapProj.projToMap(projRect);


  // Compute the initial scale factor and tile level
  QSizeF mapPixelSize = mapProj.pixelSize();
  QSizeF scale(fPixelSize.width() / mapPixelSize.width(),
               fPixelSize.height() / mapPixelSize.height());
  int maxLevel = map.maxLevel();

  while (scale.width() >= 1.1) {
    maxLevel--;
    scale /= 2.0;
  }

  //  printf("scale %lf %lf\n", scale.width(), scale.height());
  //return 0;

  // Compute the quad boundary
  OGRSpatialReference nad27;
  nad27.SetWellKnownGeogCS("NAD27");

  OGRCoordinateTransformation *ct = OGRCreateCoordinateTransformation(&nad27, &srs);
  if (ct == NULL) {
    fprintf(stderr, "Couldn't create coordinate transformation\n");
    return -1;
  }
  QRectF quad;
  int series;
  quadBoundary(filename, quad, series);
  printf("Series: %d %s\n", series, map.layer(series).name.toLatin1().data());
  printf("Quad: %lf %lf %lf %lf\n", quad.left(), quad.top(), quad.right(), 
         quad.bottom());

  QPolygonF projectedQuad = projectQuadBoundary(ct, quad);

  // Rectangle covered by the quadrangle data
  QRectF mapBoundingRect = mapProj.projToMap(projectedQuad.boundingRect());

  /*printf("top left %lf %lf\n", fProjTopLeft.x(), fProjTopLeft.y());*/
  /*for (int i = 0; i< projectedQuad.size(); i++) {
    printf("quad proj %lf %lf\n", projectedQuad[i].x(), projectedQuad[i].y());
    }*/



  // Read the image
  QImage drg(filename.filePath());
  QDir dir;

  for (int level = maxLevel; level >= 0; level--, scale /= 2.0) {
    QSizeF rasterSizeF = QSizeF(ds->GetRasterXSize() * scale.width(),
                               ds->GetRasterYSize() * scale.height());
    QSize rasterSize = rasterSizeF.toSize();
    printf("level %d size %dx%d\n", level, rasterSize.width(), rasterSize.height());
    QImage drgScaled = drg.scaled(rasterSize, Qt::IgnoreAspectRatio,
                                   Qt::SmoothTransformation);

    QPolygonF imageQuad;
    for (int i = 0; i < projectedQuad.size(); i++) {
      QPointF p = projectedQuad[i] - fProjTopLeft;
      imageQuad << QPointF(p.x() * scale.width() / fPixelSize.width(), 
                           p.y() * scale.height() / fPixelSize.height());
    }

    QSizeF baseTileSize(map.baseTileSize(), map.baseTileSize());
    int tileSize = map.tileSize(level);
    /*    printf("map bounding rect %lf %lf %lf %lf\n", mapBoundingRect.left(), mapBoundingRect.top(), 
          mapBoundingRect.right(), mapBoundingRect.bottom());*/

    QRectF tileRectF = QRectF(mapBoundingRect.bottomLeft() / qreal(tileSize),
                              mapBoundingRect.topRight() / qreal(tileSize));
    QRect tileRect(QPoint(int(floor(tileRectF.left())), int(floor(tileRectF.top()))),
                   QPoint(int(ceil(tileRectF.right())), 
                          int(ceil(tileRectF.bottom()))));
    /*    printf("tile rect %d %d %d %d\n", tileRect.left(), tileRect.top(), 
          tileRect.right(), tileRect.bottom());*/
    for (int tileY = tileRect.top(); tileY <= tileRect.bottom(); tileY++) {
      for (int tileX = tileRect.left(); tileX <= tileRect.right(); tileX++) {
        Tile key(tileX, tileY, level, series);

        QPointF tileTopLeft(tileX * tileSize, tileY * tileSize);
        QPointF deltaTileTopLeft = tileTopLeft - mapRect.topLeft();
        qreal s = qreal(1 << (map.maxLevel() - level));
        QPointF topLeft(deltaTileTopLeft.x() / s,
                        deltaTileTopLeft.y() / s);
        QRectF src(topLeft, baseTileSize);
        
        QFileInfo tilePath(map.tilePath(key));
        dir.mkpath(tilePath.path());
        QImage image;
        if (tilePath.exists()) {
          QImage idxImage = QImage(tilePath.filePath());
          image = idxImage.convertToFormat(QImage::Format_RGB32);
        } else {
          image = QImage(256, 256, QImage::Format_RGB32);
          image.setColorTable(drg.colorTable());
          image.fill(QColor(255, 255, 255).rgb());
        }

        QPainterPath pp;
        pp.addPolygon(imageQuad.translated(-topLeft));
        QPainter painter;
        painter.begin(&image);
        painter.setClipPath(pp);
        painter.drawImage(-topLeft, drgScaled);
        // painter.setPen(QPen(QColor(0, 0, 255)));
        // painter.drawPath(pp);
        painter.end();

        QImage tile = image.convertToFormat(QImage::Format_Indexed8, 
                                            drg.colorTable(), Qt::ThresholdDither);
        tile.save(tilePath.filePath(), "png");
        // printf("tile: %s\n", tilePath.filePath().toLatin1().data());
      }
    }

  }
  return 0;
}
