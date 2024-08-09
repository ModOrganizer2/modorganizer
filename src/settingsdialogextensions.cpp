#include "settingsdialogextensions.h"
#include "noeditdelegate.h"
#include "ui_settingsdialog.h"
#include <iplugin.h>

#include "organizercore.h"
#include "pluginmanager.h"

#include "settingsdialogextensionrow.h"

using namespace MOBase;

struct PluginExtensionComparator
{
  bool operator()(const PluginExtension* lhs, const PluginExtension* rhs) const
  {
    return lhs->metadata().name().compare(rhs->metadata().name(), Qt::CaseInsensitive);
  }
};

ExtensionsSettingsTab::ExtensionsSettingsTab(Settings& s,
                                             ExtensionManager& extensionManager,
                                             PluginManager& pluginManager,
                                             SettingsDialog& d)
    : SettingsTab(s, d), m_extensionManager(&extensionManager),
      m_pluginManager(&pluginManager)
{
  // TODO: use Qt system to sort extensions instead of sorting beforehand
  std::vector<const IExtension*> extensions;
  for (auto& extension : m_extensionManager->extensions()) {
    extensions.push_back(extension.get());
  }
  std::sort(extensions.begin(), extensions.end(), [](auto* lhs, auto* rhs) {
    return lhs->metadata().name().compare(rhs->metadata().name(), Qt::CaseInsensitive) <
           0;
  });

  ui->extensionsList->setSortingEnabled(false);

  for (const auto* extension : extensions) {
    auto* item   = new QListWidgetItem();
    auto* widget = new ExtensionListItemWidget(*extension);
    item->setSizeHint(widget->sizeHint());
    ui->extensionsList->addItem(item);
    ui->extensionsList->setItemWidget(item, widget);
  }

  // ui->pluginSettingsList->setStyleSheet("QTreeWidget::item {padding-right: 10px;}");
  // ui->pluginsList->setHeaderHidden(true);

  //// display plugin settings
  // std::map<const PluginExtension*, QList<IPlugin*>, PluginExtensionComparator>
  //     pluginsPerExtension;

  // for (IPlugin* plugin : pluginManager.plugins()) {
  //   pluginsPerExtension[&pluginManager.details(plugin).extension()].push_back(plugin);
  // }

  // for (auto& [extension, plugins] : pluginsPerExtension) {

  //  QTreeWidgetItem* extensionItem = new QTreeWidgetItem();
  //  extensionItem->setData(0, Qt::DisplayRole, extension->metadata().name());
  //  ui->pluginsList->addTopLevelItem(extensionItem);

  //  for (auto* plugin : plugins) {

  //    // only show master
  //    if (pluginManager.details(plugin).master() != plugin) {
  //      continue;
  //    }

  //    QTreeWidgetItem* pluginItem = new QTreeWidgetItem(extensionItem);
  //    pluginItem->setData(0, Qt::DisplayRole, plugin->localizedName());
  //  }
  //}

  // ui->pluginsList->sortByColumn(0, Qt::AscendingOrder);

  //// display plugin blacklist
  // for (const QString& pluginName : settings().plugins().blacklist()) {
  //   ui->pluginBlacklist->addItem(pluginName);
  // }

  // m_filter.setEdit(ui->pluginFilterEdit);

  // QObject::connect(ui->pluginsList, &QTreeWidget::currentItemChanged,
  //                  [&](auto* current, auto* previous) {
  //                    on_pluginsList_currentItemChanged(current, previous);
  //                  });

  // QShortcut* delShortcut =
  //     new QShortcut(QKeySequence(Qt::Key_Delete), ui->pluginBlacklist);
  // QObject::connect(delShortcut, &QShortcut::activated, &dialog(), [&] {
  //   deleteBlacklistItem();
  // });
  // QObject::connect(&m_filter, &FilterWidget::changed, [&] {
  //   filterPluginList();
  // });

  // updateListItems();
  // filterPluginList();
}
//
// void PluginsSettingsTab::updateListItems()
//{
//  for (auto i = 0; i < ui->pluginsList->topLevelItemCount(); ++i) {
//    auto* topLevelItem = ui->pluginsList->topLevelItem(i);
//    for (auto j = 0; j < topLevelItem->childCount(); ++j) {
//      auto* item   = topLevelItem->child(j);
//      auto* plugin = this->plugin(item);
//
//      bool inactive = !m_pluginManager->implementInterface<IPluginGame>(plugin) &&
//                      !m_pluginManager->isEnabled(plugin);
//
//      auto font = item->font(0);
//      font.setItalic(inactive);
//      item->setFont(0, font);
//      for (auto k = 0; k < item->childCount(); ++k) {
//        item->child(k)->setFont(0, font);
//      }
//    }
//  }
//}
//
// void PluginsSettingsTab::filterPluginList()
//{
//  auto selectedItems              = ui->pluginsList->selectedItems();
//  QTreeWidgetItem* firstNotHidden = nullptr;
//
//  for (auto i = 0; i < ui->pluginsList->topLevelItemCount(); ++i) {
//    auto* topLevelItem = ui->pluginsList->topLevelItem(i);
//
//    bool found = false;
//    for (auto j = 0; j < topLevelItem->childCount(); ++j) {
//      auto* item   = topLevelItem->child(j);
//      auto* plugin = this->plugin(item);
//
//      // Check the item or the child - If any match (item or child), the whole
//      // group is displayed.
//      bool match = m_filter.matches([plugin](const QRegularExpression& regex) {
//        return regex.match(plugin->localizedName()).hasMatch();
//      });
//
//      if (match) {
//        found = true;
//        item->setHidden(false);
//
//        if (firstNotHidden == nullptr) {
//          firstNotHidden = item;
//        }
//      } else {
//        item->setHidden(true);
//      }
//    }
//
//    topLevelItem->setHidden(!found);
//  }
//
//  // Unselect item if hidden:
//  if (firstNotHidden) {
//    ui->pluginDescription->setVisible(true);
//    ui->pluginSettingsList->setVisible(true);
//    ui->noPluginLabel->setVisible(false);
//    if (selectedItems.isEmpty()) {
//      ui->pluginsList->setCurrentItem(firstNotHidden);
//    } else if (selectedItems[0]->isHidden()) {
//      ui->pluginsList->setCurrentItem(firstNotHidden);
//    }
//  } else {
//    ui->pluginDescription->setVisible(false);
//    ui->pluginSettingsList->setVisible(false);
//    ui->noPluginLabel->setVisible(true);
//  }
//}
//
// IPlugin* PluginsSettingsTab::plugin(QTreeWidgetItem* pluginItem) const
//{
//  return static_cast<IPlugin*>(qvariant_cast<void*>(pluginItem->data(0, PluginRole)));
//}

