#ifndef SETTINGSDIALOGPLUGINS_H
#define SETTINGSDIALOGPLUGINS_H

#include "filterwidget.h"

#include "settings.h"
#include "settingsdialog.h"

class PluginsSettingsTab : public SettingsTab
{
public:
  PluginsSettingsTab(Settings& settings, PluginContainer* pluginContainer, SettingsDialog& dialog);

  void update();
  void closing() override;

private:
  void on_pluginsList_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
  void deleteBlacklistItem();
  void storeSettings(QTreeWidgetItem *pluginItem);

private slots:
  /**
   * @brief Clear and repopulate the plugin list.
   *
   */
  void populatePluginList();

  /**
   * @brief Retrieve the plugin associated to the given item in the list.
   *
   */
  MOBase::IPlugin* plugin(QTreeWidgetItem *pluginItem) const;

  constexpr static int ROLE_PLUGIN = Qt::UserRole;
  constexpr static int ROLE_SETTINGS = Qt::UserRole + 1;
  constexpr static int ROLE_DESCRIPTIONS = Qt::UserRole + 2;

private:

  PluginContainer* m_pluginContainer;

  MOBase::FilterWidget m_filter;
};

#endif // SETTINGSDIALOGPLUGINS_H
