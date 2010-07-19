#include <QChar>
#include <QComboBox>
#include <QDockWidget>
#include <QHBoxLayout>
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
#include "mapprojection.h"
#include "maprenderer.h"
#include "projection.h"
#include "consts.h"

#include <iostream>


const QChar degree(0x00b0);

class CoordFormatter {
public:
  QRegExp &getRegExp() { return re; }
  QString &name() { return sName; }

  virtual QString format(Datum, QPointF) = 0;

  // Given a datum d and "current" point c, parse a string into a geographic
  // coordinate
  virtual bool parse(Datum d, QPointF c, const QString &, QPointF &) = 0;
protected:
  QRegExp re;
  QString sName;
};

class DecimalDegreeFormatter : public CoordFormatter {
public:
  DecimalDegreeFormatter();
  virtual QString format(Datum, QPointF);
  virtual bool parse(Datum, QPointF, const QString &, QPointF &);
};

class DMSFormatter : public CoordFormatter {
public:
  DMSFormatter();
  virtual QString format(Datum, QPointF);
  virtual bool parse(Datum, QPointF, const QString &, QPointF &);
};

class UTMFormatter : public CoordFormatter {
public:
  UTMFormatter();
  virtual QString format(Datum, QPointF);
  virtual bool parse(Datum, QPointF, const QString &, QPointF &);
};

DecimalDegreeFormatter::DecimalDegreeFormatter()
{
  sName = QObject::trUtf8("DDD.MMM\xc2\xb0");

  QString pattern(QString::fromUtf8("(-?)(\\d+)(.\\d+)?\xc2\xb0?([ nNsS]) *(-?)(\\d+)(.\\d+)?\xc2\xb0?([ wWeE]?)"));
  re.setPattern(pattern);
}
bool DecimalDegreeFormatter::parse(Datum, QPointF, const QString &s, QPointF &p)
{
  if (!re.exactMatch(s)) return false;

  QString latMinus = re.cap(1);
  QString latVal = re.cap(2) + re.cap(3);
  QString latSuffix = re.cap(4);

  QString lonMinus = re.cap(5);
  QString lonVal = re.cap(6) + re.cap(7);
  QString lonSuffix = re.cap(8);

  qreal lat = latVal.toDouble();
  if (latMinus == "-") lat *= -1.0;
  if (latSuffix[0] == 's' || latSuffix[0] == 'S') lat *= -1.0;

  qreal lon = lonVal.toDouble();
  if (lonMinus == "-") lon *= -1.0;
  if (lonSuffix[0] == 'w' || lonSuffix[0] == 'W') lon *= -1.0;

  
  p = QPointF(lon, lat);
  return true;
}

QString DecimalDegreeFormatter::format(Datum, QPointF g)
{
  QString s = 
    QString::number(fabs(g.y()), 'f', 5) % degree % (g.y() >= 0 ? 'N' : 'S')
    % " " % 
    QString::number(fabs(g.x()), 'f', 5) % degree % (g.x() >= 0 ? 'E' : 'W');
  return s;
}

DMSFormatter::DMSFormatter()
{
  sName = QObject::trUtf8("DDD\xc2\xb0MM'SS\"");
  QString pattern(QString::fromUtf8("(-?)(\\d+)[\xc2\xb0 ] *(\\d+)[' ] *(\\d+)\"? *([nNsS]?) *(-?)(\\d+)[\xc2\xb0 ] *(\\d+)[' ] *(\\d+)\"? *([wWeE]?)"));
  re.setPattern(pattern);
}

