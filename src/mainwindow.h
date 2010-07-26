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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H 1

#include <QList>
#include <QMainWindow>
#include <QPrinter>
#include <QString>
#include <QTimer>
#include <QVector>

#include "projection.h"
#include "maprenderer.h"

class QActionGroup;
class QComboBox;
class QDockWidget;
class QLabel;
class QLineEdit;
class QRegExpValidator;

class Map;
class MapRenderer;
class MapWidget;

class CoordFormatter;

class Grid {
 public:
  Grid() { }
  Grid(bool e, bool u, qreal i, QString l) 
    : enabled(e), utm(u), interval(i), label(l) { }
  bool enabled;
  bool utm;
  qreal interval;
  QString label;
};

class PrintJob : public QObject, public MapRendererClient {
  Q_OBJECT;
 public:
  PrintJob(Map *m, MapRenderer *r, QPrinter *printer, int layer, QPoint mapCenter, qreal mapScale, 
           QObject *parent);
  ~PrintJob();

  virtual int currentLayer() const;
  virtual QRect visibleArea() const;

 private slots:
  void tileUpdated();
  void tryPrint();

 private:
  Map *map;
  MapRenderer *renderer;
  QPrinter *printer;

  int layer;
  QRect mapPixelRect, pageRect;
  qreal scale, scaleX, scaleY;

  bool done;

  QTimer retryTimer;
};

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  MainWindow(Map *m, MapRenderer *r, QWidget *parent = 0);
  ~MainWindow();

protected:
  virtual void closeEvent(QCloseEvent *);

private slots:
  void datumChanged(QAction *);
  void coordFormatChanged(QAction *);
  void gridChanged(QAction *);
  void layerChanged(QAction *);
  void showRulerTriggered(bool);

  void zoomInTriggered();
  void zoomOutTriggered();

  void newWindowTriggered();
  void minimizeTriggered();
  void windowZoomTriggered();
  void bringFrontTriggered();
  void windowActionTriggered(QAction *);

  void updatePosition(QPoint pos);
  void scaleChanged(float scale);

  void pageSetupTriggered(bool);
  void printTriggered(bool);

  void searchEditingFinished();
 

private:
  Map *map;
  MapRenderer *renderer;
  MapWidget *view;

  // Last cursor position in map coordinates.
  QPoint lastCursorPos;

  QVector<CoordFormatter *> coordFormats;
  QActionGroup *windowActions;

  // Possible grids to display
  QVector<Grid> grids;

  // Dots per meter of the current screen
  qreal screenDotsPerMeter;

  QToolBar *toolBar;

  // Status bar
  QLineEdit *searchLine;
  QLabel *posLabel;     // Current position
  QLabel *scaleLabel;   // Current scale

  // Print dock widget
  QDockWidget *printDock;
  QPrinter printer;
  QList<PrintJob *> printJobs;

  // Actions
  QAction *newWindowAction;
  QAction *printAction;
  QAction *pageSetupAction;

  QActionGroup *layerActionGroup;
  QActionGroup *coordFormatActionGroup;
  QActionGroup *datumActionGroup;
  QActionGroup *gridActionGroup;
  QAction *showRulerAction;

  QAction *showToolBarAction, *showStatusBarAction;
  
  QAction *zoomInAction, *zoomOutAction;


  QAction *minimizeAction;
  QAction *zoomAction;
  QAction *bringFrontAction;
  QMenu *windowMenu;

  void createActions();
  void createMenus();
  void createWidgets();
  void readSettings();

  Datum currentDatum();
  CoordFormatter *currentCoordFormatter();

  void windowListChanged();
};


#endif
