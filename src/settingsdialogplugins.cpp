#include "settingsdialogplugins.h"
#include "noeditdelegate.h"
#include "ui_settingsdialog.h"
#include <iplugin.h>

#include "disableproxyplugindialog.h"
#include "organizercore.h"
#include "plugincontainer.h"

using namespace MOBase;

PluginsSettingsTab::PluginsSettingsTab(Settings& s, PluginContainer* pluginContainer,
                                       SettingsDialog& d)
    : SettingsTab(s, d), m_pluginContainer(pluginContainer)
{
  ui->pluginSettingsList->setStyleSheet("QTreeWidget::item {padding-right: 10px;}");

  // Create top-level tree widget:
  QStringList pluginInterfaces = m_pluginContainer->pluginInterfaces();
  pluginInterfaces.sort(Qt::CaseInsensitive);
  std::map<QString, QTreeWidgetItem*> topItems;
  for (QString interfaceName : pluginInterfaces) {
    auto* item = new QTreeWidgetItem(ui->pluginsList, {interfaceName});
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    auto font = item->font(0);
    font.setBold(true);
    item->setFont(0, font);
    topItems[interfaceName] = item;
    item->setExpanded(true);
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
  }
  ui->pluginsList->setHeaderHidden(true);

  // display plugin settings
  QSet<QString> handledNames;
  for (IPlugin* plugin : settings().plugins().plugins()) {
    if (handledNames.contains(plugin->name()) ||
        m_pluginContainer->requirements(plugin).master()) {
      continue;
    }

    QTreeWidgetItem* listItem = new QTreeWidgetItem(
        topItems.at(m_pluginContainer->topImplementedInterface(plugin)));
    listItem->setData(0, Qt::DisplayRole, plugin->localizedName());
    listItem->setData(0, PluginRole, QVariant::fromValue((void*)plugin));
    listItem->setData(0, SettingsRole, settings().plugins().settings(plugin->name()));
    listItem->setData(0, DescriptionsRole,
                      settings().plugins().descriptions(plugin->name()));

    // Handle child item:
    auto children = m_pluginContainer->requirements(plugin).children();
    for (auto* child : children) {
      QTreeWidgetItem* childItem = new QTreeWidgetItem(listItem);
      childItem->setData(0, Qt::DisplayRole, child->localizedName());
      childItem->setData(0, PluginRole, QVariant::fromValue((void*)child));
      childItem->setData(0, SettingsRole, settings().plugins().settings(child->name()));
      childItem->setData(0, DescriptionsRole,
                         settings().plugins().descriptions(child->name()));

      handledNames.insert(child->name());
    }

    handledNames.insert(plugin->name());
  }

  for (auto& [k, item] : topItems) {
    if (item->childCount() == 0) {
      item->setHidden(true);
    }
  }

  ui->pluginsList->sortByColumn(0, Qt::AscendingOrder);

  // display plugin blacklist
  for (const QString& pluginName : settings().plugins().blacklist()) {
    ui->pluginBlacklist->addItem(pluginName);
  }

  m_filter.setEdit(ui->pluginFilterEdit);

  QObject::connect(ui->pluginsList, &QTreeWidget::currentItemChanged,
                   [&](auto* current, auto* previous) {
                     on_pluginsList_currentItemChanged(current, previous);
                   });
  QObject::connect(ui->enabledCheckbox, &QCheckBox::clicked, [&](bool checked) {
    on_checkboxEnabled_clicked(checked);
  });

  QShortcut* delShortcut =
      new QShortcut(QKeySequence(Qt::Key_Delete), ui->pluginBlacklist);
  QObject::connect(delShortcut, &QShortcut::activated, &dialog(), [&] {
    deleteBlacklistItem();
  });
  QObject::connect(&m_filter, &FilterWidget::changed, [&] {
    filterPluginList();
  });

  updateListItems();
  filterPluginList();
}

void PluginsSettingsTab::updateListItems()
{
  for (auto i = 0; i < ui->pluginsList->topLevelItemCount(); ++i) {
    auto* topLevelItem = ui->pluginsList->topLevelItem(i);
    for (auto j = 0; j < topLevelItem->childCount(); ++j) {
      auto* item   = topLevelItem->child(j);
      auto* plugin = this->plugin(item);

      bool inactive = !m_pluginContainer->implementInterface<IPluginGame>(plugin) &&
                      !m_pluginContainer->isEnabled(plugin);

      auto font = item->font(0);
      font.setItalic(inactive);
      item->setFont(0, font);
      for (auto k = 0; k < item->childCount(); ++k) {
        item->child(k)->setFont(0, font);
      }
    }
  }
}

