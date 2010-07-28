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
#include <QPageSetupDialog>
#include <QPrintDialog>
#include <QPrinter>
#include <QRegExp>
#include <QSizeF>
#include <QSettings>
#include <QStatusBar>
#include <QStringBuilder>
#include <QToolBar>
#include "mainwindow.h"
#include "mapwidget.h"
#include "map.h"
#include "maprenderer.h"
#include "projection.h"
#include "preferences.h"
#include "coordformatter.h"
#include "consts.h"

#include <iostream>

static const int retryTimeout = 500; // Wait 500ms for tiles to arrive before retrying a print job
QString settingMemCache = "maxMemCache";
QString settingDiskCache = "maxDiskCache";
QString settingDpi = "screenDpi";

QVector<MainWindow *> windowList;

MainWindow::MainWindow(Map *m, MapRenderer *r, QWidget *parent)
  : QMainWindow(parent), map(m), renderer(r), printer(QPrinter::HighResolution)
{
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowTitle(tr("Topographic Map Viewer"));
  setUnifiedTitleAndToolBarOnMac(true);
  resize(800, 600);

  coordFormats << new UTMFormatter();
  coordFormats << new DMSFormatter();
  coordFormats << new DecimalDegreeFormatter();
  
  grids << Grid(false, false, 1.0,   tr("No Grid"));
  grids << Grid(true, true, 100,     tr("UTM 100m"));
  grids << Grid(true, true, 1000,    tr("UTM 1000m"));
  grids << Grid(true, true, 10000,   tr("UTM 10000m"));
  grids << Grid(true, true, 100000,  tr("UTM 100000m"));
  grids << Grid(true, true, 1000000, tr("UTM 1000000m"));
  grids << Grid(true, false, 0.5/60.0, tr("30\""));
  grids << Grid(true, false, 1.0/60.0, tr("1'"));
  grids << Grid(true, false, 0.125, tr("7.5'"));
  grids << Grid(true, false, 0.25, tr("15'"));
  grids << Grid(true, false, 0.5, tr("30'"));
  grids << Grid(true, false, 0.5, trUtf8("1\xc2\xb0"));

  createWidgets();
  createActions();
  createMenus();
  readSettings();

  view->centerOn(QPoint(map->requestedSize().width() / 2, map->requestedSize().height() / 2));
  connect(view, SIGNAL(positionUpdated(QPoint)), this, SLOT(updatePosition(QPoint)));
  connect(view, SIGNAL(scaleChanged(float)), this, SLOT(scaleChanged(float)));
  scaleChanged(view->currentScale());
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
}

/*
void clonePrinter(const QPrinter &from, QPrinter &to)
{
  to.setPrinterName(from.printerName());
  to.setPrinterSelectionOption(from.printerSelectionOption());
  QSizeF paperSize = from.paperSize(QPrinter::Millimeter);
  qDebug() << "paper size " << paperSize;
  to.setPaperSize(paperSize, QPrinter::Millimeter);

  to.setCollateCopies(from.collateCopies());
  to.setColorMode(from.colorMode());
  to.setCreator(from.creator());
  to.setDocName(from.docName());
  to.setDoubleSidedPrinting(from.doubleSidedPrinting());
  to.setDuplex(from.duplex());
  to.setFontEmbeddingEnabled(from.fontEmbeddingEnabled());
  to.setFromTo(from.fromPage(), from.toPage());
  to.setFullPage(from.fullPage());
  to.setNumCopies(from.numCopies());
  to.setOrientation(from.orientation());
  to.setOutputFileName(from.outputFileName());
  qreal left, top, right, bottom;
  from.getPageMargins(&left, &top, &right, &bottom, QPrinter::Millimeter);
  to.setPageMargins(left, top, right, bottom, QPrinter::Millimeter);
  to.setPageOrder(from.pageOrder());
  to.setPaperSource(from.paperSource());
  to.setPrintProgram(from.printProgram());
  to.setPrintRange(from.printRange());
  to.setResolution(from.resolution());
}
*/

PrintJob::PrintJob(Map *m, MapRenderer *r, QPrinter *p, int layer, QPoint mapCenter, qreal mapScale, 
                   QObject *parent)
  : QObject(parent), map(m), renderer(r), printer(p), layer(layer), done(false)
{
  renderer->addClient(this);
  connect(renderer, SIGNAL(tileUpdated()), this, SLOT(tileUpdated()));
  connect(&retryTimer, SIGNAL(timeout()), this, SLOT(tryPrint()));
  
  pageRect = printer->pageRect();

  // Size of the page in meters
  QSizeF pagePhysicalArea(qreal(pageRect.width()) / printer->logicalDpiX() * metersPerInch, 
                 qreal(pageRect.height()) / printer->logicalDpiY() * metersPerInch);

  // Size of the map area in meters
  QSizeF mapPhysicalArea = pagePhysicalArea * mapScale;

  // Size of the map area in pixels
  // QSize mapPixelArea = 
  // map->mapToProj().mapRect(QRectF(QPointF(0, 0), mapPhysicalArea)).size().toSize();
  QSizeF mapPixelSize = map->mapPixelSize();
  QSize mapPixelArea(mapPhysicalArea.width() / mapPixelSize.width(), 
                     mapPhysicalArea.height() / -mapPixelSize.height());

  QPoint mapPixelTopLeft = mapCenter - QPoint(mapPixelArea.width() / 2, mapPixelArea.height() / 2);
  mapPixelRect = QRect(mapPixelTopLeft, mapPixelArea);

  scaleX = qreal(pageRect.width()) / mapPixelArea.width();
  scaleY = qreal(pageRect.height()) / mapPixelArea.height();
  scale = std::max(scaleX, scaleY);

  /*  qDebug("Print mapScale %f; scale %f", mapScale, scale);
  qDebug("Logical dpi is %d %d Physical dpi is %d %d\n", printer->logicalDpiX(), printer->logicalDpiY(), 
         printer->physicalDpiX(), printer->physicalDpiY());
  qDebug("Page is %d x %d = %f m x %f m\n", pageRect.width(), pageRect.height(), pagePhysicalArea.width(),
         pagePhysicalArea.height());
  qDebug("Map is %d x %d = %f m x %f m\n", mapPixelArea.width(), mapPixelArea.height(), mapPhysicalArea.width(),
         mapPhysicalArea.height());
  */
  tryPrint();
}

