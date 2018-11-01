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

#include <QDirIterator>
#include <QFileDialog>
#include <QMessageBox>
#include <QShortcut>
#include <QColorDialog>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>


using namespace MOBase;


SettingsDialog::SettingsDialog(PluginContainer *pluginContainer, QWidget *parent)
  : TutorableDialog("SettingsDialog", parent)
  , ui(new Ui::SettingsDialog)
  , m_PluginContainer(pluginContainer)
{
  ui->setupUi(this);

  QShortcut *delShortcut
      = new QShortcut(QKeySequence(Qt::Key_Delete), ui->pluginBlacklist);
  connect(delShortcut, SIGNAL(activated()), this, SLOT(deleteBlacklistItem()));
}

SettingsDialog::~SettingsDialog()
{
  delete ui;
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
  IPluginGame const *game
      = qApp->property("managed_game").value<IPluginGame *>();
  QDir dir = game->dataDirectory();

  Helper::backdateBSAs(qApp->applicationDirPath().toStdWString(),
                       dir.absolutePath().toStdWString());
}

void SettingsDialog::on_browseBaseDirBtn_clicked()
{
  QString temp = QFileDialog::getExistingDirectory(
      this, tr("Select base directory"), ui->baseDirEdit->text());
  if (!temp.isEmpty()) {
    ui->baseDirEdit->setText(temp);
  }
}

void SettingsDialog::on_browseDownloadDirBtn_clicked()
{
  QString searchPath = ui->downloadDirEdit->text();
  searchPath.replace("%BASE_DIR%", ui->baseDirEdit->text());

  QString temp = QFileDialog::getExistingDirectory(this, tr("Select download directory"), searchPath);
  if (!temp.isEmpty()) {
    ui->downloadDirEdit->setText(temp);
  }
}

void SettingsDialog::on_browseModDirBtn_clicked()
{
  QString searchPath = ui->modDirEdit->text();
  searchPath.replace("%BASE_DIR%", ui->baseDirEdit->text());

  QString temp = QFileDialog::getExistingDirectory(this, tr("Select mod directory"), searchPath);
  if (!temp.isEmpty()) {
    ui->modDirEdit->setText(temp);
  }
}

void SettingsDialog::on_browseCacheDirBtn_clicked()
{
  QString searchPath = ui->cacheDirEdit->text();
  searchPath.replace("%BASE_DIR%", ui->baseDirEdit->text());

  QString temp = QFileDialog::getExistingDirectory(this, tr("Select cache directory"), searchPath);
  if (!temp.isEmpty()) {
    ui->cacheDirEdit->setText(temp);
  }
}

void SettingsDialog::on_browseProfilesDirBtn_clicked()
{
  QString searchPath = ui->profilesDirEdit->text();
  searchPath.replace("%BASE_DIR%", ui->baseDirEdit->text());

  QString temp = QFileDialog::getExistingDirectory(this, tr("Select profiles directory"), searchPath);
  if (!temp.isEmpty()) {
    ui->profilesDirEdit->setText(temp);
  }
}

void SettingsDialog::on_browseOverwriteDirBtn_clicked()
{
  QString searchPath = ui->overwriteDirEdit->text();
  searchPath.replace("%BASE_DIR%", ui->baseDirEdit->text());

  QString temp = QFileDialog::getExistingDirectory(this, tr("Select overwrite directory"), searchPath);
  if (!temp.isEmpty()) {
    ui->overwriteDirEdit->setText(temp);
  }
}

void SettingsDialog::on_containsBtn_clicked()
{
  QColor result = QColorDialog::getColor(m_ContainsColor, this);
  if (result.isValid()) {
    m_ContainsColor = result;

    QString COLOR_STYLE("QPushButton { background-color : %1; color : %2; }");
    ui->containsBtn->setStyleSheet(COLOR_STYLE.arg(
     result.name()).arg(Settings::getIdealTextColor(
        result).name()));

    /*ui->containsBtn->setAutoFillBackground(true);
    ui->containsBtn->setPalette(QPalette(result));
    QPalette palette = ui->containsBtn->palette();
    palette.setColor(QPalette::Background, result);
    ui->containsBtn->setPalette(palette);*/
  }
}

