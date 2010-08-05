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

#include <cassert>
#include <QChar>
#include <QComboBox>
#include <QDebug>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QIcon>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPageSetupDialog>
#include <QPrintDialog>
#include <QPrinter>
#include <QRegExp>
#include <QSizeF>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QStringBuilder>
#include <QToolBar>
#include <QTreeView>
#include <QXmlDefaultHandler>
#include "mainwindow.h"
#include "mapwidget.h"
#include "map.h"
#include "maprenderer.h"
#include "projection.h"
#include "preferences.h"
#include "printscene.h"
#include "printview.h"
#include "coordformatter.h"
#include "consts.h"
#include "searchhandler.h"

#include <iostream>

static const int statusMessageTimeout = 10000;

// Time to wait for tiles to arrive before retrying a print job
static const int retryTimeout = 500; 

QString settingMemCache = "maxMemCache";
QString settingDiskCache = "maxDiskCache";
QString settingDpi = "screenDpi";
QString settingUseOpenGL = "useOpenGL";

QVector<MainWindow *> windowList;

MainWindow::MainWindow(Map *m, MapRenderer *r, Cache::Cache &c, 
                       QNetworkAccessManager &mgr, QWidget *parent)
  : QMainWindow(parent), map(m), renderer(r), tileCache(c), networkManager(mgr),
    pendingSearch(NULL),
    printer(QPrinter::HighResolution)
{
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowTitle(tr("Topographic Map Viewer"));
  setUnifiedTitleAndToolBarOnMac(true);
  resize(800, 600);

  defaultSearchCaption = tr("Search for coordinates or places");

  readSettings();

  connect(&tileCache, SIGNAL(ioError(const QString &)),
          this, SLOT(cacheIOError(const QString &)));

  coordFormats << new UTMFormatter();
  coordFormats << new DMSFormatter();
  coordFormats << new DecimalDegreeFormatter();
  
  grids << Grid(false, false, 1.0,   tr("No Grid"));
  grids << Grid(true, true, 100,     tr("UTM 100m"));
  grids << Grid(true, true, 1000,    tr("UTM 1000m"));
  grids << Grid(true, true, 10000,   tr("UTM 10000m"));
  grids << Grid(true, true, 100000,  tr("UTM 100000m"));
  //  grids << Grid(true, true, 1000000, tr("UTM 1000000m"));
  grids << Grid(true, false, 0.5/60.0, tr("30\""));
  grids << Grid(true, false, 1.0/60.0, tr("1'"));
  grids << Grid(true, false, 0.125, tr("7.5'"));
  grids << Grid(true, false, 0.25, tr("15'"));
  grids << Grid(true, false, 0.5, tr("30'"));
  grids << Grid(true, false, 0.5, trUtf8("1\xc2\xb0"));

  createWidgets();
  createActions();
  createMenus();

  view->setDpi(screenDpi);
  view->centerOn(QPoint(map->requestedSize().width() / 2, 
                        map->requestedSize().height() / 2));
  connect(view, SIGNAL(positionUpdated(QPoint)), this, SLOT(updatePosition(QPoint)));
  connect(view, SIGNAL(mapScaleChanged(qreal)), this, SLOT(scaleChanged(qreal)));
  scaleChanged(view->currentMapScale());
  updatePosition(view->center());

  windowList << this;
  for (int i = 0; i < windowList.size(); i++) {
    windowList.at(i)->windowListChanged();
  }

  printer.setDocName(tr("Topographic Map"));
}

MainWindow::~MainWindow()
{
  for (int i = 0; i < coordFormats.size(); i++) {
    delete coordFormats[i];
  }

  int currentWindow = -1;
  for (int i = 0; i < windowList.size(); i++) {
    if (windowList.at(i) == this) {
      currentWindow = i;
      break;
    } 
  }
  windowList.remove(currentWindow);
  for (int i = 0; i < windowList.size(); i++) {
    windowList.at(i)->windowListChanged();
  }
}

void MainWindow::pageSetupTriggered(bool)
{
  QPageSetupDialog dialog(&printer, this);
  dialog.exec();
  printScene->setPageMetrics(printer);
}