bool DMSFormatter::parse(Datum, QPointF, const QString &s, QPointF &p)
{
  if (!re.exactMatch(s)) return false;

  QString latMinus = re.cap(1);
  QString latDeg = re.cap(2);
  QString latMin = re.cap(3);
  QString latSec = re.cap(4);
  QString latSuffix = re.cap(5);

  QString lonMinus = re.cap(6);
  QString lonDeg = re.cap(7);
  QString lonMin = re.cap(8);
  QString lonSec = re.cap(9);
  QString lonSuffix = re.cap(10);

  qreal lat = latDeg.toDouble() + latMin.toDouble() / 60.0 + 
    latSec.toDouble() / 3600.0;
  if (latMinus == "-") lat *= -1.0;
  if (latSuffix[0] == 's' || latSuffix[0] == 'S') lat *= -1.0;

  qreal lon = lonDeg.toDouble() + lonMin.toDouble() / 60.0 + 
    lonSec.toDouble() / 3600.0;
  if (lonMinus == "-") lon *= -1.0;
  if (lonSuffix[0] == 'w' || lonSuffix[0] == 'W') lon *= -1.0;

  p = QPointF(lon, lat);
  return true;
}

QString DMSFormatter::format(Datum, QPointF g)
{
  qreal x = fabs(g.x()), y = fabs(g.y());
  int yd = int(y);
  int ym = int(60.0 * (y - yd));
  int ys = int(3600.0 * (y - qreal(yd) - ym / 60.0));
  int xd = int(x);
  int xm = int(60.0 * (x - xd));
  int xs = int(3600.0 * (x - qreal(xd) - xm / 60.0));
  
  QString s = 
    QString::number(yd) % degree % 
    QString::number(ym).rightJustified(2, '0') % '\'' % 
    QString::number(ys).rightJustified(2, '0') % '"' 
    % (g.y() >= 0 ? 'N' : 'S') % " " % 
    QString::number(xd) % degree % 
    QString::number(xm).rightJustified(2, '0') % '\'' % 
    QString::number(xs).rightJustified(2, '0') % '"' 
    % (g.x() >=0 ? 'E' : 'W');
  return s;
}

UTMFormatter::UTMFormatter()
{
  sName = QObject::tr("UTM");
  QString pattern("(\\d{1,2}[a-zA-Z]?)? *(\\d+)m?[eE]? +(\\d+)m?[nN]?");
  re.setPattern(pattern);
}

bool UTMFormatter::parse(Datum d, QPointF c, const QString &s, QPointF &p)
{
  if (!re.exactMatch(s)) return false;

  QString easting, northing;
  int zone;
  if (re.captureCount() == 2) {
    zone = UTM::bestZone(c).zone;
    easting = re.cap(1);
    northing = re.cap(2);
  } else {
    QString z = re.cap(1);
    easting = re.cap(2);
    northing = re.cap(3);

    if (!z[z.size() - 1].isDigit()) z = z.left(z.size() - 1);
    zone = z.toInt();
    if (zone < 1 || zone > UTM::numZones) return false;
  }

  QPointF q(easting.toDouble(), northing.toDouble());
  Projection *pj = UTM::getZoneProjection(d, zone);
  p = Geographic::getProjection(d)->transformFrom(pj, q);
  return true;
}

QString UTMFormatter::format(Datum d, QPointF g)
{
  UTM::Zone z = UTM::bestZone(g);
  Projection *pjUTM = UTM::getZoneProjection(d, z.zone);
  QPointF p = pjUTM->transformFrom(Geographic::getProjection(d), g);

  QString s = QString::number(z.zone) % z.band % " " %
    QString::number(p.x(), 'f', 0) % "mE " %
    QString::number(p.y(), 'f', 0) % "mN";

  return s;
}

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow(parent)
{

  setWindowTitle(tr("Topographic Map Viewer"));
  resize(800, 600);

  QTransform ctr;
  Projection *pj = new Projection(californiaMapProjection);
  californiaProjToMapTransform(ctr);
  map = new Map(NAD83, pj, ctr, californiaMapSize);
  renderer = new MapRenderer(map);

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

  screenDotsPerMeter = qreal(logicalDpiX()) / metersPerInch;

  createActions();
  createMenus();
  createWidgets();
  readSettings();

  view->centerOn(QPoint(389105, 366050));
  connect(view, SIGNAL(positionUpdated(QPoint)), this, SLOT(updatePosition(QPoint)));
  connect(view, SIGNAL(scaleChanged(float)), this, SLOT(scaleChanged(float)));
  scaleChanged(1.0);
  updatePosition(view->center());

}

