#ifndef MAINWINDOW_H
#define MAINWINDOW_H 1

#include <QMainWindow>

class QLabel;
class QComboBox;
class Map;
class MapProjection;
class MapWidget;

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  MainWindow(QWidget *parent = 0);
  ~MainWindow();

public slots:
  void updatePosition(QPoint pos);
  void scaleChanged(float scale);

private:
  Map *map;
  MapProjection *proj;
  MapWidget *view;

  // Status bar
  QLabel *posLabel;       // Current position
  QLabel *scaleLabel;     // Current scale
  QComboBox *layerCombo;  // Current map layer
  QComboBox *datumCombo;  // Current geodetic datum
  QComboBox *coordFormatCombo; // Current coordinate display format

  // Actions
  QAction *printAction;
  QAction *showGridAction;

  void createActions();
  void createMenus();
};


#endif