PrintJob::PrintJob(PrintScene *ps, Cache::Cache &tileCache, QPrinter *p, 
                   int layer, QPoint mapCenter, qreal mapScale, QObject *parent)
  : QObject(parent), printScene(ps), printer(p), done(false)
{
  printScene->setPageMetrics(*printer);
  printScene->centerMapOn(mapCenter);
  printScene->setMapLayer(layer);
  printScene->setMapScale(mapScale);

  connect(&tileCache, SIGNAL(tileLoaded()), this, SLOT(tileLoaded()));
  connect(&retryTimer, SIGNAL(timeout()), this, SLOT(tryPrint()));

  tryPrint();
}


void PrintJob::tryPrint()
{
  if (!done && printScene->tilesFinishedLoading()) {
    //   qDebug() << "print job printing" << scale << " " << scaleX << " " << scaleY << " " << mapPixelRect;
    QPainter painter;
    painter.begin(printer);
    QRectF pageRect = printer->pageRect();
    QRectF targetRect(0.0, 0.0, pageRect.width(), pageRect.height());
    printScene->render(&painter, targetRect, pageRect);
    painter.end();
    
    done = true;
    printer = NULL;
  }
}

void PrintJob::tileLoaded()
{
  if (!retryTimer.isActive()) { retryTimer.start(retryTimeout); }
}

void MainWindow::printTriggered(bool)
{
  QPrintDialog *dialog = new QPrintDialog(&printer, this);
  dialog->setWindowTitle(tr("Print Document"));
  if (dialog->exec() != QDialog::Accepted)
    return;

  //  QPrinter *p = new QPrinter(QPrinter::HighResolution);
  //  clonePrinter(printer, *p);

  PrintJob *job = new PrintJob(printScene, tileCache, &printer, 
                               view->currentLayer(), view->center(),
                               view->currentMapScale(), this);
  printJobs << job;
}

void MainWindow::createWidgets()
{

  toolBar = new QToolBar(this);

  // Create the status bar
  posLabel = new QLabel();
  posLabel->setAlignment(Qt::AlignRight);
  QString posTemplate;
  posTemplate.fill('X', 30);
  posLabel->setMinimumWidth(posLabel->fontMetrics().width(posTemplate));
  posLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  statusBar()->addPermanentWidget(posLabel);

  scaleLabel = new QLabel();
  scaleLabel->setAlignment(Qt::AlignRight);
  scaleLabel->setMinimumWidth(scaleLabel->fontMetrics().width("1:XXXXXXXXXX"));
  scaleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  statusBar()->addPermanentWidget(scaleLabel);


  // Create the search dock widget
  searchDock = new QDockWidget(tr("Find"), this, Qt::Drawer);
  searchDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
  QWidget *searchArea = new QWidget(this);
  QVBoxLayout *searchVBox = new QVBoxLayout;
  searchArea->setLayout(searchVBox);

  searchResults = new QStandardItemModel;

  resultList = new QTreeView(searchDock);
  resultList->setAlternatingRowColors(true);
  resultList->setEditTriggers(QAbstractItemView::NoEditTriggers);
  resultList->setSelectionBehavior(QAbstractItemView::SelectRows);
  resultList->setSelectionMode(QAbstractItemView::SingleSelection);
  connect(resultList, SIGNAL(activated(const QModelIndex &)),
          this, SLOT(searchResultActivated(const QModelIndex &)));
  resultList->setModel(searchResults);
  searchVBox->addWidget(resultList);
  searchDock->setWidget(searchArea);
  searchDock->hide();

  // Create the print dock widget
  /*  printDock = new QDockWidget(tr("Print"), this);
  printDock->setObjectName("PrintDock");
  printDock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
  printDock->setWidget(new QLabel("hello"));
  printDock->hide();
  addDockWidget(Qt::BottomDockWidgetArea, printDock);
  */


  centralWidgetStack = new QStackedWidget;

  // Create the main view
  view = new MapWidget(map, renderer, usingGL);
  centralWidgetStack->addWidget(view);

  printScene = new PrintScene(map, renderer, printer);
  printView = new PrintView(printScene, usingGL);
  centralWidgetStack->addWidget(printView);

  centralWidgetStack->setCurrentWidget(view);
  setCentralWidget(centralWidgetStack);
}

