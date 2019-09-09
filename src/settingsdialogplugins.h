#ifndef SETTINGSDIALOGPLUGINS_H
#define SETTINGSDIALOGPLUGINS_H

#include "settings.h"
#include "settingsdialog.h"

class PluginsSettingsTab : public SettingsTab
{
public:
  PluginsSettingsTab(Settings& settings, SettingsDialog& dialog);

  void update();
  void closing() override;

private:
  void on_pluginsList_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
  void deleteBlacklistItem();
  void storeSettings(QListWidgetItem *pluginItem);
};

#endif // SETTINGSDIALOGPLUGINS_H
