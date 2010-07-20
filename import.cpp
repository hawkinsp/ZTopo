#include <cassert>
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
#include <QMap>
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

#include <ogrsf_frmts.h>

using namespace std;


// Size of a 7.5' quad in degrees
static const qreal quadSize = 0.125; 

// Geographic quadrangles aren't rectangles in projection space; we described
// quadrangles in projection space as polygons with numSidePoints points per
// quadrangle side.
static const int numSidePoints = 4;  

// By how many DRG pixels is a quad permitted to overhang an image boundary before
// we conclude that we have some sort of misalignment?
static const qreal drgQuadSlackPixels = 5;

class Quad {
public:
  Quad() { }
  Quad(int s, QString i, QString n, QPolygonF b) 
  : series(s), id(i), name(n), boundary(b) { }
  int series;
  QString id;
  QString name;

  // Quad boundary in projection coordinates
  QPolygonF boundary;
};

// Not all quads are aligned to the regular quadrangle grid. Some are offset
// or irregular sizes. We keep a list of exceptions.
QMap<QString, Quad> quads;


void readQuadIndex(int series, QString file, QString layerName, Projection *pj)
{
  OGRDataSource       *ds = OGRSFDriverRegistrar::Open(file.toLatin1().data(), false);
  if (!ds) {
    fprintf(stderr, "Could not open quad index '%s'.\n", file.toLatin1().data());
    exit(-1);
  }

  OGRLayer  *layer;
  layer = ds->GetLayerByName(layerName.toLatin1().data());
  if (!layer) {
    fprintf(stderr, "Could not read layer '%s'.\n", layerName.toLatin1().data());
    exit(-1);
  }

  OGRSpatialReference *srs = layer->GetSpatialRef();
  if (!srs) {
    fprintf(stderr, "Missing spatial reference for layer '%s'.\n", 
            layerName.toLatin1().data());
    exit(-1);
  }
  char *proj = NULL;
  srs->exportToProj4(&proj);
  if (!proj) {
    fprintf(stderr, "Error computing PROJ4 spatial reference for layer '%s'.\n", 
            layerName.toLatin1().data());
    exit(-1);
  }
  Projection pjIndex(proj);
  CPLFree(proj);


  layer->ResetReading();
  OGRFeatureDefn *def = layer->GetLayerDefn();

  int idFieldNr = def->GetFieldIndex("ID");
  int nameFieldNr = def->GetFieldIndex("NAME");
  if (idFieldNr < 0 || nameFieldNr < 0) {
    fprintf(stderr, "Missing index layer fields.\n");
    exit(-1);
  }


  OGRFeature *f;
  while ((f = layer->GetNextFeature()) != NULL) {
    QString id(f->GetFieldAsString(idFieldNr));
    QString name(f->GetFieldAsString(nameFieldNr));

    //    printf("Quad id: %s; name: %s\n", id.toLatin1().data(), name.toLatin1().data());

    OGRGeometry *g;
    g = f->GetGeometryRef();
    if (g != NULL && wkbFlatten(g->getGeometryType()) == wkbPolygon) {
      OGRPolygon *p = (OGRPolygon *)g;
      OGRLinearRing *r = p->getExteriorRing();
      if (!r) {
        fprintf(stderr, "Quad has no exterior polygon ring %s\n", 
                id.toLatin1().data());
        continue;
      }

      int size = r->getNumPoints();
      QPolygonF boundary;
      for (int i = 0; i < size; i++) {
        OGRPoint p;
        r->getPoint(i, &p);
        boundary << QPointF(p.getX(), p.getY());
      }

      QPolygonF projBoundary = pj->transformFrom(&pjIndex, boundary);
      
      quads[id] = Quad(series, id, name, projBoundary);
    } else {
      fprintf(stderr, "Missing or invalid geometry for quad %s\n", 
              id.toLatin1().data());
    }
  }

  OGRDataSource::DestroyDataSource(ds);
}

void makePolygon(QPolygonF &p, const QRectF &r)
{
  QPolygonF rp(r);
  for (int a = 0; a < rp.size(); a++) {
    int b = (a + 1 == rp.size()) ? 0 : a + 1;
    for (int i = 0; i < numSidePoints; i++) {
      double pos = double(i) / double(numSidePoints);
      double x = (1.0 - pos) * rp[a].x() + pos * rp[b].x();
      double y = (1.0 - pos) * rp[a].y() + pos * rp[b].y();
      p << QPointF(x, y);
    }
  }
}