void MainWindow::createActions()
{
  newWindowAction = new QAction(tr("New &Window"), this);
  connect(newWindowAction, SIGNAL(triggered(bool)), 
          this, SLOT(newWindowTriggered()));

  pageSetupAction = new QAction(tr("Page &Setup..."), this);
  connect(pageSetupAction, SIGNAL(triggered(bool)), this, 
          SLOT(pageSetupTriggered(bool)));

  printAction = new QAction(tr("&Print..."), this);
  printAction->setShortcuts(QKeySequence::Print);
  connect(printAction, SIGNAL(triggered(bool)), this, SLOT(printTriggered(bool)));

  closeAction = new QAction(tr("&Close"), this);
  connect(closeAction, SIGNAL(triggered(bool)), this, SLOT(close()));

  // Regular vs print view
  viewActionGroup = new QActionGroup(this);
  viewActionGroup->setExclusive(true);
  mapViewAction = new QAction(tr("Map View"), this);
  mapViewAction->setCheckable(true);
  mapViewAction->setData(0);
  viewActionGroup->addAction(mapViewAction);
  printViewAction = new QAction(tr("Print View"), this);
  printViewAction->setCheckable(true);
  printViewAction->setData(1);
  viewActionGroup->addAction(printViewAction);
  mapViewAction->setChecked(true);
  connect(viewActionGroup, SIGNAL(triggered(QAction *)),
          this, SLOT(viewChanged(QAction *)));


  // Map Layers
  layerActionGroup = new QActionGroup(this);

  QAction *autoLayer = new QAction(tr("Automatic"), this);
  autoLayer->setData(QVariant(-1));
  autoLayer->setCheckable(true);
  layerActionGroup->addAction(autoLayer);

  for (int i = 0; i < map->numLayers(); i++) {
    QAction *a = new QAction(map->layer(i).name(), this);
    a->setData(QVariant(i));
    a->setCheckable(true);
    layerActionGroup->addAction(a);
  }
  autoLayer->setChecked(true);
  connect(layerActionGroup, SIGNAL(triggered(QAction *)),
          this, SLOT(layerChanged(QAction *)));

  // Coordinate formats
  coordFormatActionGroup = new QActionGroup(this);
  for (int i = 0; i < coordFormats.size(); i++) {
    QAction *a = new QAction(coordFormats[i]->name(), this);
    a->setData(QVariant(i));
    a->setCheckable(true);
    if (i == 0) a->setChecked(true);
    coordFormatActionGroup->addAction(a);
  }
  connect(coordFormatActionGroup, SIGNAL(triggered(QAction *)),
          this, SLOT(coordFormatChanged(QAction *)));

  // Coordinate datums
  datumActionGroup = new QActionGroup(this);

  QAction *aNAD83 = new QAction(datumName(NAD83), this);
  aNAD83->setData(QVariant(NAD83));
  aNAD83->setCheckable(true);
  datumActionGroup->addAction(aNAD83);

  QAction *aNAD27 = new QAction(datumName(NAD27), this);
  aNAD27->setData(QVariant(NAD27));
  aNAD27->setCheckable(true);
  datumActionGroup->addAction(aNAD27);
  aNAD83->setChecked(true);

  connect(datumActionGroup, SIGNAL(triggered(QAction *)),
          this, SLOT(datumChanged(QAction *)));

  preferencesAction = new QAction(tr("&Preferences..."), this);
  connect(preferencesAction, SIGNAL(triggered()),
          this, SLOT(preferencesTriggered()));

  // Map grids
  gridActionGroup = new QActionGroup(this);
  for (int i = 0; i < grids.size(); i++) {
    QAction *a = new QAction(grids[i].label, this);
    a->setData(QVariant(i));
    a->setCheckable(true);
    if (i == 0) a->setChecked(true);
    gridActionGroup->addAction(a);
  }
  connect(gridActionGroup, SIGNAL(triggered(QAction *)),
          this, SLOT(gridChanged(QAction *)));
  
  showRulerAction = new QAction(tr("Show &Ruler"), this);
  showRulerAction->setCheckable(true);
  showRulerAction->setChecked(true);
  connect(showRulerAction, SIGNAL(triggered(bool)),
          this, SLOT(showRulerTriggered(bool)));

  showToolBarAction = new QAction(tr("Show &Tool Bar"), this);
  showToolBarAction->setCheckable(true);
  showToolBarAction->setChecked(true);
  connect(showToolBarAction, SIGNAL(toggled(bool)),
          this, SLOT(setToolbarVisible(bool)));

  showStatusBarAction = new QAction(tr("Show &Status Bar"), this);
  showStatusBarAction->setCheckable(true);
  showStatusBarAction->setChecked(true);
  connect(showStatusBarAction, SIGNAL(toggled(bool)),
          statusBar(), SLOT(setVisible(bool)));


  zoomInAction = new QAction(QIcon(":/images/zoom-in.png"), 
	tr("Zoom &In"), this);
  connect(zoomInAction, SIGNAL(triggered()),
          this, SLOT(zoomInTriggered()));
  zoomInAction->setShortcuts(QKeySequence::ZoomIn);
  zoomOutAction = new QAction(QIcon(":/images/zoom-out.png"), 
	tr("Zoom &Out"), this);
  connect(zoomOutAction, SIGNAL(triggered()),
          this, SLOT(zoomOutTriggered()));
  zoomOutAction->setShortcuts(QKeySequence::ZoomOut);


  minimizeAction = new QAction(tr("&Minimize"), this);
  connect(minimizeAction, SIGNAL(triggered()),
          this, SLOT(minimizeTriggered()));

  zoomAction = new QAction(tr("&Zoom"), this);
  connect(zoomAction, SIGNAL(triggered()),
          this, SLOT(windowZoomTriggered()));
 
  bringFrontAction = new QAction(tr("Bring All to Front"), this);
  connect(bringFrontAction, SIGNAL(triggered()),
          this, SLOT(bringFrontTriggered()));

  windowActions = NULL;

  showSearchResults = new QAction(QIcon(":/images/edit-find.png"), 
                                  tr("Show Search Results"), this);
  showSearchResults->setCheckable(true);
  showSearchResults->setChecked(false);
  connect(showSearchResults, SIGNAL(toggled(bool)),
          this, SLOT(setSearchResultsVisible(bool)));
}

