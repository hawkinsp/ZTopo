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

class Cache::Cache;
class QActionGroup;
class QComboBox;
class QDockWidget;
class QLabel;
class QLineEdit;
class QModelIndex;
class QStandardItemModel;
class QTreeView;
class QNetworkAccessManager;

class Map;
class MapRenderer;
class MapWidget;

class CoordFormatter;
class SearchContentHandler;

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
  PrintJob(Map *m, MapRenderer *r, Cache::Cache &tileCache, 
           QPrinter *printer, int layer, QPoint mapCenter, qreal mapScale, 
           QObject *parent);
  ~PrintJob();

  virtual int currentLayer() const;
  virtual QRect visibleArea() const;

 private slots:
  void tileLoaded();
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

extern QString settingMemCache, settingDiskCache, settingDpi;

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  MainWindow(Map *m, MapRenderer *r, Cache::Cache &c, QNetworkAccessManager &mgr, 
             QWidget *parent = 0);
  ~MainWindow();

  friend class SearchContentHandler;

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

  void preferencesTriggered();
  void newWindowTriggered();
  void minimizeTriggered();
  void windowZoomTriggered();
  void bringFrontTriggered();
  void windowActionTriggered(QAction *);

  void updatePosition(QPoint pos);
  void scaleChanged(float scale);

  void pageSetupTriggered(bool);
  void printTriggered(bool);

  void searchEntered();

  void searchResultsReceived();
  void searchResultActivated(const QModelIndex &);

  void cacheIOError(const QString &msg);
private:
  Map *map;
  MapRenderer *renderer;
  Cache::Cache &tileCache;

  QNetworkAccessManager &networkManager;

  MapWidget *view;

  // Last cursor position in map coordinates.
  QPoint lastCursorPos;

  QVector<CoordFormatter *> coordFormats;
  QActionGroup *windowActions;

  // Possible grids to display
  QVector<Grid> grids;

  // Dots per inch of the current screen; 0 = use value reported by Qt
  int screenDpi;

  QToolBar *toolBar;


  QString defaultSearchCaption;
  QLabel *searchCaption;
  QLineEdit *searchLine;
  QDockWidget *searchDock;
  QTreeView *resultList;
  QStandardItemModel *searchResults;
  QNetworkReply *pendingSearch;


  // Status bar
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
  QAction *closeAction;

  QActionGroup *layerActionGroup;
  QActionGroup *coordFormatActionGroup;
  QActionGroup *datumActionGroup;
  QActionGroup *gridActionGroup;
  QAction *showRulerAction;

  QAction *showToolBarAction, *showStatusBarAction;
  
  QAction *zoomInAction, *zoomOutAction;

  QAction *preferencesAction;
  QAction *minimizeAction;
  QAction *zoomAction;
  QAction *bringFrontAction;
  QAction *showSearchResults;
  QMenu *windowMenu;

  void createActions();
  void createMenus();
  void createWidgets();
  void readSettings();

  Datum currentDatum();
  CoordFormatter *currentCoordFormatter();

  bool usingGL;

  void setSearchResultsVisible(bool);

  // Notifications from peer windows
  void windowListChanged();
  void glPreferenceChanged(bool useGL);
};


#endif