void SettingsDialog::on_containedBtn_clicked()
{
  QColor result = QColorDialog::getColor(m_ContainedColor, this);
  if (result.isValid()) {
    m_ContainedColor = result;

    QString COLOR_STYLE("QPushButton { background-color : %1; color : %2; }");
    ui->containedBtn->setStyleSheet(COLOR_STYLE.arg(
      result.name()).arg(Settings::getIdealTextColor(
        result).name()));

    /*ui->containedBtn->setAutoFillBackground(true);
    ui->containedBtn->setPalette(QPalette(result));
    QPalette palette = ui->containedBtn->palette();
    palette.setColor(QPalette::Background, result);
    ui->containedBtn->setPalette(palette);*/
  }
}

void SettingsDialog::on_overwrittenBtn_clicked()
{
  QColor result = QColorDialog::getColor(m_OverwrittenColor, this);
  if (result.isValid()) {
    m_OverwrittenColor = result;

    QString COLOR_STYLE("QPushButton { background-color : %1; color : %2; }");
    ui->overwrittenBtn->setStyleSheet(COLOR_STYLE.arg(
      result.name()).arg(Settings::getIdealTextColor(
        result).name()));

    /*ui->overwrittenBtn->setAutoFillBackground(true);
    ui->overwrittenBtn->setPalette(QPalette(result));
    QPalette palette = ui->overwrittenBtn->palette();
    palette.setColor(QPalette::Background, result);
    ui->overwrittenBtn->setPalette(palette);*/
  }
}

void SettingsDialog::on_overwritingBtn_clicked()
{
  QColor result = QColorDialog::getColor(m_OverwritingColor, this);
  if (result.isValid()) {
    m_OverwritingColor = result;

    QString COLOR_STYLE("QPushButton { background-color : %1; color : %2; }");
    ui->overwritingBtn->setStyleSheet(COLOR_STYLE.arg(
      result.name()).arg(Settings::getIdealTextColor(
        result).name()));

    /*ui->overwritingBtn->setAutoFillBackground(true);
    ui->overwritingBtn->setPalette(QPalette(result));
    QPalette palette = ui->overwritingBtn->palette();
    palette.setColor(QPalette::Background, result);
    ui->overwritingBtn->setPalette(palette);*/
  }
}

void SettingsDialog::on_resetColorsBtn_clicked()
{
  m_OverwritingColor = QColor(255, 0, 0, 64);
  m_OverwrittenColor = QColor(0, 255, 0, 64);
  m_ContainsColor = QColor(0, 0, 255, 64);
  m_ContainedColor = QColor(0, 0, 255, 64);

  QString COLOR_STYLE("QPushButton { background-color : %1; color : %2; }");

  ui->overwritingBtn->setStyleSheet(COLOR_STYLE.arg(
    QColor(255, 0, 0, 64).name()).arg(Settings::getIdealTextColor(
      QColor(255, 0, 0, 64)).name()));

  ui->overwrittenBtn->setStyleSheet(COLOR_STYLE.arg(
    QColor(0, 255, 0, 64).name()).arg(Settings::getIdealTextColor(
      QColor(0, 255, 0, 64)).name()));

  ui->containsBtn->setStyleSheet(COLOR_STYLE.arg(
    QColor(0, 0, 255, 64).name()).arg(Settings::getIdealTextColor(
      QColor(0, 0, 255, 64)).name()));

  ui->containedBtn->setStyleSheet(COLOR_STYLE.arg(
    QColor(0, 0, 255, 64).name()).arg(Settings::getIdealTextColor(
      QColor(0, 0, 255, 64)).name()));

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

void SettingsDialog::on_clearCacheButton_clicked()
{
  QDir(Settings::instance().getCacheDirectory()).removeRecursively();
  NexusInterface::instance(m_PluginContainer)->clearCache();
}

