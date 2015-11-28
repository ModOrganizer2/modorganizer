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

#include <QDirIterator>
#include <QFileDialog>
#include <QMessageBox>
#include <QShortcut>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>


using namespace MOBase;


SettingsDialog::SettingsDialog(QWidget *parent)
  : TutorableDialog("SettingsDialog", parent), ui(new Ui::SettingsDialog)
{
  ui->setupUi(this);

  QShortcut *delShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), ui->pluginBlacklist);
  connect(delShortcut, SIGNAL(activated()), this, SLOT(deleteBlacklistItem()));
}

SettingsDialog::~SettingsDialog()
{
  delete ui;
}

void SettingsDialog::addPlugins(const std::vector<IPlugin*> &plugins)
{
  foreach (IPlugin *plugin, plugins) {
    ui->pluginsList->addItem(plugin->name());
  }
}

void SettingsDialog::accept()
{
  storeSettings(ui->pluginsList->currentItem());
  TutorableDialog::accept();
}


void SettingsDialog::on_loginCheckBox_toggled(bool checked)
{
  QLineEdit *usernameEdit = findChild<QLineEdit*>("usernameEdit");
  QLineEdit *passwordEdit = findChild<QLineEdit*>("passwordEdit");
  if (checked) {
    passwordEdit->setEnabled(true);
    usernameEdit->setEnabled(true);
  } else {
    passwordEdit->setEnabled(false);
    usernameEdit->setEnabled(false);
  }
}

void SettingsDialog::on_categoriesBtn_clicked()
{
  CategoriesDialog dialog(this);
  if (dialog.exec() == QDialog::Accepted) {
    dialog.commitChanges();
  }
}

void SettingsDialog::on_bsaDateBtn_clicked()
{
  IPluginGame const *game = qApp->property("managed_game").value<IPluginGame const *>();
  QDir dir = game->dataDirectory();

  Helper::backdateBSAs(qApp->property("dataPath").toString().toStdWString(),
                       dir.absolutePath().toStdWString());
}

void SettingsDialog::on_browseDownloadDirBtn_clicked()
{
  QString temp = QFileDialog::getExistingDirectory(this, tr("Select download directory"), ui->downloadDirEdit->text());
  if (!temp.isEmpty()) {
    ui->downloadDirEdit->setText(temp);
  }
}

void SettingsDialog::on_browseModDirBtn_clicked()
{
  QString temp = QFileDialog::getExistingDirectory(this, tr("Select mod directory"), ui->downloadDirEdit->text());
  if (!temp.isEmpty()) {
    ui->modDirEdit->setText(temp);
  }
}

void SettingsDialog::on_browseCacheDirBtn_clicked()
{
  QString temp = QFileDialog::getExistingDirectory(this, tr("Select cache directory"), ui->cacheDirEdit->text());
  if (!temp.isEmpty()) {
    ui->cacheDirEdit->setText(temp);
  }
}

void SettingsDialog::on_resetDialogsButton_clicked()
{
  if (QMessageBox::question(this, tr("Confirm?"),
          tr("This will make all dialogs show up again where you checked the \"Remember selection\"-box. Continue?"),
          QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    emit resetDialogs();
  }
}

void SettingsDialog::storeSettings(QListWidgetItem *pluginItem)
{
  if (pluginItem != nullptr) {
    QMap<QString, QVariant> settings = pluginItem->data(Qt::UserRole + 1).toMap();

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

  QMap<QString, QVariant> settings = current->data(Qt::UserRole + 1).toMap();
  QMap<QString, QVariant> descriptions = current->data(Qt::UserRole + 2).toMap();
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
}

void SettingsDialog::deleteBlacklistItem()
{
  ui->pluginBlacklist->takeItem(ui->pluginBlacklist->currentIndex().row());
}

void SettingsDialog::on_associateButton_clicked()
{
  Settings::instance().registerAsNXMHandler(true);
}
