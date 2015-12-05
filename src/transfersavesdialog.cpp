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

#include "ui_transfersavesdialog.h"
#include "iplugingame.h"
#include "savegamegamebyro.h"
#include "utility.h"

#include <QDir>
#include <QMessageBox>

#include <Shlwapi.h>
#include <shlobj.h>


using namespace MOBase;
using namespace MOShared;


TransferSavesDialog::TransferSavesDialog(const Profile &profile, IPluginGame const *gamePlugin, QWidget *parent)
  : TutorableDialog("TransferSaves", parent)
  , ui(new Ui::TransferSavesDialog)
  , m_Profile(profile)
  , m_GamePlugin(gamePlugin)
{
  ui->setupUi(this);
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
  m_GlobalSaves.clear();
  QDir savesDir(m_GamePlugin->savesDirectory());
  savesDir.setNameFilters(QStringList() << QString("*.%1").arg(m_GamePlugin->savegameExtension()));

  QStringList files = savesDir.entryList(QDir::Files, QDir::Time);

  for (const QString &filename : files) {
    SaveGameGamebryo *save = new SaveGameGamebryo(this, savesDir.absoluteFilePath(filename), m_GamePlugin);
    save->setParent(this);
    m_GlobalSaves.push_back(save);
  }
}


void TransferSavesDialog::refreshLocalSaves()
{
  m_LocalSaves.clear();

  QDir savesDir(m_Profile.absolutePath() + "/saves");

  savesDir.setNameFilters(QStringList() << QString("*.%1").arg(m_GamePlugin->savegameExtension()));

  QStringList files = savesDir.entryList(QDir::Files, QDir::Time);

  foreach (const QString &filename, files) {
    SaveGameGamebryo *save = new SaveGameGamebryo(this, savesDir.absoluteFilePath(filename), m_GamePlugin);
    save->setParent(this);
    m_LocalSaves.push_back(save);
  }
}


void TransferSavesDialog::refreshGlobalCharacters()
{
  std::set<QString> characters;
  for (std::vector<SaveGame*>::const_iterator iter = m_GlobalSaves.begin();
       iter != m_GlobalSaves.end(); ++iter) {
    characters.insert((*iter)->pcName());
  }
  ui->globalCharacterList->clear();
  for (std::set<QString>::const_iterator iter = characters.begin();
       iter != characters.end(); ++iter) {
    ui->globalCharacterList->addItem(*iter);
  }
  if (ui->globalCharacterList->count() > 0) {
    ui->globalCharacterList->setCurrentRow(0);
    ui->copyToLocalBtn->setEnabled(true);
    ui->moveToLocalBtn->setEnabled(true);
  } else {
    ui->copyToLocalBtn->setEnabled(false);
    ui->moveToLocalBtn->setEnabled(false);
  }
}


void TransferSavesDialog::refreshLocalCharacters()
{
  std::set<QString> characters;
  for (std::vector<SaveGame*>::const_iterator iter = m_LocalSaves.begin();
       iter != m_LocalSaves.end(); ++iter) {
    characters.insert((*iter)->pcName());
  }
  ui->localCharacterList->clear();
  for (std::set<QString>::const_iterator iter = characters.begin();
       iter != characters.end(); ++iter) {
    ui->localCharacterList->addItem(*iter);
  }
  if (ui->localCharacterList->count() > 0) {
    ui->localCharacterList->setCurrentRow(0);
    ui->copyToGlobalBtn->setEnabled(true);
    ui->moveToGlobalBtn->setEnabled(true);
  } else {
    ui->copyToGlobalBtn->setEnabled(false);
    ui->moveToGlobalBtn->setEnabled(false);
  }
}


bool TransferSavesDialog::testOverwrite(OverwriteMode &overwriteMode, const QString &destinationFile)
{
  QMessageBox::StandardButton res = overwriteMode == OVERWRITE_YES ? QMessageBox::Yes : QMessageBox::No;
  if (overwriteMode == OVERWRITE_ASK) {
    res = QMessageBox::question(this, tr("Overwrite"),
                                tr("Overwrite the file \"%1\"").arg(destinationFile),
                                QMessageBox::Yes | QMessageBox::No | QMessageBox::YesToAll | QMessageBox::NoToAll);
    if (res == QMessageBox::YesToAll) {
      overwriteMode = OVERWRITE_YES;
      res = QMessageBox::Yes;
    } else if (res == QMessageBox::NoToAll) {
      overwriteMode = OVERWRITE_NO;
      res = QMessageBox::No;
    }
  }
  return res == QMessageBox::Yes;
}