void ExtensionsSettingsTab::update()
{
  // transfer plugin settings to in-memory structure
  // for (int i = 0; i < ui->pluginsList->topLevelItemCount(); ++i) {
  //  auto* topLevelItem = ui->pluginsList->topLevelItem(i);
  //  for (int j = 0; j < topLevelItem->childCount(); ++j) {
  //    auto* item = topLevelItem->child(j);
  //    settings().plugins().setSettings(plugin(item)->name(),
  //                                     item->data(0, SettingsRole).toMap());
  //  }
  //}

  // set plugin blacklist
  QStringList names;
  for (QListWidgetItem* item : ui->pluginBlacklist->findItems("*", Qt::MatchWildcard)) {
    names.push_back(item->text());
  }

  settings().plugins().setBlacklist(names);

  settings().plugins().save();
}

void ExtensionsSettingsTab::closing()
{
  // storeSettings(ui->pluginsList->currentItem());
}
//
// void PluginsSettingsTab::on_pluginsList_currentItemChanged(QTreeWidgetItem* current,
//                                                           QTreeWidgetItem* previous)
//{
//  storeSettings(previous);
//
//  if (!current->data(0, PluginRole).isValid()) {
//    return;
//  }
//
//  ui->pluginSettingsList->clear();
//  IPlugin* plugin = this->plugin(current);
//  // ui->authorLabel->setText(plugin->author());
//  // ui->versionLabel->setText(plugin->version().canonicalString());
//  // ui->descriptionLabel->setText(plugin->description());
//
//  //// Checkbox, do not show for children or game plugins, disable
//  //// if the plugin cannot be enabled.
//  // ui->enabledCheckbox->setVisible(
//  //     !m_pluginManager->implementInterface<IPluginGame>(plugin) &&
//  //     plugin->master().isEmpty());
//
//  bool enabled       = m_pluginManager->isEnabled(plugin);
//  auto& requirements = m_pluginManager->details(plugin);
//  auto problems      = requirements.problems();
//
//  // Plugin is enable or can be enabled.
//  if (enabled || problems.empty()) {
//    ui->enabledCheckbox->setDisabled(false);
//    ui->enabledCheckbox->setToolTip("");
//    ui->enabledCheckbox->setChecked(enabled);
//  }
//  // Plugin is disable and cannot be enabled.
//  else {
//    ui->enabledCheckbox->setDisabled(true);
//    ui->enabledCheckbox->setChecked(false);
//    if (problems.size() == 1) {
//      ui->enabledCheckbox->setToolTip(problems[0].shortDescription());
//    } else {
//      QStringList descriptions;
//      for (auto& problem : problems) {
//        descriptions.append(problem.shortDescription());
//      }
//      ui->enabledCheckbox->setToolTip("<ul><li>" + descriptions.join("</li><li>") +
//                                      "</li></ul>");
//    }
//  }
//
//  QVariantMap settings     = current->data(0, SettingsRole).toMap();
//  QVariantMap descriptions = current->data(0, DescriptionsRole).toMap();
//  ui->pluginSettingsList->setEnabled(settings.count() != 0);
//  for (auto iter = settings.begin(); iter != settings.end(); ++iter) {
//    QTreeWidgetItem* newItem = new QTreeWidgetItem(QStringList(iter.key()));
//    QVariant value           = *iter;
//    QString description;
//    {
//      auto descriptionIter = descriptions.find(iter.key());
//      if (descriptionIter != descriptions.end()) {
//        description = descriptionIter->toString();
//      }
//    }
//
//    ui->pluginSettingsList->setItemDelegateForColumn(0, new NoEditDelegate());
//    newItem->setData(1, Qt::DisplayRole, value);
//    newItem->setData(1, Qt::EditRole, value);
//    newItem->setToolTip(1, description);
//
//    newItem->setFlags(newItem->flags() | Qt::ItemIsEditable);
//    ui->pluginSettingsList->addTopLevelItem(newItem);
//  }
//
//  ui->pluginSettingsList->resizeColumnToContents(0);
//  ui->pluginSettingsList->resizeColumnToContents(1);
//}
//
// void PluginsSettingsTab::deleteBlacklistItem()
//{
//  ui->pluginBlacklist->takeItem(ui->pluginBlacklist->currentIndex().row());
//}
//
// void PluginsSettingsTab::storeSettings(QTreeWidgetItem* pluginItem)
//{
//  if (pluginItem != nullptr && pluginItem->data(0, PluginRole).isValid()) {
//    QVariantMap settings = pluginItem->data(0, SettingsRole).toMap();
//
//    for (int i = 0; i < ui->pluginSettingsList->topLevelItemCount(); ++i) {
//      const QTreeWidgetItem* item = ui->pluginSettingsList->topLevelItem(i);
//      settings[item->text(0)]     = item->data(1, Qt::DisplayRole);
//    }
//
//    pluginItem->setData(0, SettingsRole, settings);
//  }
//}