void PluginsSettingsTab::filterPluginList()
{
  auto selectedItems              = ui->pluginsList->selectedItems();
  QTreeWidgetItem* firstNotHidden = nullptr;

  for (auto i = 0; i < ui->pluginsList->topLevelItemCount(); ++i) {
    auto* topLevelItem = ui->pluginsList->topLevelItem(i);

    bool found = false;
    for (auto j = 0; j < topLevelItem->childCount(); ++j) {
      auto* item   = topLevelItem->child(j);
      auto* plugin = this->plugin(item);

      // Check the item or the child - If any match (item or child), the whole
      // group is displayed.
      bool match = m_filter.matches([plugin](const QRegularExpression& regex) {
        return regex.match(plugin->localizedName()).hasMatch();
      });
      for (auto* child : m_pluginContainer->requirements(plugin).children()) {
        match = match || m_filter.matches([child](const QRegularExpression& regex) {
          return regex.match(child->localizedName()).hasMatch();
        });
      }

      if (match) {
        found = true;
        item->setHidden(false);

        if (firstNotHidden == nullptr) {
          firstNotHidden = item;
        }
      } else {
        item->setHidden(true);
      }
    }

    topLevelItem->setHidden(!found);
  }

  // Unselect item if hidden:
  if (firstNotHidden) {
    ui->pluginDescription->setVisible(true);
    ui->pluginSettingsList->setVisible(true);
    ui->noPluginLabel->setVisible(false);
    if (selectedItems.isEmpty()) {
      ui->pluginsList->setCurrentItem(firstNotHidden);
    } else if (selectedItems[0]->isHidden()) {
      ui->pluginsList->setCurrentItem(firstNotHidden);
    }
  } else {
    ui->pluginDescription->setVisible(false);
    ui->pluginSettingsList->setVisible(false);
    ui->noPluginLabel->setVisible(true);
  }
}

IPlugin* PluginsSettingsTab::plugin(QTreeWidgetItem* pluginItem) const
{
  return static_cast<IPlugin*>(qvariant_cast<void*>(pluginItem->data(0, PluginRole)));
}

void PluginsSettingsTab::update()
{
  // transfer plugin settings to in-memory structure
  for (int i = 0; i < ui->pluginsList->topLevelItemCount(); ++i) {
    auto* topLevelItem = ui->pluginsList->topLevelItem(i);
    for (int j = 0; j < topLevelItem->childCount(); ++j) {
      auto* item = topLevelItem->child(j);
      settings().plugins().setSettings(plugin(item)->name(),
                                       item->data(0, SettingsRole).toMap());
    }
  }

  // set plugin blacklist
  QStringList names;
  for (QListWidgetItem* item : ui->pluginBlacklist->findItems("*", Qt::MatchWildcard)) {
    names.push_back(item->text());
  }

  settings().plugins().setBlacklist(names);

  settings().plugins().save();
}

void PluginsSettingsTab::closing()
{
  storeSettings(ui->pluginsList->currentItem());
}

void PluginsSettingsTab::on_checkboxEnabled_clicked(bool checked)
{
  // Retrieve the plugin:
  auto* item = ui->pluginsList->currentItem();
  if (!item || !item->data(0, PluginRole).isValid()) {
    return;
  }
  IPlugin* plugin          = this->plugin(item);
  const auto& requirements = m_pluginContainer->requirements(plugin);

  // User wants to enable:
  if (checked) {
    m_pluginContainer->setEnabled(plugin, true, false);
  } else {
    // Custom check for proxy + current game:
    if (m_pluginContainer->implementInterface<IPluginProxy>(plugin)) {

      // Current game:
      auto* game = m_pluginContainer->managedGame();
      if (m_pluginContainer->requirements(game).proxy() == plugin) {
        QMessageBox::warning(parentWidget(), QObject::tr("Cannot disable plugin"),
                             QObject::tr("The '%1' plugin is used by the current game "
                                         "plugin and cannot disabled.")
                                 .arg(plugin->localizedName()),
                             QMessageBox::Ok);
        ui->enabledCheckbox->setChecked(true);
        return;
      }

      // Check the proxied plugins:
      auto proxied = requirements.proxied();
      if (!proxied.empty()) {
        DisableProxyPluginDialog dialog(plugin, proxied, parentWidget());
        if (dialog.exec() != QDialog::Accepted) {
          ui->enabledCheckbox->setChecked(true);
          return;
        }
      }
    }

    // Check if the plugins is required for other plugins:
    auto requiredFor = requirements.requiredFor();
    if (!requiredFor.empty()) {
      QStringList pluginNames;
      for (auto& p : requiredFor) {
        pluginNames.append(p->localizedName());
      }
      pluginNames.sort();
      QString message =
          QObject::tr("<p>Disabling the '%1' plugin will also disable the following "
                      "plugins:</p><ul>%1</ul><p>Do you want to continue?</p>")
              .arg(plugin->localizedName())
              .arg("<li>" + pluginNames.join("</li><li>") + "</li>");
      if (QMessageBox::warning(parentWidget(), QObject::tr("Really disable plugin?"),
                               message,
                               QMessageBox::Yes | QMessageBox::No) == QMessageBox::No) {
        ui->enabledCheckbox->setChecked(true);
        return;
      }
    }
    m_pluginContainer->setEnabled(plugin, false, true);
  }

  // Proxy was disabled / enabled, need restart:
  if (m_pluginContainer->implementInterface<IPluginProxy>(plugin)) {
    dialog().setExitNeeded(Exit::Restart);
  }

  updateListItems();
}