PrintJob::~PrintJob()
{
  //  if (printer) { delete printer; }
  if (!done)   { renderer->removeClient(this); }
}

int PrintJob::currentLayer() const {
  return layer;
}

QRect PrintJob::visibleArea() const {
  return mapPixelRect;
}

void PrintJob::tryPrint()
{
  if (!done && renderer->loadTiles(layer, mapPixelRect, scale)) {
    //   qDebug() << "print job printing" << scale << " " << scaleX << " " << scaleY << " " << mapPixelRect;
    QPainter painter;
    painter.begin(printer);
    painter.drawRect(0, 0, pageRect.width(), pageRect.height());
    painter.scale(scaleX / scale, scaleY / scale);
    renderer->render(painter, layer, mapPixelRect, scale);
    painter.end();
    
    done = true;
    renderer->removeClient(this);
    // delete printer;
    printer = NULL;
  }
}

void PrintJob::tileUpdated()
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
  int dpi = (screenDpi <= 0) ? logicalDpiX() : screenDpi;

  qreal scale = (map->mapPixelSize().width() * dpi / metersPerInch)
    / view->currentScale(); 

  PrintJob *job = new PrintJob(map, renderer, &printer, view->currentLayer(), view->center(), scale, this);
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


  // Create the print dock widget
  printDock = new QDockWidget(tr("Print"), this);
  printDock->setObjectName("PrintDock");
  printDock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
  printDock->setWidget(new QLabel("hello"));
  printDock->hide();
  addDockWidget(Qt::BottomDockWidgetArea, printDock);


  // Create the main view
  view = new MapWidget(map, renderer);
  view->setCursor(Qt::OpenHandCursor);
  setCentralWidget(view);

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
          toolBar, SLOT(setVisible(bool)));

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
  connect(searchLine, SIGNAL(editingFinished()), this, SLOT(searchEditingFinished()));
  toolBar->addWidget(new QLabel(tr("Find:")));
  toolBar->addWidget(searchLine);
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
  viewMenu->addAction(preferencesAction);
  viewMenu->addSeparator();
  viewMenu->addAction(showToolBarAction);
  viewMenu->addAction(showStatusBarAction);

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
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  QSettings settings;
  settings.setValue("geometry", saveGeometry());
  settings.setValue("windowState", saveState());
  QMainWindow::closeEvent(event);
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
  } else {
    view->hideGrid();
  }
}


void MainWindow::newWindowTriggered()
{
  MainWindow *m = new MainWindow(map, renderer);
  m->show();
}

void MainWindow::preferencesTriggered()
{
  Cache::Cache &cache = renderer->getCache();
  PreferencesDialog prefDlg(cache);
  prefDlg.setDpi(screenDpi);
  prefDlg.setCacheSizes(cache.getMemCacheSize(), cache.getDiskCacheSize());
  int ret = prefDlg.exec();

  if (ret != QDialog::Accepted)
    return;

  screenDpi = prefDlg.getDpi();
  cache.setCacheSizes(prefDlg.getMemSize(), prefDlg.getDiskSize());
  QSettings settings;
  settings.setValue(settingDpi, screenDpi);
  settings.setValue(settingMemCache, prefDlg.getMemSize());
  settings.setValue(settingDiskCache, prefDlg.getDiskSize());
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
  view->setScale(view->currentScale() * zoomIncrement);
}

void MainWindow::zoomOutTriggered()
{
  view->setScale(view->currentScale() / zoomIncrement);
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


void MainWindow::searchEditingFinished()
{
  Projection *pjGeo = Geographic::getProjection(currentDatum());
  QPointF gc = pjGeo->transformFrom(map->projection(), view->center());
  QPointF gp;

  for (int i = 0; i < coordFormats.size(); i++) {
    if (coordFormats[i]->parse(currentDatum(), gc, searchLine->text(), gp)) {
      QPointF p = map->projection()->transformFrom(pjGeo, gp);
      QPoint mp = map->projToMap().map(p).toPoint();
      view->centerOn(mp);
      lastCursorPos = mp;
      return;
    }
  }

  statusBar()->showMessage(tr("Unknown coordinates"), 10000);
}

void MainWindow::updatePosition(QPoint m)
{
  lastCursorPos = m;

  Datum d = currentDatum();
  QPointF g = Geographic::getProjection(d)->transformFrom(map->projection(), 
                                             map->mapToProj().map(QPointF(m)));

  posLabel->setText(currentCoordFormatter()->format(d, g));
}

void MainWindow::scaleChanged(float scaleFactor)
{
  //  printf("dpi %d %d phys %d %d\n", logicalDpiX(), logicalDpiY(), physicalDpiX(), physicalDpiY());
  int dpi = (screenDpi <= 0) ? logicalDpiX() : screenDpi;

  qreal scale = (map->mapPixelSize().width() * dpi / metersPerInch)
    / scaleFactor;
  scaleLabel->setText("1:" + QString::number(int(scale)));
}
