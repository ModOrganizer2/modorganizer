#ifndef SETTINGSDIALOGPLUGINS_H
#define SETTINGSDIALOGPLUGINS_H

#include "filterwidget.h"

#include "settings.h"
#include "settingsdialog.h"

class PluginsSettingsTab : public SettingsTab
{
public:
  PluginsSettingsTab(Settings& settings, PluginManager& pluginManager,
                     SettingsDialog& dialog);

  void update();
  void closing() override;

private:
  void on_pluginsList_currentItemChanged(QTreeWidgetItem* current,
                                         QTreeWidgetItem* previous);
  void deleteBlacklistItem();
  void storeSettings(QTreeWidgetItem* pluginItem);

private slots:

  /**
   * @brief Update the list item to display inactive plugins.
   */
  void updateListItems();

  /**
   * @brief Filter the plugin list according to the filter widget.
   *
   */
  void filterPluginList();

  /**
   * @brief Retrieve the plugin associated to the given item in the list.
   *
   */
  MOBase::IPlugin* plugin(QTreeWidgetItem* pluginItem) const;

  enum
  {
    PluginRole       = Qt::UserRole,
    SettingsRole     = Qt::UserRole + 1,
    DescriptionsRole = Qt::UserRole + 2
  };

private:
  PluginManager* m_pluginManager;

  MOBase::FilterWidget m_filter;
};

#endif  // SETTINGSDIALOGPLUGINS_H