void MainWindow::setToolbarVisible(bool vis)
{
#ifdef Q_WS_MAC
  // Work around artifacts that appear when hiding Mac unified toolbar.
  hide();
  toolBar->setVisible(vis);
  setSearchResultsVisible(false);
  show();
#else
  toolBar->setVisible(vis);
#endif

}

void MainWindow::createMenus()
{
  // Create the tool bar
  toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  toolBar->setObjectName("ToolBar");
  toolBar->addAction(zoomInAction);
  toolBar->addAction(zoomOutAction);

  QWidget* spacer = new QWidget();
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  toolBar->addWidget(spacer);

  searchLine = new QLineEdit();
  connect(searchLine, SIGNAL(returnPressed()), this, SLOT(searchEntered()));
  QWidget *searchArea = new QWidget(this);
  QVBoxLayout *searchVBox = new QVBoxLayout;
  searchVBox->addWidget(searchLine);
  QFont smallFont;
  smallFont.setPointSize(10);
  searchCaption = new QLabel(defaultSearchCaption);
  searchCaption->setAlignment(Qt::AlignHCenter);
  searchCaption->setFont(smallFont);
  searchVBox->addWidget(searchCaption);
  searchArea->setLayout(searchVBox);
  toolBar->addWidget(searchArea);
  
  toolBar->addAction(showSearchResults);
  addToolBar(toolBar);

  // Create the menu bar
  QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
  fileMenu->addAction(newWindowAction);
  fileMenu->addSeparator();
  fileMenu->addAction(printAction);
  fileMenu->addAction(pageSetupAction);
#ifndef Q_WS_MAC
  // The "Close" menu item doesn't fit into the Mac UI guidelines; Qt automatically generates
  // a suitable menu item anyway.
  fileMenu->addSeparator();
  fileMenu->addAction(closeAction);
#endif

  QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
  viewMenu->addAction(mapViewAction);
  viewMenu->addAction(printViewAction);
  viewMenu->addSeparator();
  viewMenu->addAction(preferencesAction);
  viewMenu->addSeparator();
  viewMenu->addAction(showToolBarAction);
  viewMenu->addAction(showStatusBarAction);
  viewMenu->addAction(showSearchResults);

  viewMenu->addSeparator();
  QMenu *layerMenu = viewMenu->addMenu(tr("Map &Layer"));
  QList<QAction *> layerActions = layerActionGroup->actions();
  for (int i = 0; i < layerActions.size(); i++) {
    layerMenu->addAction(layerActions.at(i));
  }

  viewMenu->addSeparator();

  QMenu *formatMenu = viewMenu->addMenu(tr("Coordinate &Format"));
  QList<QAction *> formatActions = coordFormatActionGroup->actions();
  for (int i = 0; i < formatActions.size(); i++) {
    formatMenu->addAction(formatActions.at(i));
  }

  QMenu *datumMenu = viewMenu->addMenu(tr("Coordinate &Datum"));
  QList<QAction *> datumActions = datumActionGroup->actions();
  for (int i = 0; i < datumActions.size(); i++) {
    datumMenu->addAction(datumActions.at(i));
  }

  viewMenu->addSeparator();

  QMenu *gridMenu = viewMenu->addMenu(tr("&Grid"));
  QList<QAction *> gridActions = gridActionGroup->actions();
  for (int i = 0; i < gridActions.size(); i++) {
    gridMenu->addAction(gridActions.at(i));
  }

  viewMenu->addAction(showRulerAction);

  viewMenu->addSeparator();
  viewMenu->addAction(zoomInAction);
  viewMenu->addAction(zoomOutAction);


  windowMenu = menuBar()->addMenu(tr("&Window"));  
}

