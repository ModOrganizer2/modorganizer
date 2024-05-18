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

#include "transfersavesdialog.h"

#include "iplugingame.h"
#include "isavegame.h"
#include "savegameinfo.h"
#include "ui_transfersavesdialog.h"
#include <log.h>
#include <utility.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFlags>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <QtDebug>

using namespace MOBase;
using namespace MOShared;

TransferSavesDialog::TransferSavesDialog(const Profile& profile,
                                         IPluginGame const* gamePlugin, QWidget* parent)
    : TutorableDialog("TransferSaves", parent), ui(new Ui::TransferSavesDialog),
      m_Profile(profile), m_GamePlugin(gamePlugin)
{
  ui->setupUi(this);
  ui->label_2->setText(tr("Characters for profile %1").arg(m_Profile.name()));
  refreshGlobalSaves();
  refreshLocalSaves();
  refreshGlobalCharacters();
  refreshLocalCharacters();
}

TransferSavesDialog::~TransferSavesDialog()
{
  delete ui;
}

void TransferSavesDialog::refreshGlobalSaves()
{
  refreshSaves(m_GlobalSaves, m_GamePlugin->savesDirectory().absolutePath());
}

void TransferSavesDialog::refreshLocalSaves()
{
  refreshSaves(m_LocalSaves, m_Profile.savePath());
}

void TransferSavesDialog::refreshGlobalCharacters()
{
  refreshCharacters(m_GlobalSaves, ui->globalCharacterList, ui->copyToLocalBtn,
                    ui->moveToLocalBtn);
}

void TransferSavesDialog::refreshLocalCharacters()
{
  refreshCharacters(m_LocalSaves, ui->localCharacterList, ui->copyToGlobalBtn,
                    ui->moveToGlobalBtn);
}

bool TransferSavesDialog::testOverwrite(OverwriteMode& overwriteMode,
                                        const QString& destinationFile)
{
  QMessageBox::StandardButton res =
      overwriteMode == OVERWRITE_YES ? QMessageBox::Yes : QMessageBox::No;
  if (overwriteMode == OVERWRITE_ASK) {
    res = QMessageBox::question(this, tr("Overwrite"),
                                tr("Overwrite the file \"%1\"").arg(destinationFile),
                                QMessageBox::Yes | QMessageBox::No |
                                    QMessageBox::YesToAll | QMessageBox::NoToAll);
    if (res == QMessageBox::YesToAll) {
      overwriteMode = OVERWRITE_YES;
      res           = QMessageBox::Yes;
    } else if (res == QMessageBox::NoToAll) {
      overwriteMode = OVERWRITE_NO;
      res           = QMessageBox::No;
    }
  }
  return res == QMessageBox::Yes;
}

#define MOVE_SAVES "Move all save games of character \"%1\""
#define COPY_SAVES "Copy all save games of character \"%1\""

#define TO_PROFILE "to the profile?"
#define TO_GLOBAL                                                                      \
  "to the global location? Please be aware that this will mess up the running number " \
  "of save games."

void TransferSavesDialog::on_moveToLocalBtn_clicked()
{
  QString character = ui->globalCharacterList->currentItem()->text();
  if (transferCharacters(
          character, MOVE_SAVES TO_PROFILE, m_GamePlugin->savesDirectory(),
          m_GlobalSaves[character], m_Profile.savePath(),
          [this](const QString& source, const QString& destination) -> bool {
            return shellMove(source, destination, this);
          },
          "Failed to move {} to {}")) {
    refreshGlobalSaves();
    refreshGlobalCharacters();
    refreshLocalSaves();
    refreshLocalCharacters();
  }
}

void TransferSavesDialog::on_copyToLocalBtn_clicked()
{
  QString character = ui->globalCharacterList->currentItem()->text();
  if (transferCharacters(
          character, COPY_SAVES TO_PROFILE, m_GamePlugin->savesDirectory(),
          m_GlobalSaves[character], m_Profile.savePath(),
          [this](const QString& source, const QString& destination) -> bool {
            return shellCopy(source, destination, this);
          },
          "Failed to copy {} to {}")) {
    refreshLocalSaves();
    refreshLocalCharacters();
  }
}

