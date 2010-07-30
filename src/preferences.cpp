#include "preferences.h"

PreferencesDialog::PreferencesDialog(Cache::Cache &c, QWidget *parent)
  : QDialog(parent), cache(c)
{
  ui.setupUi(this);
  connect(ui.emptyCacheButton, SIGNAL(clicked(bool)), this, SLOT(emptyCacheClick()));
  ui.defaultDPILabel->setText(QString::number(logicalDpiX()));
}

int PreferencesDialog::getDpi()
{
  if (ui.defaultDPIButton->isChecked()) {
    return 0;
  } else {
    return ui.customDPIBox->value();
  }
}

int PreferencesDialog::getMemSize()
{
  return ui.memCacheSize->value();
}

int PreferencesDialog::getDiskSize()
{
  return ui.diskCacheSize->value();
}

bool PreferencesDialog::getUseOpenGL()
{
  return ui.openglCheckBox->isChecked();
}

void PreferencesDialog::setUseOpenGL(bool use)
{
  ui.openglCheckBox->setChecked(use);
}

void PreferencesDialog::setDpi(int dpi)
{
  if (dpi <= 0) {
    ui.defaultDPIButton->setChecked(true);
  } else {
    ui.customDPIButton->setChecked(true);
    ui.customDPIBox->setValue(dpi);
  }
}

void PreferencesDialog::setCacheSizes(int memSize, int diskSize)
{
  ui.memCacheSize->setValue(memSize);
  ui.diskCacheSize->setValue(diskSize);
}

void PreferencesDialog::emptyCacheClick()
{
  cache.emptyDiskCache();
}

