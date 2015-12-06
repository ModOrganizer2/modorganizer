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

#include "profilesdialog.h"
#include "ui_profilesdialog.h"
#include "report.h"
#include "utility.h"
#include "transfersavesdialog.h"
#include "profileinputdialog.h"
#include "mainwindow.h"
#include "aboutdialog.h"
#include <iplugingame.h>
#include <bsainvalidation.h>
#include <appconfig.h>
#include <QListWidgetItem>
#include <QInputDialog>
#include <QLineEdit>
#include <QDirIterator>
#include <QMessageBox>
#include <QWhatsThis>


using namespace MOBase;
using namespace MOShared;

Q_DECLARE_METATYPE(Profile::Ptr)


ProfilesDialog::ProfilesDialog(const QString &profileName, MOBase::IPluginGame const *game, QWidget *parent)
  : TutorableDialog("Profiles", parent)
  , ui(new Ui::ProfilesDialog)
  , m_FailState(false)
  , m_Game(game)
{
  ui->setupUi(this);

  QDir profilesDir(qApp->property("dataPath").toString() + "/" + QString::fromStdWString(AppConfig::profilesPath()));
  profilesDir.setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);
  m_ProfilesList = findChild<QListWidget*>("profilesList");

  QDirIterator profileIter(profilesDir);

  while (profileIter.hasNext()) {
    profileIter.next();
    QListWidgetItem *item = addItem(profileIter.filePath());
    if (profileName == profileIter.fileName()) {
      m_ProfilesList->setCurrentItem(item);
    }
  }

  QCheckBox *invalidationBox = findChild<QCheckBox*>("invalidationBox");

  BSAInvalidation *invalidation = game->feature<BSAInvalidation>();

  if (invalidation == nullptr) {
    invalidationBox->setToolTip(tr("Archive invalidation isn't required for this game."));
    invalidationBox->setEnabled(false);
  }
}

ProfilesDialog::~ProfilesDialog()
{
  delete ui;
}

void ProfilesDialog::showEvent(QShowEvent *event)
{
  TutorableDialog::showEvent(event);

  if (m_ProfilesList->count() == 0) {
    QPoint pos = m_ProfilesList->mapToGlobal(QPoint(0, 0));
    pos.rx() += m_ProfilesList->width() / 2;
    pos.ry() += (m_ProfilesList->height() / 2) - 20;
    QWhatsThis::showText(pos,
        QObject::tr("Before you can use ModOrganizer, you need to create at least one profile. "
                    "ATTENTION: Run the game at least once before creating a profile!"), m_ProfilesList);
  }
}

void ProfilesDialog::on_closeButton_clicked()
{
  close();
}


QListWidgetItem *ProfilesDialog::addItem(const QString &name)
{
  QDir profileDir(name);
  QListWidgetItem *newItem = new QListWidgetItem(profileDir.dirName(), m_ProfilesList);
  try {
    newItem->setData(Qt::UserRole, QVariant::fromValue(Profile::Ptr(new Profile(profileDir, m_Game))));
    m_FailState = false;
  } catch (const std::exception& e) {
    reportError(tr("failed to create profile: %1").arg(e.what()));
  }
  return newItem;
}

void ProfilesDialog::createProfile(const QString &name, bool useDefaultSettings)
{
  try {
    QListWidget *profilesList = findChild<QListWidget*>("profilesList");
    QListWidgetItem *newItem = new QListWidgetItem(name, profilesList);
    newItem->setData(Qt::UserRole, QVariant::fromValue(Profile::Ptr(new Profile(name, m_Game, useDefaultSettings))));
    profilesList->addItem(newItem);
    m_FailState = false;
  } catch (const std::exception&) {
    m_FailState = true;
    throw;
  }
}

