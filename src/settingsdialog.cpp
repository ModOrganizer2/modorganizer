/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "settingsdialog.h"

#include "ui_settingsdialog.h"
#include "categoriesdialog.h"
#include "helper.h"
#include "noeditdelegate.h"
#include "iplugingame.h"
#include "settings.h"
#include "instancemanager.h"
#include "nexusinterface.h"
#include "plugincontainer.h"

#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <QDirIterator>
#include <QFileDialog>
#include <QMessageBox>
#include <QShortcut>
#include <QColorDialog>
#include <QInputDialog>
#include <QJsonDocument>
#include <QDesktopServices>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>


using namespace MOBase;


SettingsDialog::SettingsDialog(PluginContainer *pluginContainer, Settings* settings, QWidget *parent)
  : TutorableDialog("SettingsDialog", parent)
  , ui(new Ui::SettingsDialog)
  , m_settings(settings)
  , m_PluginContainer(pluginContainer)
  , m_GeometriesReset(false)
  , m_keyChanged(false)
{
  ui->setupUi(this);
  ui->pluginSettingsList->setStyleSheet("QTreeWidget::item {padding-right: 10px;}");

  QShortcut *delShortcut = new QShortcut(
    QKeySequence(Qt::Key_Delete), ui->pluginBlacklist);
  connect(delShortcut, SIGNAL(activated()), this, SLOT(deleteBlacklistItem()));
}

SettingsDialog::~SettingsDialog()
{
  disconnect(this);
  delete ui;
}

QString SettingsDialog::getColoredButtonStyleSheet() const
{
  return QString("QPushButton {"
    "background-color: %1;"
    "color: %2;"
    "border: 1px solid;"
    "padding: 3px;"
    "}");
}

void SettingsDialog::accept()
{
  QString newModPath = ui->modDirEdit->text();
  newModPath.replace("%BASE_DIR%", ui->baseDirEdit->text());

  if ((QDir::fromNativeSeparators(newModPath) !=
       QDir::fromNativeSeparators(
           Settings::instance().getModDirectory(true))) &&
      (QMessageBox::question(
           nullptr, tr("Confirm"),
           tr("Changing the mod directory affects all your profiles! "
              "Mods not present (or named differently) in the new location "
              "will be disabled in all profiles. "
              "There is no way to undo this unless you backed up your "
              "profiles manually. Proceed?"),
           QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)) {
    return;
  }

  storeSettings(ui->pluginsList->currentItem());
  TutorableDialog::accept();
}

bool SettingsDialog::getResetGeometries()
{
  return ui->resetGeometryBtn->isChecked();
}

bool SettingsDialog::getApiKeyChanged()
{
  return m_keyChanged;
}

void SettingsDialog::on_execBlacklistBtn_clicked()
{
  bool ok = false;
  QString result = QInputDialog::getMultiLineText(
    this,
    tr("Executables Blacklist"),
    tr("Enter one executable per line to be blacklisted from the virtual file system.\n"
       "Mods and other virtualized files will not be visible to these executables and\n"
       "any executables launched by them.\n\n"
       "Example:\n"
       "    Chrome.exe\n"
       "    Firefox.exe"),
    m_ExecutableBlacklist.split(";").join("\n"),
    &ok
    );
  if (ok) {
    QStringList blacklist;
    for (auto exec : result.split("\n")) {
      if (exec.trimmed().endsWith(".exe", Qt::CaseInsensitive)) {
        blacklist << exec.trimmed();
      }
    }
    m_ExecutableBlacklist = blacklist.join(";");
  }
}

void SettingsDialog::on_bsaDateBtn_clicked()
{
  IPluginGame const *game
      = qApp->property("managed_game").value<IPluginGame *>();
  QDir dir = game->dataDirectory();

  Helper::backdateBSAs(qApp->applicationDirPath().toStdWString(),
                       dir.absolutePath().toStdWString());
}

void SettingsDialog::storeSettings(QListWidgetItem *pluginItem)
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

void SettingsDialog::on_pluginsList_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
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

void SettingsDialog::deleteBlacklistItem()
{
  ui->pluginBlacklist->takeItem(ui->pluginBlacklist->currentIndex().row());
}

void SettingsDialog::on_resetGeometryBtn_clicked()
{
  m_GeometriesReset = true;
  ui->resetGeometryBtn->setChecked(true);
}
