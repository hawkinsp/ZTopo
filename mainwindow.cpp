#include <QComboBox>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QStringBuilder>
#include "mainwindow.h"
#include "mapview.h"
#include "mapscene.h"
#include "map.h"
#include "mapprojection.h"
#include "consts.h"

#include <iostream>

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow(parent)
{
  setWindowTitle(tr("Topographic Map Viewer"));
  resize(800, 600);

  QStringList layers;
  layers << tr("Auto") << tr("1:500k") << tr("1:100k") << tr("1:24k");
  layerCombo = new QComboBox();
  layerCombo->addItems(layers);
  statusBar()->addPermanentWidget(layerCombo);

  // Create the status bar
  QStringList datums;
  datums << tr("NAD27") << tr("NAD83");
  datumCombo = new QComboBox();
  datumCombo->addItems(datums);
  statusBar()->addPermanentWidget(datumCombo);  

  QStringList formats;
  formats << tr("UTM") << tr("DD.MMM") << tr("D.M'S\"");
  coordFormatCombo = new QComboBox();
  coordFormatCombo->addItems(formats);
  statusBar()->addPermanentWidget(coordFormatCombo);  

  posLabel = new QLabel();
  scaleLabel = new QLabel();
  statusBar()->addPermanentWidget(posLabel);
  statusBar()->addPermanentWidget(scaleLabel);

  // Create the main view
  proj = new MapProjection();
  map = new Map(proj);
  scene = new MapScene(map);
  view = new MapView(scene);
  view->centerOn(389105.0, 366050.0);
  view->update();
  setCentralWidget(view);
  connect(view, SIGNAL(positionUpdated(QPoint)), this, SLOT(updatePosition(QPoint)));
  connect(view, SIGNAL(scaleChanged(float)), this, SLOT(scaleChanged(float)));
  updatePosition(view->mapToScene(view->rect().center()).toPoint());
  scaleChanged(1.0);

  createActions();
  createMenus();

}

MainWindow::~MainWindow()
{
  delete map;
  delete proj;
}

void MainWindow::createActions()
{
  printAction = new QAction(tr("&Print..."), this);
  showGridAction = new QAction(tr("Show &Grid"), this);
  showGridAction->setCheckable(true);
  connect(showGridAction, SIGNAL(toggled(bool)),
          view, SLOT(setGridDisplayed(bool)));
}

void MainWindow::createMenus()
{
  // Create the menu bar
  QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
  fileMenu->addAction(printAction);

  QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
  viewMenu->addAction(showGridAction);

  QMenu *windowMenu = menuBar()->addMenu(tr("&Window"));
  
}

void MainWindow::updatePosition(QPoint mPos)
{
  QPointF pPos = proj->mapToProj(QPointF(mPos));
  posLabel->setText(QString::number(pPos.x()) % ", " % QString::number(pPos.y()));
}

void MainWindow::scaleChanged(float scaleFactor)
{
  float screenMetersPerDot = (metersPerInch / (float)logicalDpiX());
  float scale = (proj->pixelSize() / screenMetersPerDot) / scaleFactor; 
  scaleLabel->setText(
    QString("1:" % QString::number((int)scale)).rightJustified(11, ' ')
  );
}