void MainWindow::readSettings()
{
  QSettings settings;
  screenDpi = settings.value(settingDpi, 0).toInt();
  restoreGeometry(settings.value("geometry").toByteArray());
  restoreState(settings.value("windowState").toByteArray());
  usingGL = settings.value(settingUseOpenGL, false).toBool();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  QSettings settings;
  settings.setValue("geometry", saveGeometry());
  settings.setValue("windowState", saveState());
  QMainWindow::closeEvent(event);
}

void MainWindow::setSearchResultsVisible(bool vis)
{
  QList<QString> labels;
  labels << tr("Name");
  labels << tr("Type");
  labels << tr("County");
  labels << tr("Map Name");
  searchResults->setHorizontalHeaderLabels(labels);
  for (int i = 0; i < labels.size(); i++) {
    resultList->resizeColumnToContents(i);
  }
  showSearchResults->setChecked(vis);
  view->setSearchResultsVisible(vis);
  if (vis) setCurrentView(MapKind);
  searchDock->setVisible(vis);
}

void MainWindow::viewChanged(QAction *a)
{
  setCurrentView(ViewKind(a->data().toInt()));
}

ViewKind MainWindow::currentView()
{
  return ViewKind(centralWidgetStack->currentIndex());
}

void MainWindow::setCurrentView(ViewKind kind)
{

  switch (kind) {
  case MapKind: //
    centralWidgetStack->setCurrentIndex(MapKind);
    mapViewAction->setChecked(true);
    break;
  case PrintKind:
    printScene->setPageMetrics(printer);
    printScene->setMapScale(view->currentMapScale());
    printScene->setMapLayer(view->currentLayer());
    printScene->centerMapOn(view->center());
    printView->fitToView();
    centralWidgetStack->setCurrentIndex(PrintKind);
    printViewAction->setChecked(true);
    setSearchResultsVisible(false);
    break;
  default: qFatal("Unknown view ID in viewChanged");
  }
}

void MainWindow::layerChanged(QAction *a)
{
  int layer = a->data().toInt();
  view->setLayer(layer);
}

void MainWindow::datumChanged(QAction *)
{
  updatePosition(lastCursorPos);
  gridChanged(gridActionGroup->checkedAction());
}


void MainWindow::coordFormatChanged(QAction *a)
{
  int idx = a->data().toInt();
  assert(0 <= idx && idx < coordFormats.size());
  updatePosition(lastCursorPos);
}

void MainWindow::gridChanged(QAction *a)
{
  int g = a->data().toInt();
  if (grids[g].enabled) {
    view->showGrid(currentDatum(), grids[g].utm, grids[g].interval);
    printScene->showGrid(currentDatum(), grids[g].utm, grids[g].interval);
  } else {
    view->hideGrid();
    printScene->hideGrid();
  }
}


