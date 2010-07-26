#ifndef PREFERENCES_H
#define PREFERENCES_H 1
#include <QDialog>
#include "ui_preferences.h"
#include "tilecache.h"

class PreferencesDialog : public QDialog
{
  Q_OBJECT;
public:
  PreferencesDialog(Cache::Cache &cache, QWidget *parent = NULL);

  int getDpi();
  int getMemSize();
  int getDiskSize();

  void setDpi(int customValue);
  void setCacheSizes(int memSize, int diskSize);

private slots:
  void emptyCacheClick();

private:
  Ui::PreferencesDialog ui;
  Cache::Cache &cache;
};

#endif