MainWindow::~MainWindow()
{
  for (int i = 0; i < coordFormats.size(); i++) {
    delete coordFormats[i];
  }
  delete map;
}

void MainWindow::pageSetupTriggered(bool)
{
  QPageSetupDialog dialog(this);
  dialog.exec();
}

void MainWindow::printTriggered(bool)
{
  QPrinter printer(QPrinter::HighResolution);
  QPrintDialog *dialog = new QPrintDialog(&printer, this);
  dialog->setWindowTitle(tr("Print Document"));
  if (dialog->exec() != QDialog::Accepted)
    return;


  QRect pageRect = printer.pageRect();

  qreal mapScale = 24000.0;
  // Size of the page in meters
  QSizeF pagePhysicalArea(qreal(pageRect.width()) / printer.logicalDpiX() * metersPerInch, 
                 qreal(pageRect.height()) / printer.logicalDpiY() * metersPerInch);

  // Size of the map area in meters
  QSizeF mapPhysicalArea = pagePhysicalArea * mapScale;

  // Size of the map area in pixels
  QSize mapPixelArea = 
    map->mapToProj().mapRect(QRectF(QPointF(0, 0), mapPhysicalArea)).size().toSize();
  //  QSizeF mapPixelSize = proj->pixelSize();
  //  QSize mapPixelArea(mapPhysicalArea.width() / mapPixelSize.width(), 
  //                     mapPhysicalArea.height() / -mapPixelSize.height());

  QPoint mapPixelTopLeft = view->viewTopLeft();
  QRect mapPixelRect(mapPixelTopLeft, mapPixelArea);

  qreal scaleX = pageRect.width() / mapPixelArea.width();
  qreal scaleY = pageRect.height() / mapPixelArea.height();
  qreal scale = std::min(1.0, std::max(scaleX, scaleY));

  int layer = 0;
  renderer->loadTiles(layer, mapPixelRect, scale, true);

  printf("Logical dpi is %d %d Physical dpi is %d %d\n", printer.logicalDpiX(), printer.logicalDpiY(), 
         printer.physicalDpiX(), printer.physicalDpiY());
  printf("Page is %d x %d = %f m x %f m\n", pageRect.width(), pageRect.height(), pagePhysicalArea.width(),
         pagePhysicalArea.height());
  printf("Map is %d x %d = %f m x %f m\n", mapPixelArea.width(), mapPixelArea.height(), mapPhysicalArea.width(),
         mapPhysicalArea.height());

  QPainter painter;
  painter.begin(&printer);
  painter.scale(scaleX / scale, scaleY / scale);
  renderer->render(painter, layer, mapPixelRect, scale);
  painter.end();
}