void MainWindow::newWindowTriggered()
{
  MainWindow *m = new MainWindow(map, renderer, tileCache, networkManager);
  m->show();
}

void MainWindow::preferencesTriggered()
{
  PreferencesDialog prefDlg(tileCache);
  prefDlg.setDpi(screenDpi);
  prefDlg.setCacheSizes(tileCache.getMemCacheSize(), tileCache.getDiskCacheSize());
  prefDlg.setUseOpenGL(usingGL);
  int ret = prefDlg.exec();

  if (ret != QDialog::Accepted)
    return;

  screenDpi = prefDlg.getDpi();
  view->setDpi(screenDpi);
  tileCache.setCacheSizes(prefDlg.getMemSize(), prefDlg.getDiskSize());
  QSettings settings;
  settings.setValue(settingDpi, screenDpi);
  settings.setValue(settingMemCache, prefDlg.getMemSize());
  settings.setValue(settingDiskCache, prefDlg.getDiskSize());
  settings.setValue(settingUseOpenGL, prefDlg.getUseOpenGL());
  if (usingGL != prefDlg.getUseOpenGL()) {
    foreach (MainWindow *w, windowList) {
      w->glPreferenceChanged(prefDlg.getUseOpenGL());
    }
  }
}

void MainWindow::windowListChanged()
{
  if (windowActions) delete windowActions;

  windowMenu->clear();
  windowMenu->addAction(minimizeAction);
  windowMenu->addAction(zoomAction);
  windowMenu->addSeparator();
  windowMenu->addAction(bringFrontAction);
  windowMenu->addSeparator();

  windowActions = new QActionGroup(this);
  connect(windowActions, SIGNAL(triggered(QAction *)), 
          this, SLOT(windowActionTriggered(QAction *)));
  for (int i = 0; i < windowList.size(); i++) {
    QAction *a = new QAction(windowList[i]->windowTitle(), this);
    windowActions->addAction(a);
    windowMenu->addAction(a);
  }
}

void MainWindow::glPreferenceChanged(bool useGL)
{
  usingGL = useGL;
  view->setGL(useGL);
  printView->setGL(useGL);
}

void MainWindow::windowActionTriggered(QAction *a)
{
  int i = a->data().toInt();
  windowList[i]->raise();
}

void MainWindow::showRulerTriggered(bool v)
{
  view->setRulerVisible(v);
}

static const qreal zoomIncrement = 1.333;
void MainWindow::zoomInTriggered()
{
  switch (currentView()) {
  case MapKind: view->zoomIn(); break;
  case PrintKind: printView->scale(zoomIncrement, zoomIncrement); break;
  }
}

void MainWindow::zoomOutTriggered()
{
  switch (currentView()) {
  case MapKind: view->zoomOut(); break;
  case PrintKind: printView->scale(1.0/zoomIncrement, 1.0/zoomIncrement); break;
  }
}


void MainWindow::minimizeTriggered()
{
  setWindowState(Qt::WindowMinimized);
}

void MainWindow::windowZoomTriggered()
{
  setWindowState(Qt::WindowMaximized);
}

void MainWindow::bringFrontTriggered()
{
  raise();
}



Datum MainWindow::currentDatum() {
  return Datum(datumActionGroup->checkedAction()->data().toInt());
}

CoordFormatter *MainWindow::currentCoordFormatter() {
  return coordFormats[coordFormatActionGroup->checkedAction()->data().toInt()];
}


