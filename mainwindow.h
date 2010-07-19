#ifndef MAINWINDOW_H
#define MAINWINDOW_H 1

#include <QMainWindow>
#include <QString>
#include <QVector>

#include "projection.h"

class QActionGroup;
class QComboBox;
class QDockWidget;
class QLabel;
class QLineEdit;
class QRegExpValidator;

class Map;
class MapProjection;
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

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  MainWindow(QWidget *parent = 0);
  ~MainWindow();

public slots:
  void updatePosition(QPoint pos);
  void scaleChanged(float scale);

  void pageSetupTriggered(bool);
  void printTriggered(bool);

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

  void minimizeTriggered();
  void windowZoomTriggered();
  void bringFrontTriggered();

  void scaleEditingFinished();
  void posEditingFinished();
 

private:
  Map *map;
  MapRenderer *renderer;
  MapWidget *view;

  // Last cursor position in map coordinates.
  QPoint lastCursorPos;

  QVector<CoordFormatter *> coordFormats;

  // Possible grids to display
  QVector<Grid> grids;

  // Dots per meter of the current screen
  qreal screenDotsPerMeter;


  // Status bar
  QLineEdit *posLine;     // Current position
  QRegExpValidator *posValidator;
  QLineEdit *scaleLine;   // Current scale

  // Print dock widget
  QDockWidget *printDock;

  // Actions
  QAction *printAction;
  QAction *pageSetupAction;

  QActionGroup *layerActionGroup;
  QActionGroup *coordFormatActionGroup;
  QActionGroup *datumActionGroup;
  QActionGroup *gridActionGroup;
  QAction *showRulerAction;
  
  QAction *zoomInAction, *zoomOutAction;


  QAction *minimizeAction;
  QAction *zoomAction;
  QAction *bringFrontAction;

  void createActions();
  void createMenus();
  void createWidgets();
  void readSettings();

  Datum currentDatum();
  CoordFormatter *currentCoordFormatter();
};


#endif