// Return the boundary of a quad in projection space, based on the
// first 8 bytes of the filename.
// Reference: http://topomaps.usgs.gov/drg/drg_name.html
void getQuadInfo(QFileInfo filename, Projection *pj, Quad &quad)
{
  QString baseName = filename.baseName();
  QByteArray name = filename.baseName().toLatin1();
  QSizeF size;
  int series;

  if (baseName.size() != 8) goto bad;

  if (quads.contains(baseName)) {
    quad = quads[baseName];
  } else {
    printf("Quad not found in index, using defaults\n");
    switch (name[0]) {
    case 'o':
      // 1:24000 series
      size = QSizeF(quadSize, quadSize); // 7.5' quads
      series = 1;
      break;
    case 'f':
      // 1:100000 series
      size = QSizeF(1.0, 0.5); // 60' x 30' quads
      series = 0;
      break;
    default:
      goto bad;
    }

    if (!isdigit(name[1]) || !isdigit(name[2])) goto bad;
    qreal y = qreal(10 * (name[1] - '0') + (name[2] - '0'));
    
    if (!isdigit(name[3]) || !isdigit(name[4]) || !isdigit(name[5])) goto bad;
    qreal x = qreal(100 * (name[3] - '0') + 10 * (name[4] - '0') + (name[5] - '0'));
    
    if (!isalpha(name[6]) || !isdigit(name[7])) goto bad;
    int suby = tolower(name[6]) - 'a';
    int subx = name[7] - '1';
    
    x += quadSize * subx + size.width();
    y += quadSize * suby;
    
    QRectF r(QPointF(-x, y), size);
    QPolygonF geoBounds;
    makePolygon(geoBounds, r);
    QPolygonF projBounds = 
      pj->transformFrom(Geographic::getProjection(NAD27), geoBounds);
    quad = Quad(series, baseName, baseName, projBounds);
  } 
  return;
 bad:
  fprintf(stderr, "Bad quad identifier '%s'\n", name.data());
  exit(-1);
}

