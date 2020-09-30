#include "settingsdialogplugins.h"
#include "ui_settingsdialog.h"
#include "noeditdelegate.h"
#include <iplugin.h>

#include "plugincontainer.h"

using MOBase::IPlugin;

PluginsSettingsTab::PluginsSettingsTab(Settings& s, PluginContainer* pluginContainer, SettingsDialog& d)
  : SettingsTab(s, d)
{
  ui->pluginSettingsList->setStyleSheet("QTreeWidget::item {padding-right: 10px;}");

  // Create top-level tree widget:
  QStringList pluginInterfaces = pluginContainer->pluginInterfaces();
  pluginInterfaces.sort(Qt::CaseInsensitive);
  std::map<QString, QTreeWidgetItem*> topItems;
  for (QString interfaceName : pluginInterfaces) {
    auto *item = new QTreeWidgetItem(ui->pluginsList, { interfaceName });
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    auto font = item->font(0);
    font.setBold(true);
    item->setFont(0, font);
    topItems[interfaceName] = item;
    item->setExpanded(true);
  }
  ui->pluginsList->setHeaderHidden(true);

  // display plugin settings
  QSet<QString> handledNames;
  for (IPlugin *plugin : settings().plugins().plugins()) {
    if (handledNames.contains(plugin->name()))
      continue;
    QTreeWidgetItem* listItem = new QTreeWidgetItem(
      topItems.at(pluginContainer->topImplementedInterface(plugin)));
    listItem->setData(0, Qt::DisplayRole, plugin->localizedName());
    listItem->setData(0, ROLE_PLUGIN, QVariant::fromValue((void*)plugin));
    listItem->setData(0, ROLE_SETTINGS, settings().plugins().settings(plugin->name()));
    listItem->setData(0, ROLE_DESCRIPTIONS, settings().plugins().descriptions(plugin->name()));

    if (handledNames.isEmpty()) {
      listItem->setSelected(true);
    }

    handledNames.insert(plugin->name());
  }

  ui->pluginsList->sortByColumn(0, Qt::AscendingOrder);

  // display plugin blacklist
  for (const QString &pluginName : settings().plugins().blacklist()) {
    ui->pluginBlacklist->addItem(pluginName);
  }

  m_filter.setEdit(ui->pluginFilterEdit);
  m_filter.setList(ui->pluginsList);

  QObject::connect(
    ui->pluginsList, &QTreeWidget::currentItemChanged,
    [&](auto* current, auto* previous) { on_pluginsList_currentItemChanged(current, previous); });

  QShortcut *delShortcut = new QShortcut(
    QKeySequence(Qt::Key_Delete), ui->pluginBlacklist);
  QObject::connect(delShortcut, &QShortcut::activated, &dialog(), [&]{ deleteBlacklistItem(); });
}

IPlugin* PluginsSettingsTab::plugin(QTreeWidgetItem* pluginItem) const
{
  return static_cast<IPlugin*>(qvariant_cast<void*>(pluginItem->data(0, ROLE_PLUGIN)));
}

void PluginsSettingsTab::update()
{
  // transfer plugin settings to in-memory structure
  for (int i = 0; i < ui->pluginsList->topLevelItemCount(); ++i) {
    auto *topLevelItem = ui->pluginsList->topLevelItem(i);
    for (int j = 0; j < topLevelItem->childCount(); ++j) {
      auto* item = topLevelItem->child(j);
      settings().plugins().setSettings(
        plugin(item)->name(), item->data(0, ROLE_SETTINGS).toMap());
    }
  }

  // set plugin blacklist
  QStringList names;
  for (QListWidgetItem *item : ui->pluginBlacklist->findItems("*", Qt::MatchWildcard)) {
    names.push_back(item->text());
  }

  settings().plugins().setBlacklist(names);

  settings().plugins().save();
}

void PluginsSettingsTab::closing()
{
  storeSettings(ui->pluginsList->currentItem());
}

void PluginsSettingsTab::on_pluginsList_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
  storeSettings(previous);

  if (!current->data(0, ROLE_PLUGIN).isValid()) {
    return;
  }

  ui->pluginSettingsList->clear();
  IPlugin* plugin = this->plugin(current);
  ui->authorLabel->setText(plugin->author());
  ui->versionLabel->setText(plugin->version().canonicalString());
  ui->descriptionLabel->setText(plugin->description());

  QVariantMap settings = current->data(0, ROLE_SETTINGS).toMap();
  QVariantMap descriptions = current->data(0, ROLE_DESCRIPTIONS).toMap();
  ui->pluginSettingsList->setEnabled(settings.count() != 0);
  for (auto iter = settings.begin(); iter != settings.end(); ++iter) {
    QTreeWidgetItem *newItem = new QTreeWidgetItem(QStringList(iter.key()));
    QVariant value = *iter;
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

void PluginsSettingsTab::storeSettings(QTreeWidgetItem *pluginItem)
{
  if (pluginItem != nullptr && pluginItem->data(0, ROLE_PLUGIN).isValid()) {
    QVariantMap settings = pluginItem->data(0, ROLE_SETTINGS).toMap();

    for (int i = 0; i < ui->pluginSettingsList->topLevelItemCount(); ++i) {
      const QTreeWidgetItem *item = ui->pluginSettingsList->topLevelItem(i);
      settings[item->text(0)] = item->data(1, Qt::DisplayRole);
    }

    pluginItem->setData(0, ROLE_SETTINGS, settings);
  }
}
