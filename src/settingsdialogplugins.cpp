#include "settingsdialogplugins.h"
#include "ui_settingsdialog.h"
#include "noeditdelegate.h"
#include <iplugin.h>

using MOBase::IPlugin;

PluginsSettingsTab::PluginsSettingsTab(Settings& s, SettingsDialog& d)
  : SettingsTab(s, d)
{
  ui->pluginSettingsList->setStyleSheet("QTreeWidget::item {padding-right: 10px;}");

  // display plugin settings
  QSet<QString> handledNames;
  for (IPlugin *plugin : settings().plugins().plugins()) {
    if (handledNames.contains(plugin->name()))
      continue;
    QListWidgetItem *listItem = new QListWidgetItem(plugin->localizedName(), ui->pluginsList);
    listItem->setData(ROLE_PLUGIN, QVariant::fromValue((void*)plugin));
    listItem->setData(ROLE_SETTINGS, settings().plugins().settings(plugin->name()));
    listItem->setData(ROLE_DESCRIPTIONS, settings().plugins().descriptions(plugin->name()));
    ui->pluginsList->addItem(listItem);
    handledNames.insert(plugin->name());
  }

  // display plugin blacklist
  for (const QString &pluginName : settings().plugins().blacklist()) {
    ui->pluginBlacklist->addItem(pluginName);
  }

  QObject::connect(
    ui->pluginsList, &QListWidget::currentItemChanged,
    [&](auto* current, auto* previous) { on_pluginsList_currentItemChanged(current, previous); });

  QShortcut *delShortcut = new QShortcut(
    QKeySequence(Qt::Key_Delete), ui->pluginBlacklist);
  QObject::connect(delShortcut, &QShortcut::activated, &dialog(), [&]{ deleteBlacklistItem(); });
}

IPlugin* PluginsSettingsTab::plugin(QListWidgetItem* pluginItem) const
{
  return static_cast<IPlugin*>(qvariant_cast<void*>(pluginItem->data(ROLE_PLUGIN)));
}

void PluginsSettingsTab::update()
{
  // transfer plugin settings to in-memory structure
  for (int i = 0; i < ui->pluginsList->count(); ++i) {
    QListWidgetItem *item = ui->pluginsList->item(i);
    settings().plugins().setSettings(
      plugin(item)->name(), item->data(ROLE_SETTINGS).toMap());
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

void PluginsSettingsTab::on_pluginsList_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
  storeSettings(previous);

  ui->pluginSettingsList->clear();
  IPlugin *plugin = static_cast<IPlugin*>(current->data(Qt::UserRole).value<void*>());
  ui->authorLabel->setText(plugin->author());
  ui->versionLabel->setText(plugin->version().canonicalString());
  ui->descriptionLabel->setText(plugin->description());

  QVariantMap settings = current->data(ROLE_SETTINGS).toMap();
  QVariantMap descriptions = current->data(ROLE_DESCRIPTIONS).toMap();
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

void PluginsSettingsTab::storeSettings(QListWidgetItem *pluginItem)
{
  if (pluginItem != nullptr) {
    QVariantMap settings = pluginItem->data(ROLE_SETTINGS).toMap();

    for (int i = 0; i < ui->pluginSettingsList->topLevelItemCount(); ++i) {
      const QTreeWidgetItem *item = ui->pluginSettingsList->topLevelItem(i);
      settings[item->text(0)] = item->data(1, Qt::DisplayRole);
    }

    pluginItem->setData(ROLE_SETTINGS, settings);
  }
}
