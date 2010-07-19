#ifndef MAINWINDOW_H
#define MAINWINDOW_H 1

#include <QMainWindow>
#include <QString>
#include <QVector>

class QComboBox;
class QDockWidget;
class QLabel;
class QLineEdit;

class Map;
class MapProjection;
class MapRenderer;
class MapWidget;

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

private slots:
  void coordSystemChanged(int);
  void gridChanged(int);
  void layerChanged(int);

private:
  Map *map;
  MapRenderer *renderer;
  MapWidget *view;

  QPoint lastCursorPos;

  QVector<Grid> grids;

  // Status bar
  QLineEdit *posLine;     // Current position
  QLineEdit *scaleLine;   // Current scale
  QComboBox *layerCombo;  // Current map layer
  QComboBox *gridCombo;  // Current map layer
  QComboBox *datumCombo;  // Current geodetic datum
  QComboBox *coordFormatCombo; // Current coordinate display format

  // Print dock widget
  QDockWidget *printDock;

  // Actions
  QAction *printAction;
  QAction *pageSetupAction;
  QAction *showGridAction;

  void createActions();
  void createMenus();
  void createWidgets();
};


#endif
