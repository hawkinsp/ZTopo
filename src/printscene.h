#ifndef PRINTSCENE_H
#define PRINTSCENE_H 1

#include "projection.h"
#include <QGraphicsScene>
class Map;
class MapItem;
class MapRenderer;

class PrintScene : public QGraphicsScene
{
  Q_OBJECT;

public:
  PrintScene(Map *m, MapRenderer *r, const QPrinter &printer);

  // Center on map coordinates
  void centerMapOn(QPoint c);

  // Set the current map layer
  void setMapLayer(int layer);

  // Set the current map scale; 24000 means 1:24000.
  void setMapScale(int scale);

  // Adjust the scene's page metrics to match the given printer
  void setPageMetrics(const QPrinter &printer);

  // Set grid projection
  void showGrid(Datum d, bool utm, qreal interval);
  void hideGrid();

  bool tilesFinishedLoading();

private slots:
  void tileLoaded();


private:
  QGraphicsRectItem *paperRectItem;
  QGraphicsRectItem *pageRectItem;
  MapItem *mapItem;
};

#endif