void MainWindow::createWidgets()
{
  // Create the status bar

  posLine = new QLineEdit();
  posLine->setMaxLength(32);
  connect(posLine, SIGNAL(editingFinished()), this, SLOT(posEditingFinished()));
  posValidator = new QRegExpValidator(this);
  posValidator->setRegExp(currentCoordFormatter()->getRegExp());
  posLine->setValidator(posValidator);

  QWidget *scaleWidget = new QWidget();
  QHBoxLayout *scaleLayout = new QHBoxLayout();
  scaleLayout->setSizeConstraint(QLayout::SetMinimumSize);
  scaleLayout->setContentsMargins(1, 0, 1, 0);
  scaleLayout->setSpacing(0);
  scaleLine = new QLineEdit();
  QValidator *scaleValidator = new QIntValidator(100, 100000000, this);
  scaleLine->setValidator(scaleValidator);
  scaleLayout->addWidget(new QLabel(tr("1:")));
  scaleLayout->addWidget(scaleLine);
  scaleWidget->setLayout(scaleLayout);
  connect(scaleLine, SIGNAL(editingFinished()), this, SLOT(scaleEditingFinished()));

  statusBar()->addPermanentWidget(posLine);
  statusBar()->addPermanentWidget(scaleWidget);


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
  pageSetupAction = new QAction(tr("Page &Setup..."), this);
  connect(pageSetupAction, SIGNAL(triggered(bool)), this, 
          SLOT(pageSetupTriggered(bool)));

  printAction = new QAction(tr("&Print..."), this);
  printAction->setShortcuts(QKeySequence::Print);
  connect(printAction, SIGNAL(triggered(bool)), this, SLOT(printTriggered(bool)));


  // Map Layers
  layerActionGroup = new QActionGroup(this);

  QAction *autoLayer = new QAction(tr("Automatic"), this);
  autoLayer->setData(QVariant(-1));
  autoLayer->setCheckable(true);
  layerActionGroup->addAction(autoLayer);

  for (int i = 0; i < map->numLayers(); i++) {
    QAction *a = new QAction(map->layer(i).label, this);
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

  zoomInAction = new QAction(tr("Zoom &In"), this);
  connect(zoomInAction, SIGNAL(triggered()),
          this, SLOT(zoomInTriggered()));
  zoomInAction->setShortcuts(QKeySequence::ZoomIn);
  zoomOutAction = new QAction(tr("Zoom &Out"), this);
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

}

void MainWindow::createMenus()
{
  // Create the tool bar
  QToolBar *tb = new QToolBar(this);
  tb->setObjectName("ToolBar");
  tb->addAction(zoomInAction);
  tb->addAction(zoomOutAction);
  addToolBar(tb);

  // Create the menu bar
  QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
  fileMenu->addAction(printAction);
  fileMenu->addAction(pageSetupAction);

  QMenu *viewMenu = menuBar()->addMenu(tr("&View"));

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


  QMenu *windowMenu = menuBar()->addMenu(tr("&Window"));
  windowMenu->addAction(minimizeAction);
  windowMenu->addAction(zoomAction);
  windowMenu->addSeparator();
  windowMenu->addAction(bringFrontAction);
  
}

void MainWindow::readSettings()
{
  QSettings settings;
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
  posValidator->setRegExp(coordFormats[idx]->getRegExp());
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


void MainWindow::posEditingFinished()
{
  Projection *pjGeo = Geographic::getProjection(currentDatum());
  QPointF gc = pjGeo->transformFrom(map->projection(), view->center());
  QPointF gp;
  if (currentCoordFormatter()->parse(currentDatum(), gc, posLine->text(), gp)) {
    QPointF p = map->projection()->transformFrom(pjGeo, gp);
    QPoint mp = map->projToMap().map(p).toPoint();
    view->centerOn(mp);
    lastCursorPos = mp;
  }
}

void MainWindow::updatePosition(QPoint m)
{
  lastCursorPos = m;

  Datum d = currentDatum();
  QPointF g = Geographic::getProjection(d)->transformFrom(map->projection(), 
                                             map->mapToProj().map(QPointF(m)));

  posLine->setText(currentCoordFormatter()->format(d, g));
}

void MainWindow::scaleEditingFinished()
{
  qreal scale = (map->mapPixelSize().width() * screenDotsPerMeter) /
    qreal(scaleLine->text().toUInt());
  view->setScale(scale);
}

void MainWindow::scaleChanged(float scaleFactor)
{
  //  printf("dpi %d %d phys %d %d\n", logicalDpiX(), logicalDpiY(), physicalDpiX(), physicalDpiY());
  qreal scale = (map->mapPixelSize().width() * screenDotsPerMeter) 
    / scaleFactor; 
  scaleLine->setText(QString::number(int(scale)));
}