void MainWindow::searchEntered()
{
  Projection *pjGeo = Geographic::getProjection(currentDatum());
  QPointF centerProj = map->mapToProj().map(view->center());
  QPointF centerGeo = pjGeo->transformFrom(map->projection(), centerProj);
  QPointF gp;
  QString query = searchLine->text();

  searchResults->clear();
  view->setSearchResultsVisible(false);
  view->setSearchResults(QList<QPoint>());
  if (query.isEmpty()) {
    searchCaption->setText(defaultSearchCaption);
    return;
  }

  // First try to interpret input as a coordinate
  for (int i = 0; i < coordFormats.size(); i++) {
    if (coordFormats[i]->parse(currentDatum(), centerGeo, searchLine->text(), gp)) {
      QPointF p = map->projection()->transformFrom(pjGeo, gp);
      QPoint mp = map->projToMap().map(p).toPoint();
      view->centerOn(mp);

      QList<QStandardItem *> items;
      QStandardItem *name = new QStandardItem(searchLine->text());
      name->setData(gp, Qt::UserRole + 1);
      items << name;
      items << new QStandardItem("Coordinate");
      searchResults->appendRow(items);

      QList<QPoint> mapPoints;
      mapPoints << mp;
      view->setSearchResults(mapPoints);
      setSearchResultsVisible(true);

      searchCaption->setText(tr("Matching coordinate found"));
      lastCursorPos = mp;
      return;
    }
  }

  // Otherwise try GNIS search
  if (pendingSearch) {
    pendingSearch->abort();
    pendingSearch = NULL;
  }

  QUrl url("http://geonames.usgs.gov/pls/gnis/x");
  url.addQueryItem("fname", "'" % searchLine->text() % "'");
  url.addQueryItem("state", "'california'");
  url.addQueryItem("op", "1");

  QNetworkRequest request(url);
  pendingSearch = networkManager.get(request);
  connect(pendingSearch, SIGNAL(finished()), this, SLOT(searchResultsReceived()));

  searchCaption->setText(tr("Searching..."));
}


void MainWindow::searchResultsReceived()
{
  assert(pendingSearch != NULL);
  
  if (pendingSearch->error() != 0) {
    QString msg = tr("Error retrieving search results: %1")
      .arg(pendingSearch->errorString());
    statusBar()->showMessage(msg, statusMessageTimeout);  
    searchCaption->setText(msg);
    setSearchResultsVisible(false);
    pendingSearch = NULL;
    return;
  }
  
  QXmlInputSource source(pendingSearch);
  QXmlSimpleReader reader;
  SearchHandler handler;
  reader.setContentHandler(&handler);
  reader.setErrorHandler(&handler);
  reader.parse(source);

  pendingSearch->deleteLater();
  pendingSearch = NULL;

  if (handler.hasErrors()) {
    statusBar()->showMessage(tr("Error reading search results"), 
                             statusMessageTimeout);
    setSearchResultsVisible(false);
    return;
  }

  const QList<SearchResult> &results = handler.results();
  searchCaption->setText(tr("%1 results found").arg(results.size()));

  QList<QPoint> resultPoints;
  Projection *pjGeo = Geographic::getProjection(NAD83);

  resultList->setSortingEnabled(false);
  foreach (const SearchResult &r, results) {
    QList<QStandardItem *> items;
    QStandardItem *name = new QStandardItem(r.featureName);
    name->setData(r.location, Qt::UserRole + 1);
    items << name;
    items << new QStandardItem(r.featureType);
    items << new QStandardItem(r.countyName);
    items << new QStandardItem(r.cellName);
    searchResults->appendRow(items);

    QPointF pProj = map->projection()->transformFrom(pjGeo, r.location);
    resultPoints << map->projToMap().map(pProj).toPoint();
  }
  resultList->setSortingEnabled(true);
  view->setSearchResults(resultPoints);
  setSearchResultsVisible(true);

  if (resultPoints.size() == 1) {
    view->centerOn(resultPoints[0]);
  } 

}

void MainWindow::searchResultActivated(const QModelIndex &i)
{
  QModelIndex name = i.sibling(i.row(), 0);
  QPointF pGeo = name.data(Qt::UserRole + 1).toPointF();
  QPointF pProj = map->projection()->transformFrom(Geographic::getProjection(NAD83),
                                                   pGeo);
  QPoint pMap = map->projToMap().map(pProj).toPoint();
  view->centerOn(pMap);
}

void MainWindow::updatePosition(QPoint m)
{
  lastCursorPos = m;

  Datum d = currentDatum();
  QPointF g = Geographic::getProjection(d)->transformFrom(map->projection(), 
                                             map->mapToProj().map(QPointF(m)));

  posLabel->setText(currentCoordFormatter()->format(d, g));
}

void MainWindow::scaleChanged(qreal mapScale)
{
  scaleLabel->setText("1:" + QString::number(int(mapScale)));
}

void MainWindow::cacheIOError(const QString &msg)
{
  statusBar()->showMessage(msg, statusMessageTimeout);
}