void TransferSavesDialog::on_moveToGlobalBtn_clicked()
{
  QString character = ui->localCharacterList->currentItem()->text();
  if (transferCharacters(
          character, MOVE_SAVES TO_GLOBAL, m_Profile.savePath(),
          m_LocalSaves[character], m_GamePlugin->savesDirectory().absolutePath(),
          [this](const QString& source, const QString& destination) -> bool {
            return shellMove(source, destination, this);
          },
          "Failed to move {} to {}")) {
    refreshGlobalSaves();
    refreshGlobalCharacters();
    refreshLocalSaves();
    refreshLocalCharacters();
  }
}

void TransferSavesDialog::on_copyToGlobalBtn_clicked()
{
  QString character = ui->localCharacterList->currentItem()->text();
  if (transferCharacters(
          character, COPY_SAVES TO_GLOBAL, m_Profile.savePath(),
          m_LocalSaves[character], m_GamePlugin->savesDirectory().absolutePath(),
          [this](const QString& source, const QString& destination) -> bool {
            return shellCopy(source, destination, this);
          },
          "Failed to copy {} to {}")) {
    refreshGlobalSaves();
    refreshGlobalCharacters();
  }
}

void TransferSavesDialog::on_doneButton_clicked()
{
  close();
}

void TransferSavesDialog::on_globalCharacterList_currentTextChanged(
    const QString& currentText)
{
  ui->globalSavesList->clear();
  // sadly this can get called while we're resetting the list, with an invalid
  // name, so we have to check.
  SaveCollection::const_iterator saveList = m_GlobalSaves.find(currentText);
  if (saveList != m_GlobalSaves.end()) {
    for (SaveListItem const& save : saveList->second) {
      ui->globalSavesList->addItem(QFileInfo(save->getFilepath()).fileName());
    }
  }
}

void TransferSavesDialog::on_localCharacterList_currentTextChanged(
    const QString& currentText)
{
  ui->localSavesList->clear();
  // sadly this can get called while we're resetting the list, with an invalid
  // name, so we have to check.
  SaveCollection::const_iterator saveList = m_LocalSaves.find(currentText);
  if (saveList != m_LocalSaves.end()) {
    for (SaveListItem const& save : saveList->second) {
      ui->localSavesList->addItem(QFileInfo(save->getFilepath()).fileName());
    }
  }
}

void TransferSavesDialog::refreshSaves(SaveCollection& saveCollection,
                                       QString const& savedir)
{
  saveCollection.clear();

  auto saves = m_GamePlugin->listSaves(savedir);
  std::sort(saves.begin(), saves.end(), [](auto const& lhs, auto const& rhs) {
    return lhs->getCreationTime() > rhs->getCreationTime();
  });

  for (auto& save : saves) {
    saveCollection[save->getSaveGroupIdentifier()].push_back(save);
  }
}

void TransferSavesDialog::refreshCharacters(const SaveCollection& saveCollection,
                                            QListWidget* charList, QPushButton* copy,
                                            QPushButton* move)
{
  charList->clear();
  for (SaveCollection::value_type const& val : saveCollection) {
    charList->addItem(val.first);
  }
  if (charList->count() > 0) {
    charList->setCurrentRow(0);
    copy->setEnabled(true);
    move->setEnabled(true);
  } else {
    copy->setEnabled(false);
    move->setEnabled(false);
  }
}

bool TransferSavesDialog::transferCharacters(
    QString const& character, char const* message, QDir const& sourceDirectory,
    SaveList& saves, QDir const& destination,
    const std::function<bool(const QString&, const QString&)>& method,
    std::format_string<QString, QString> errmsg)
{
  if (QMessageBox::question(this, tr("Confirm"), tr(message).arg(character),
                            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
    return false;
  }

  OverwriteMode overwriteMode = OVERWRITE_ASK;

  for (SaveListItem const& save : saves) {
    for (QString source : save->allFiles()) {
      QFileInfo sourceFile(source);
      QString destinationFile(
          destination.absoluteFilePath(sourceDirectory.relativeFilePath(source)));

      // If the file is already there, let them skip (or not).
      if (QFile::exists(destinationFile)) {
        if (!testOverwrite(overwriteMode, destinationFile)) {
          continue;
        }
        // OK, they want to remove it.
        QFile::remove(destinationFile);
      }

      if (!method(sourceFile.absoluteFilePath(), destinationFile)) {
        log::error(errmsg, sourceFile.absoluteFilePath(), std::move(destinationFile));
      }
    }
  }
  return true;
}