void ProfilesDialog::createProfile(const QString &name, const Profile &reference)
{
  try {
    QListWidget *profilesList = findChild<QListWidget*>("profilesList");
    QListWidgetItem *newItem = new QListWidgetItem(name, profilesList);
    newItem->setData(Qt::UserRole, QVariant::fromValue(Profile::Ptr(Profile::createPtrFrom(name, reference, m_Game))));
    profilesList->addItem(newItem);
    m_FailState = false;
  } catch (const std::exception&) {
    m_FailState = true;
    throw;
  }
}

void ProfilesDialog::on_addProfileButton_clicked()
{
  ProfileInputDialog dialog(this);
  bool okClicked = dialog.exec();
  QString name = dialog.getName();

  if (okClicked && (name.size() > 0)) {
    try {
      createProfile(name, dialog.getPreferDefaultSettings());
    } catch (const std::exception &e) {
      reportError(tr("failed to create profile: %1").arg(e.what()));
    }
  }
}

void ProfilesDialog::on_copyProfileButton_clicked()
{
  bool okClicked;
  QString name = QInputDialog::getText(this, tr("Name"), tr("Please enter a name for the new profile"), QLineEdit::Normal, QString(), &okClicked);
  fixDirectoryName(name);
  if (okClicked) {
    if (name.size() > 0) {
      QListWidget *profilesList = findChild<QListWidget*>("profilesList");

      try {
        const Profile::Ptr currentProfile = profilesList->currentItem()->data(Qt::UserRole).value<Profile::Ptr>();
        createProfile(name, *currentProfile);
      } catch (const std::exception &e) {
        reportError(tr("failed to copy profile: %1").arg(e.what()));
      }
    } else {
      QMessageBox::warning(this, tr("Invalid name"), tr("Invalid profile name"));
    }
  }
}

void ProfilesDialog::on_removeProfileButton_clicked()
{
  QMessageBox confirmBox(QMessageBox::Question, tr("Confirm"), tr("Are you sure you want to remove this profile (including local savegames if any)?"),
                         QMessageBox::Yes | QMessageBox::No);

  if (confirmBox.exec() == QMessageBox::Yes) {
    QListWidget *profilesList = findChild<QListWidget*>("profilesList");

    Profile::Ptr currentProfile = profilesList->currentItem()->data(Qt::UserRole).value<Profile::Ptr>();
    QString profilePath;
    if (currentProfile.get() == nullptr) {
      profilePath = qApp->property("dataPath").toString()
                  + "/" + QString::fromStdWString(AppConfig::profilesPath())
                  + "/" + profilesList->currentItem()->text();
      if (QMessageBox::question(this, tr("Profile broken"),
            tr("This profile you're about to delete seems to be broken or the path is invalid. "
            "I'm about to delete the following folder: \"%1\". Proceed?").arg(profilePath), QMessageBox::Yes | QMessageBox::No) == QMessageBox::No) {
        return;
      }
    } else {
      // on destruction, the profile object would write the profile.ini file again, so
      // we have to get rid of the it before deleting the directory
      profilePath = currentProfile->absolutePath();
    }
    QListWidgetItem* item = profilesList->takeItem(profilesList->currentRow());
    if (item != nullptr) {
      delete item;
    }
    if (!shellDelete(QStringList(profilePath))) {
      qWarning("Failed to shell-delete \"%s\" (errorcode %lu), trying regular delete", qPrintable(profilePath), ::GetLastError());
      if (!removeDir(profilePath)) {
        qWarning("regular delete failed too");
      }
    }
  }
}


void ProfilesDialog::on_renameButton_clicked()
{
  Profile::Ptr currentProfile = ui->profilesList->currentItem()->data(Qt::UserRole).value<Profile::Ptr>();

  bool valid = false;
  QString name;

  while (!valid) {
    bool ok = false;
    name = QInputDialog::getText(this, tr("Rename Profile"), tr("New Name"),
                                 QLineEdit::Normal, currentProfile->name(),
                                 &ok);
    valid = fixDirectoryName(name);
    if (!ok) {
      return;
    }
  }

  ui->profilesList->currentItem()->setText(name);
  currentProfile->rename(name);
}