void PluginsSettingsTab::on_pluginsList_currentItemChanged(QTreeWidgetItem* current,
                                                           QTreeWidgetItem* previous)
{
  storeSettings(previous);

  if (!current->data(0, PluginRole).isValid()) {
    return;
  }

  ui->pluginSettingsList->clear();
  IPlugin* plugin = this->plugin(current);
  ui->authorLabel->setText(plugin->author());
  ui->versionLabel->setText(plugin->version().canonicalString());
  ui->descriptionLabel->setText(plugin->description());

  // Checkbox, do not show for children or game plugins, disable
  // if the plugin cannot be enabled.
  ui->enabledCheckbox->setVisible(
      !m_pluginContainer->implementInterface<IPluginGame>(plugin) &&
      plugin->master().isEmpty());

  bool enabled       = m_pluginContainer->isEnabled(plugin);
  auto& requirements = m_pluginContainer->requirements(plugin);
  auto problems      = requirements.problems();

  if (m_pluginContainer->requirements(plugin).isCorePlugin()) {
    ui->enabledCheckbox->setDisabled(true);
    ui->enabledCheckbox->setToolTip(
        QObject::tr("This plugin is required for Mod Organizer to work properly and "
                    "cannot be disabled."));
  }
  // Plugin is enable or can be enabled.
  else if (enabled || problems.empty()) {
    ui->enabledCheckbox->setDisabled(false);
    ui->enabledCheckbox->setToolTip("");
    ui->enabledCheckbox->setChecked(enabled);
  }
  // Plugin is disable and cannot be enabled.
  else {
    ui->enabledCheckbox->setDisabled(true);
    ui->enabledCheckbox->setChecked(false);
    if (problems.size() == 1) {
      ui->enabledCheckbox->setToolTip(problems[0].shortDescription());
    } else {
      QStringList descriptions;
      for (auto& problem : problems) {
        descriptions.append(problem.shortDescription());
      }
      ui->enabledCheckbox->setToolTip("<ul><li>" + descriptions.join("</li><li>") +
                                      "</li></ul>");
    }
  }

  QVariantMap settings     = current->data(0, SettingsRole).toMap();
  QVariantMap descriptions = current->data(0, DescriptionsRole).toMap();
  ui->pluginSettingsList->setEnabled(settings.count() != 0);
  for (auto iter = settings.begin(); iter != settings.end(); ++iter) {
    QTreeWidgetItem* newItem = new QTreeWidgetItem(QStringList(iter.key()));
    QVariant value           = *iter;
    QString description;
    {
      auto descriptionIter = descriptions.find(iter.key());
      if (descriptionIter != descriptions.end()) {
        description = descriptionIter->toString();
      }
    }

    ui->pluginSettingsList->setItemDelegateForColumn(0, new NoEditDelegate());
    newItem->setData(1, Qt::DisplayRole, value);
    newItem->setData(1, Qt::EditRole, value);
    newItem->setToolTip(1, description);

    newItem->setFlags(newItem->flags() | Qt::ItemIsEditable);
    ui->pluginSettingsList->addTopLevelItem(newItem);
  }

  ui->pluginSettingsList->resizeColumnToContents(0);
  ui->pluginSettingsList->resizeColumnToContents(1);
}

void PluginsSettingsTab::deleteBlacklistItem()
{
  ui->pluginBlacklist->takeItem(ui->pluginBlacklist->currentIndex().row());
}

void PluginsSettingsTab::storeSettings(QTreeWidgetItem* pluginItem)
{
  if (pluginItem != nullptr && pluginItem->data(0, PluginRole).isValid()) {
    QVariantMap settings = pluginItem->data(0, SettingsRole).toMap();

    for (int i = 0; i < ui->pluginSettingsList->topLevelItemCount(); ++i) {
      const QTreeWidgetItem* item = ui->pluginSettingsList->topLevelItem(i);
      settings[item->text(0)]     = item->data(1, Qt::DisplayRole);
    }

    pluginItem->setData(0, SettingsRole, settings);
  }
}
