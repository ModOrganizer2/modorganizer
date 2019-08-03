#ifndef SETTINGSDIALOGPLUGINS_H
#define SETTINGSDIALOGPLUGINS_H

#include "settings.h"
#include "settingsdialog.h"

class PluginsSettingsTab : public SettingsTab
{
public:
  PluginsSettingsTab(Settings *m_parent, SettingsDialog &m_dialog);

  void update();
  void closing() override;

private:
  void on_pluginsList_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
  void deleteBlacklistItem();
  void storeSettings(QListWidgetItem *pluginItem);
};

#endif // SETTINGSDIALOGPLUGINS_H