void TransferSavesDialog::on_moveToLocalBtn_clicked()
{
  QString selectedCharacter = ui->globalCharacterList->currentItem()->text();
  if (QMessageBox::question(this, tr("Confirm"),
      tr("Copy all save games of character \"%1\" to the profile?").arg(selectedCharacter),
      QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    QString destination = m_Profile.absolutePath() + "/saves";
    OverwriteMode overwriteMode = OVERWRITE_ASK;

    for (std::vector<SaveGame*>::const_iterator iter = m_GlobalSaves.begin();
         iter != m_GlobalSaves.end(); ++iter) {
      if ((*iter)->pcName() == selectedCharacter) {
        QStringList files = (*iter)->saveFiles();
        foreach (const QString &file, files) {
          QFileInfo fileInfo(file);
          QString destinationFile = destination + "/" + fileInfo.fileName();
          if (QFile::exists(destinationFile)) {
            if (testOverwrite(overwriteMode, destinationFile)) {
              QFile::remove(destinationFile);
            } else {
              continue;
            }
          }
          if (!QFile::rename(fileInfo.absoluteFilePath(), destinationFile)) {
            qCritical("failed to move %s to %s",
                      fileInfo.absoluteFilePath().toUtf8().constData(),
                      destinationFile.toUtf8().constData());
          }
        }
      }
    }
  }
  refreshGlobalSaves();
  refreshGlobalCharacters();
  refreshLocalSaves();
  refreshLocalCharacters();
}

void TransferSavesDialog::on_copyToLocalBtn_clicked()
{
  QString selectedCharacter = ui->globalCharacterList->currentItem()->text();
  if (QMessageBox::question(this, tr("Confirm"),
      tr("Copy all save games of character \"%1\" to the profile?").arg(selectedCharacter),
      QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    QString destination = m_Profile.absolutePath() + "/saves";
    OverwriteMode overwriteMode = OVERWRITE_ASK;
    for (std::vector<SaveGame*>::const_iterator iter = m_GlobalSaves.begin();
         iter != m_GlobalSaves.end(); ++iter) {
      if ((*iter)->pcName() == selectedCharacter) {
        QStringList files = (*iter)->saveFiles();
        foreach (const QString &file, files) {
          QFileInfo fileInfo(file);
          QString destinationFile = destination + "/" + fileInfo.fileName();
          if (QFile::exists(destinationFile)) {
            if (testOverwrite(overwriteMode, destinationFile)) {
              QFile::remove(destinationFile);
            } else {
              continue;
            }
          }
          if (!QFile::copy(fileInfo.absoluteFilePath(), destinationFile)) {
            qCritical("failed to copy %s to %s",
                      fileInfo.absoluteFilePath().toUtf8().constData(),
                      destinationFile.toUtf8().constData());
          }
        }
      }
    }
  }
  refreshLocalSaves();
  refreshLocalCharacters();
}

void TransferSavesDialog::on_moveToGlobalBtn_clicked()
{
  QString selectedCharacter = ui->localCharacterList->currentItem()->text();
  if (QMessageBox::question(this, tr("Confirm"),
      tr("Move all save games of character \"%1\" to the global location? Please be aware "
         "that this will mess up the running number of save games.").arg(selectedCharacter),
      QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {

    QDir destination = m_GamePlugin->savesDirectory();
    OverwriteMode overwriteMode = OVERWRITE_ASK;
    for (std::vector<SaveGame*>::const_iterator iter = m_LocalSaves.begin();
         iter != m_LocalSaves.end(); ++iter) {
      if ((*iter)->pcName() == selectedCharacter) {
        QStringList files = (*iter)->saveFiles();
        foreach (const QString &file, files) {
          QFileInfo fileInfo(file);
          QString destinationFile = destination.filePath(fileInfo.fileName());
          if (QFile::exists(destinationFile)) {
            if (testOverwrite(overwriteMode, destinationFile)) {
              QFile::remove(destinationFile);
            } else {
              continue;
            }
          }
          if (!QFile::rename(fileInfo.absoluteFilePath(), destinationFile)) {
            qCritical("failed to move %s to %s",
                      fileInfo.absoluteFilePath().toUtf8().constData(),
                      destinationFile.toUtf8().constData());
          }
        }
      }
    }
  }
  refreshGlobalSaves();
  refreshGlobalCharacters();
  refreshLocalSaves();
  refreshLocalCharacters();
}

void TransferSavesDialog::on_copyToGlobalBtn_clicked()
{
  QString selectedCharacter = ui->localCharacterList->currentItem()->text();
  if (QMessageBox::question(this, tr("Confirm"),
      tr("Copy all save games of character \"%1\" to the global location? Please be aware "
         "that this will mess up the running number of save games.").arg(selectedCharacter),
      QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {

    QDir destination = m_GamePlugin->savesDirectory();
    OverwriteMode overwriteMode = OVERWRITE_ASK;
    for (std::vector<SaveGame*>::const_iterator iter = m_LocalSaves.begin();
         iter != m_LocalSaves.end(); ++iter) {
      if ((*iter)->pcName() == selectedCharacter) {
        QStringList files = (*iter)->saveFiles();
        foreach (const QString &file, files) {
          QFileInfo fileInfo(file);
          QString destinationFile = destination.filePath(fileInfo.fileName());
          if (QFile::exists(destinationFile)) {
            if (testOverwrite(overwriteMode, destinationFile)) {
              QFile::remove(destinationFile);
            } else {
              continue;
            }
          }
          if (!QFile::copy(fileInfo.absoluteFilePath(), destinationFile)) {
            qCritical("failed to copy %s to %s",
                      fileInfo.absoluteFilePath().toUtf8().constData(),
                      destinationFile.toUtf8().constData());
          }
        }
      }
    }
  }
  refreshGlobalSaves();
  refreshGlobalCharacters();
}

void TransferSavesDialog::on_doneButton_clicked()
{
  close();
}

void TransferSavesDialog::on_globalCharacterList_currentTextChanged(const QString &currentText)
{
  ui->globalSavesList->clear();
  for (std::vector<SaveGame*>::const_iterator iter = m_GlobalSaves.begin();
       iter != m_GlobalSaves.end(); ++iter) {
    if ((*iter)->pcName() == currentText) {
      ui->globalSavesList->addItem(QFileInfo((*iter)->fileName()).fileName());
    }
  }
}

void TransferSavesDialog::on_localCharacterList_currentTextChanged(const QString &currentText)
{
  ui->localSavesList->clear();
  for (std::vector<SaveGame*>::const_iterator iter = m_LocalSaves.begin();
       iter != m_LocalSaves.end(); ++iter) {
    if ((*iter)->pcName() == currentText) {
      ui->localSavesList->addItem(QFileInfo((*iter)->fileName()).fileName());
    }
  }
}
