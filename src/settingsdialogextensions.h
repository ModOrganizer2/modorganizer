#ifndef SETTINGSDIALOGEXTENSIONS_H
#define SETTINGSDIALOGEXTENSIONS_H

#include "filterwidget.h"

#include "settings.h"
#include "settingsdialog.h"

#include "extensionmanager.h"
#include "pluginmanager.h"

class ExtensionsSettingsTab : public SettingsTab
{
public:
  ExtensionsSettingsTab(Settings& settings, ExtensionManager& extensionManager,
                        PluginManager& pluginManager, SettingsDialog& dialog);

  void update();
  void closing() override;

  // private:
  //   void on_pluginsList_currentItemChanged(QListWidgetItem* current,
  //                                          QListWidgetItem* previous);
  //   void deleteBlacklistItem();
  //   void storeSettings(QListWidgetItem* pluginItem);

private slots:

  ///**
  // * @brief Update the list item to display inactive plugins.
  // */
  // void updateListItems();

  ///**
  // * @brief Filter the plugin list according to the filter widget.
  // *
  // */
  // void filterPluginList();

  ///**
  // * @brief Retrieve the plugin associated to the given item in the list.
  // *
  // */
  // MOBase::IPlugin* plugin(QListWidgetItem* pluginItem) const;

  void extensionSelected(MOBase::IExtension const& extension);

  enum
  {
    PluginRole       = Qt::UserRole,
    SettingsRole     = Qt::UserRole + 1,
    DescriptionsRole = Qt::UserRole + 2
  };

private:
  ExtensionManager* m_extensionManager;
  PluginManager* m_pluginManager;

  // the currently selected extension
  const MOBase::IExtension* m_currentExtension{nullptr};

  MOBase::FilterWidget m_filter;
};

#endif  // SETTINGSDIALOGPLUGINS_H