void ProfilesDialog::on_invalidationBox_stateChanged(int state)
{
  QListWidgetItem *currentItem = ui->profilesList->currentItem();
  if (currentItem == nullptr) {
    return;
  }
  if (!ui->invalidationBox->isEnabled()) {
    return;
  }
  try {
    QVariant currentProfileVariant = currentItem->data(Qt::UserRole);
    if (!currentProfileVariant.isValid() || currentProfileVariant.isNull()) {
      return;
    }
    const Profile::Ptr currentProfile = currentItem->data(Qt::UserRole).value<Profile::Ptr>();
    if (state == Qt::Unchecked) {
      currentProfile->deactivateInvalidation();
    } else {
      currentProfile->activateInvalidation();
    }
  } catch (const std::exception &e) {
    reportError(tr("failed to change archive invalidation state: %1").arg(e.what()));
  }
}


void ProfilesDialog::on_profilesList_currentItemChanged(QListWidgetItem *current, QListWidgetItem*)
{
  QCheckBox *invalidationBox = findChild<QCheckBox*>("invalidationBox");
  QCheckBox *localSavesBox = findChild<QCheckBox*>("localSavesBox");
  QPushButton *copyButton = findChild<QPushButton*>("copyProfileButton");
  QPushButton *removeButton = findChild<QPushButton*>("removeProfileButton");
  QPushButton *transferButton = findChild<QPushButton*>("transferButton");
  QPushButton *renameButton = findChild<QPushButton*>("renameButton");

  if (current != nullptr) {
    if (!current->data(Qt::UserRole).isValid()) return;
    const Profile::Ptr currentProfile = current->data(Qt::UserRole).value<Profile::Ptr>();

    try {
      bool invalidationSupported = false;
      invalidationBox->blockSignals(true);
      invalidationBox->setChecked(currentProfile->invalidationActive(&invalidationSupported));
      invalidationBox->setEnabled(invalidationSupported);
      invalidationBox->blockSignals(false);

      bool localSaves = currentProfile->localSavesEnabled();
      transferButton->setEnabled(localSaves);
      // prevent the stateChanged-event for the saves-box from triggering, otherwise it may think local saves
      // were disabled and delete the files/rename the dir
      localSavesBox->blockSignals(true);
      localSavesBox->setChecked(localSaves);
      localSavesBox->blockSignals(false);

      copyButton->setEnabled(true);
      removeButton->setEnabled(true);
      renameButton->setEnabled(true);
    } catch (const std::exception& E) {
      reportError(tr("failed to determine if invalidation is active: %1").arg(E.what()));
      copyButton->setEnabled(false);
      removeButton->setEnabled(false);
      renameButton->setEnabled(false);
      invalidationBox->setChecked(false);
    }
  } else {
    invalidationBox->setChecked(false);
    copyButton->setEnabled(false);
    removeButton->setEnabled(false);
    renameButton->setEnabled(false);
  }
}

void ProfilesDialog::on_localSavesBox_stateChanged(int state)
{
  Profile::Ptr currentProfile = m_ProfilesList->currentItem()->data(Qt::UserRole).value<Profile::Ptr>();

  if (currentProfile->enableLocalSaves(state == Qt::Checked)) {
    ui->transferButton->setEnabled(state == Qt::Checked);
  } else {
    // revert checkbox-state
    ui->localSavesBox->setChecked(state != Qt::Checked);
  }
}

void ProfilesDialog::on_transferButton_clicked()
{
  const Profile::Ptr currentProfile = m_ProfilesList->currentItem()->data(Qt::UserRole).value<Profile::Ptr>();
  TransferSavesDialog transferDialog(*currentProfile, m_Game, this);
  transferDialog.exec();
}
