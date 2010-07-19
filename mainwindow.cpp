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
#include <QSizeF>
#include <QStatusBar>
#include <QStringBuilder>
#include "mainwindow.h"
//#include "mapview.h"
//#include "mapscene.h"
#include "mapwidget.h"
#include "map.h"
#include "mapprojection.h"
#include "maprenderer.h"
#include "projection.h"
#include "consts.h"

#include <iostream>

// Coordinate formats; must match 
enum CoordFormat {
  FormatDMS = 0, // Degrees/minutes/seconds
  FormatDecimalDegrees = 1,
  FormatUTM = 2
};

const QChar degree(0x00b0);


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

  //  scene = new MapScene(map);
  //  view = new MapView(scene);
  view->centerOn(QPoint(389105, 366050));
  connect(view, SIGNAL(positionUpdated(QPoint)), this, SLOT(updatePosition(QPoint)));
  connect(view, SIGNAL(scaleChanged(float)), this, SLOT(scaleChanged(float)));
  scaleChanged(1.0);
  updatePosition(view->center());

  createActions();
  createMenus();

}

MainWindow::~MainWindow()
{
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
  QStringList layers;
  layers << tr("Auto");
  for (int i = 0; i < map->numLayers(); i++) {
    layers << map->layer(i).label;
  }
  layerCombo = new QComboBox();
  layerCombo->addItems(layers);
  statusBar()->addPermanentWidget(layerCombo);
  connect(layerCombo, SIGNAL(currentIndexChanged(int)),
          this, SLOT(layerChanged(int)));

  QStringList coordFormats;
  QStringList datums; 
  coordFormats << trUtf8("DDD\xc2\xb0MM'SS\"") 
               << trUtf8("DDD.MMM\xc2\xb0") 
               << tr("UTM");

  datums << datumName(NAD27) << datumName(NAD83);

  datumCombo = new QComboBox();
  datumCombo->addItems(datums);
  statusBar()->addPermanentWidget(datumCombo);  
  connect(datumCombo, SIGNAL(currentIndexChanged(int)), 
          this, SLOT(coordSystemChanged(int)));

  coordFormatCombo = new QComboBox();
  coordFormatCombo->addItems(coordFormats);
  statusBar()->addPermanentWidget(coordFormatCombo);  
  connect(coordFormatCombo, SIGNAL(currentIndexChanged(int)), 
          this, SLOT(coordSystemChanged(int)));

  gridCombo = new QComboBox();
  QStringList gridOptions;
  for (int i = 0; i < grids.size(); i++) {
    gridOptions << grids[i].label;
  }
  gridCombo->addItems(gridOptions);
  statusBar()->addPermanentWidget(gridCombo);  
  connect(gridCombo, SIGNAL(currentIndexChanged(int)), 
          this, SLOT(gridChanged(int)));



  posLine = new QLineEdit();
  posLine->setMaxLength(32);

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

  statusBar()->addPermanentWidget(posLine);
  statusBar()->addPermanentWidget(scaleWidget);


  // Create the print dock widget
  printDock = new QDockWidget(tr("Print"), this);
  printDock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
  printDock->setWidget(new QLabel("hello"));
  printDock->hide();
  addDockWidget(Qt::BottomDockWidgetArea, printDock);


  // Create the main view
  view = new MapWidget(map, renderer);
  setCentralWidget(view);

}

void MainWindow::createActions()
{
  pageSetupAction = new QAction(tr("Page &Setup..."), this);
  connect(pageSetupAction, SIGNAL(triggered(bool)), this, 
          SLOT(pageSetupTriggered(bool)));

  printAction = new QAction(tr("&Print..."), this);
  connect(printAction, SIGNAL(triggered(bool)), this, SLOT(printTriggered(bool)));

  showGridAction = new QAction(tr("Show &Grid"), this);
  showGridAction->setCheckable(true);
  //  connect(showGridAction, SIGNAL(toggled(bool)),
  //          view, SLOT(setGridDisplayed(bool)));
}

void MainWindow::createMenus()
{
  // Create the menu bar
  QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
  fileMenu->addAction(printAction);
  fileMenu->addAction(pageSetupAction);

  QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
  viewMenu->addAction(showGridAction);

  QMenu *windowMenu = menuBar()->addMenu(tr("&Window"));
  
}

void MainWindow::layerChanged(int layer)
{
  // Layer combo contains "Automatic" as an additional first choice;
  // Subtract 1 to get the format required by MapWidget::setLayer().
  view->setLayer(layer - 1);
}

void MainWindow::coordSystemChanged(int)
{
  updatePosition(lastCursorPos);
  gridChanged(gridCombo->currentIndex());
}

void MainWindow::gridChanged(int g)
{

  if (grids[g].enabled) {
    Datum d = Datum(datumCombo->currentIndex());
    view->showGrid(d, grids[g].utm, grids[g].interval);
  } else {
    view->hideGrid();
  }
}

void MainWindow::updatePosition(QPoint m)
{
  lastCursorPos = m;
  if (posLine->isModified()) return;

  CoordFormat format = CoordFormat(coordFormatCombo->currentIndex());
  Datum d = Datum(datumCombo->currentIndex());
  QPointF g = Geographic::getProjection(d)->transformFrom(map->projection(), 
                                             map->mapToProj().map(QPointF(m)));
    
  switch (format) {
  case FormatDMS: {
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

    posLine->setText(s);
    break;
  }

  case FormatDecimalDegrees: {
    QString s = 
      QString::number(fabs(g.y()), 'f', 5) % degree % (g.y() >= 0 ? 'N' : 'S')
      % " " % 
      QString::number(fabs(g.x()), 'f', 5) % degree % (g.x() >= 0 ? 'E' : 'W');
    posLine->setText(s);
    break;
  }
  case FormatUTM: {
    UTM::Zone z = UTM::bestZone(g);
    Projection *pjUTM = UTM::getZoneProjection(d, z.zone);
    QPointF p = pjUTM->transformFrom(Geographic::getProjection(d), g);

    QString s = QString::number(z.zone) % z.band % " " %
      QString::number(p.y(), 'f', 0) % "mN " %
      QString::number(p.x(), 'f', 0) % "mE";
    
    posLine->setText(s);
    break;
  }
  default: abort();
  }

}

void MainWindow::scaleChanged(float scaleFactor)
{
  if (!scaleLine->isModified()) {
    qreal screenDotsPerMeter = ((float)logicalDpiX() / metersPerInch);
    //  printf("dpi %d %d phys %d %d\n", logicalDpiX(), logicalDpiY(), physicalDpiX(), physicalDpiY());
    qreal scale = (map->mapPixelSize().width() * screenDotsPerMeter) 
      / scaleFactor; 
    scaleLine->setText(QString::number(int(scale)));
  }
}
