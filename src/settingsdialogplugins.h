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

  /**
   * @brief Retrieve the plugin associated to the given item in the list.
   *
   */
  MOBase::IPlugin* plugin(QListWidgetItem *pluginItem) const;

  constexpr static int ROLE_PLUGIN = Qt::UserRole;
  constexpr static int ROLE_SETTINGS = Qt::UserRole + 1;
  constexpr static int ROLE_DESCRIPTIONS = Qt::UserRole + 2;
};

#endif // SETTINGSDIALOGPLUGINS_H
