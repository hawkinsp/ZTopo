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
class QStackedWidget;
class QStandardItemModel;
class QTreeView;
class QNetworkAccessManager;

class Map;
class MapRenderer;
class MapWidget;
class PrintScene;
class PrintView;
class RootData;

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

class PrintJob : public QObject {
  Q_OBJECT;
 public:
  PrintJob(PrintScene *ps, Cache::Cache &tileCache, QPrinter *printer, QObject *parent);

 private slots:
  void tileLoaded();
  void tryPrint();

 private:
  PrintScene *printScene;
  QPrinter *printer;

  bool done;

  QTimer retryTimer;
};

extern QString settingMemCache, settingDiskCache, settingDpi;

enum ViewKind {
  MapKind = 0,
  PrintKind = 1
};

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  MainWindow(const RootData &d, Map *m, MapRenderer *r, Cache::Cache &c, 
             QNetworkAccessManager &mgr, QWidget *parent = 0);
  ~MainWindow();

  friend class SearchContentHandler;

protected:
  virtual void closeEvent(QCloseEvent *);

private slots:
  void datumChanged(QAction *);
  void coordFormatChanged(QAction *);
  void gridChanged(QAction *);
  void layerChanged(QAction *);
  void viewChanged(QAction *);
  void showRulerTriggered(bool);


  void setToolbarVisible(bool);

  void zoomInTriggered();
  void zoomOutTriggered();
  void zoomToScaleTriggered(QAction *);

  void preferencesTriggered();
  void newWindowTriggered();
  void minimizeTriggered();
  void windowZoomTriggered();
  void bringFrontTriggered();
  void windowActionTriggered(QAction *);

  void updatePosition(QPoint pos);
  void scaleChanged(qreal scale);

  void pageSetupTriggered(bool);
  void printTriggered(bool);

  void searchEntered();

  void setSearchResultsVisible(bool);
  void searchResultsReceived();
  void searchResultActivated(const QModelIndex &);

  void cacheIOError(const QString &msg);
private:
  const RootData &rootData;
  Map *map;
  MapRenderer *renderer;
  Cache::Cache &tileCache;

  QNetworkAccessManager &networkManager;

  // There are two possible views --- the standard map view and a print preview.
  QStackedWidget *centralWidgetStack;
  MapWidget *view;
  PrintView *printView;
  PrintScene *printScene;

  // Last cursor position in map coordinates.
  QPoint lastCursorPos;

  QVector<CoordFormatter *> coordFormats;
  QActionGroup *windowActions;

  // Possible grids to display
  QList<Grid> grids;

  // Suggested scales
  QList<int> suggestedMapScales;

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

  QActionGroup *viewActionGroup;
  QAction *mapViewAction;
  QAction *printViewAction;

  QActionGroup *layerActionGroup;
  QActionGroup *coordFormatActionGroup;
  QActionGroup *datumActionGroup;
  QActionGroup *gridActionGroup;
  QAction *showRulerAction;

  QAction *showToolBarAction, *showStatusBarAction;
  
  QAction *zoomInAction, *zoomOutAction;
  QActionGroup *zoomToScaleActionGroup;

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

  // Notifications from peer windows
  void windowListChanged();
  void glPreferenceChanged(bool useGL);

  ViewKind currentView();
  void setCurrentView(ViewKind);
};


#endif