int main(int argc, char **argv)
{
  if (argc < 2) {
    fprintf(stderr, "import <file.tif>\n");
    return -1;
  }

  OGRRegisterAll();
  GDALAllRegister();

  QTransform tr;

  Projection *pjGeo = Geographic::getProjection(NAD27);
  Projection *pj = new Projection(californiaMapProjection);
  californiaProjToMapTransform(tr);

  Map map(NAD27, pj, tr, californiaMapSize);


  readQuadIndex(1, "/Users/hawkinsp/geo/drg/index/drg24.shp", "drg24", pj);

  // Index information seems buggy for these quads -- just use the regular grid.
  quads.remove("o37122g4"); // San Francisco North
  quads.remove("o37122g5"); // Point Bonita


  QFileInfo filename(argv[1]);

  GDALDataset *ds = (GDALDataset *)GDALOpen(filename.filePath().toLatin1().data(),
                                            GA_ReadOnly);
  if (!ds) {
    fprintf(stderr, "ERROR: Could not open dataset.\n");
    return -1;
  }

  const char *proj = ds->GetProjectionRef();

  Quad quad;
  getQuadInfo(filename, pj, quad);

  // Read projection
  OGRSpatialReference srs;
  srs.importFromWkt((char **)&proj);

  // Size of the DRG
  QSize drgSize = QSize(ds->GetRasterXSize(), ds->GetRasterYSize());
  printf("DRG id: %s, name %s, size %dx%d\n", quad.id.toLatin1().data(),
         quad.name.toLatin1().data(), drgSize.width(), drgSize.height());

  // ------------------------------------------
  // Read geotransform coefficients. The geotransform describe the mapping from
  // DRG image space to projection space.
  double geoTransformCoeff[6];
  ds->GetGeoTransform(geoTransformCoeff);

  // Top left coordinate of the drg in projection space
  QPointF fProjTopLeft = QPointF(geoTransformCoeff[0], geoTransformCoeff[3]);

  // Size of a drg pixel in projection space
  QSizeF fPixelSize = QSizeF(geoTransformCoeff[1], geoTransformCoeff[5]);

  // Check Y pixel size is the negation of the X pixel size
  if (fabs(fPixelSize.width() + fPixelSize.height()) >= epsilon) {
    fprintf(stderr, "Invalid pixel sizes\n");
    return -1;
  }

  // We assume the geotransform consists of only translation and scaling. 
  // We'd need to do a more general image transformation to handle shearing.
  if (fabs(geoTransformCoeff[2]) >= epsilon 
      || fabs(geoTransformCoeff[4]) >= epsilon) {
    fprintf(stderr, "ERROR: DRG geotransform has shear component.\n");
    return -1;
  }

  // Transforms from drg space to projection space and vice versa
  QTransform drgProjTransform;
  drgProjTransform.translate(fProjTopLeft.x(), fProjTopLeft.y());
  drgProjTransform.scale(fPixelSize.width(), fPixelSize.height());
  QTransform projDrgTransform = drgProjTransform.inverted();


  // Size of the DRG in projection space
  QSizeF fProjSize = QSizeF(qreal(ds->GetRasterXSize()) * fPixelSize.width(),
                            qreal(ds->GetRasterYSize()) * fPixelSize.height());
  QRectF projRect = QRectF(fProjTopLeft, fProjSize);

  // Rectangle covered by the entire map image
  QRectF mapRect = map.projToMap().mapRect(projRect);

  printf("Map Rect: %lf %lf %lf %lf\n", mapRect.left(), mapRect.top(),
         mapRect.right(), mapRect.bottom());


  // Compute the initial scale factor and tile level
  QSizeF mapPixelSize = map.mapPixelSize();
  assert(mapPixelSize.width() + mapPixelSize.height() < epsilon);

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
  QPolygonF projQuad(quad.boundary);


  QPolygonF geoQuad = pjGeo->transformFrom(pj, projQuad);
  QRectF geoQuadBounds(geoQuad.boundingRect());
  printf("Series: %d %s\n", quad.series, map.layer(quad.series).name.toLatin1().data());
  printf("Geographic quad boundary: %lf %lf %lf %lf\n", geoQuadBounds.left(),
         geoQuadBounds.top(), geoQuadBounds.right(), geoQuadBounds.bottom());


  // Quad bounding rectangle in map space
  QRectF projQuadBounds = projQuad.boundingRect();
  QRectF mapQuadBounds = map.projToMap().mapRect(projQuadBounds);
  printf("Quad bounding rectangle in map space: %lf %lf %lf %lf\n", 
         mapQuadBounds.left(), mapQuadBounds.top(),
         mapQuadBounds.right(), mapQuadBounds.bottom());


  // Quad bounding rectangle in drg space
  QPolygonF drgBounds = projDrgTransform.map(projQuad);
  QRectF drgQuadBounds = drgBounds.boundingRect();
  printf("Quad bounding rectangle in drg space: %lf %lf %lf %lf\n", 
         drgQuadBounds.left(), drgQuadBounds.top(),
         drgQuadBounds.right(), drgQuadBounds.bottom());


  // Read the image
  QImage drg(filename.filePath());

  if (drgQuadBounds.left() < -drgQuadSlackPixels ||
      drgQuadBounds.right() > drgSize.width() + drgQuadSlackPixels ||
      drgQuadBounds.top() < -drgQuadSlackPixels ||
      drgQuadBounds.bottom() > drgSize.height() + drgQuadSlackPixels) {
    QString mfile("misalign-" + filename.baseName() + ".png");
    fprintf(stderr, "WARNING: DRG and quadrangle boundaries are misaligned; diagnostic saved to '%s'!\n", mfile.toLatin1().data());

    QImage image = drg.convertToFormat(QImage::Format_RGB32);
    QPainter p;
    p.begin(&image);
    QPainterPath pp;
    pp.addPolygon(drgBounds);
    p.setPen(QPen(Qt::blue, 2));
    p.drawPath(pp);
    p.end();
    image.save(mfile, "png");
  }

  /*printf("top left %lf %lf\n", fProjTopLeft.x(), fProjTopLeft.y());*/
  /*for (int i = 0; i< projectedQuad.size(); i++) {
    printf("quad proj %lf %lf\n", projectedQuad[i].x(), projectedQuad[i].y());
    }*/



  QDir dir;

  //  for (int level = maxLevel; level >= 0; level--, scale /= 2.0) {
  int level = maxLevel;
    QSizeF rasterSizeF = QSizeF(ds->GetRasterXSize() * scale.width(),
                               ds->GetRasterYSize() * scale.height());
    QSize rasterSize = rasterSizeF.toSize();
    printf("level %d size %dx%d\n", level, rasterSize.width(), rasterSize.height());
    QImage drgScaled = drg.scaled(rasterSize, Qt::IgnoreAspectRatio,
                                   Qt::SmoothTransformation);

    QPolygonF imageQuad;
    for (int i = 0; i < projQuad.size(); i++) {
      QPointF p = projQuad[i] - fProjTopLeft;
      imageQuad << QPointF(p.x() * scale.width() / fPixelSize.width(), 
                           p.y() * scale.height() / fPixelSize.height());
    }

    QSizeF baseTileSize(map.baseTileSize(), map.baseTileSize());
    int tileSize = map.tileSize(level);

    QRectF tileRectF = QRectF(mapQuadBounds.topLeft() / qreal(tileSize),
                              mapQuadBounds.bottomRight() / qreal(tileSize));
    QRect tileRect(QPoint(int(floor(tileRectF.left())), int(floor(tileRectF.top()))),
                   QPoint(int(ceil(tileRectF.right())), 
                          int(ceil(tileRectF.bottom()))));
    /*    printf("tile rect %d %d %d %d\n", tileRect.left(), tileRect.top(), 
          tileRect.right(), tileRect.bottom());*/
    for (int tileY = tileRect.top(); tileY <= tileRect.bottom(); tileY++) {
      for (int tileX = tileRect.left(); tileX <= tileRect.right(); tileX++) {
        Tile key(tileX, tileY, level, quad.series);

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

    //  }
  return 0;
}
