#include "settingsdialogplugins.h"
#include "ui_settingsdialog.h"
#include "noeditdelegate.h"
#include <iplugin.h>

using MOBase::IPlugin;

PluginsSettingsTab::PluginsSettingsTab(Settings *m_parent, SettingsDialog &m_dialog)
  : SettingsTab(m_parent, m_dialog)
{
  // display plugin settings
  QSet<QString> handledNames;
  for (IPlugin *plugin : m_parent->plugins()) {
    if (handledNames.contains(plugin->name()))
      continue;
    QListWidgetItem *listItem = new QListWidgetItem(plugin->name(), ui->pluginsList);
    listItem->setData(Qt::UserRole, QVariant::fromValue((void*)plugin));
    listItem->setData(Qt::UserRole + 1, m_parent->m_PluginSettings[plugin->name()]);
    listItem->setData(Qt::UserRole + 2, m_parent->m_PluginDescriptions[plugin->name()]);
    ui->pluginsList->addItem(listItem);
    handledNames.insert(plugin->name());
  }

  // display plugin blacklist
  for (const QString &pluginName : m_parent->m_PluginBlacklist) {
    ui->pluginBlacklist->addItem(pluginName);
  }

  QObject::connect(
    ui->pluginsList, &QListWidget::currentItemChanged,
    [&](auto* current, auto* previous) { on_pluginsList_currentItemChanged(current, previous); });

  QShortcut *delShortcut = new QShortcut(
    QKeySequence(Qt::Key_Delete), ui->pluginBlacklist);
  QObject::connect(delShortcut, &QShortcut::activated, parentWidget(), [&]{ deleteBlacklistItem(); });
}

void PluginsSettingsTab::update()
{
  // transfer plugin settings to in-memory structure
  for (int i = 0; i < ui->pluginsList->count(); ++i) {
    QListWidgetItem *item = ui->pluginsList->item(i);
    m_parent->m_PluginSettings[item->text()] = item->data(Qt::UserRole + 1).toMap();
  }
  // store plugin settings on disc
  for (auto iterPlugins = m_parent->m_PluginSettings.begin(); iterPlugins != m_parent->m_PluginSettings.end(); ++iterPlugins) {
    for (auto iterSettings = iterPlugins->begin(); iterSettings != iterPlugins->end(); ++iterSettings) {
      m_Settings.setValue("Plugins/" + iterPlugins.key() + "/" + iterSettings.key(), iterSettings.value());
    }
  }

  // store plugin blacklist
  m_parent->m_PluginBlacklist.clear();
  for (QListWidgetItem *item : ui->pluginBlacklist->findItems("*", Qt::MatchWildcard)) {
    m_parent->m_PluginBlacklist.insert(item->text());
  }
  m_parent->writePluginBlacklist();
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

  QVariantMap settings = current->data(Qt::UserRole + 1).toMap();
  QVariantMap descriptions = current->data(Qt::UserRole + 2).toMap();
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
    QVariantMap settings = pluginItem->data(Qt::UserRole + 1).toMap();

    for (int i = 0; i < ui->pluginSettingsList->topLevelItemCount(); ++i) {
      const QTreeWidgetItem *item = ui->pluginSettingsList->topLevelItem(i);
      settings[item->text(0)] = item->data(1, Qt::DisplayRole);
    }

    pluginItem->setData(Qt::UserRole + 1, settings);
  }
}
