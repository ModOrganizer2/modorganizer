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

#include "bsainvalidation.h"
#include "filesystemutilities.h"
#include "iplugingame.h"
#include "localsavegames.h"
#include "organizercore.h"
#include "profile.h"
#include "profileinputdialog.h"
#include "report.h"
#include "settings.h"
#include "shared/appconfig.h"
#include "transfersavesdialog.h"

#include <QDir>
#include <QDirIterator>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QWhatsThis>

#include <Windows.h>

#include <exception>

using namespace MOBase;
using namespace MOShared;

Q_DECLARE_METATYPE(Profile::Ptr)

ProfilesDialog::ProfilesDialog(const QString& profileName, OrganizerCore& organizer,
                               QWidget* parent)
    : TutorableDialog("Profiles", parent), ui(new Ui::ProfilesDialog),
      m_FailState(false), m_Game(organizer.managedGame()), m_ActiveProfileName("")
{
  ui->setupUi(this);

  QDir profilesDir(Settings::instance().paths().profiles());
  profilesDir.setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);

  QDirIterator profileIter(profilesDir);

  while (profileIter.hasNext()) {
    profileIter.next();
    QListWidgetItem* item = addItem(profileIter.filePath());
    if (profileName == profileIter.fileName()) {
      ui->profilesList->setCurrentItem(item);
      m_ActiveProfileName = profileName;
    }
  }

  BSAInvalidation* invalidation = m_Game->feature<BSAInvalidation>();

  if (invalidation == nullptr) {
    ui->invalidationBox->setToolTip(
        tr("Archive invalidation isn't required for this game."));
    ui->invalidationBox->setEnabled(false);
  }

  if (!m_Game->feature<LocalSavegames>()) {
    ui->localSavesBox->setToolTip(
        tr("This game does not support profile-specific game saves."));
    ui->localSavesBox->setEnabled(false);
  }

  connect(this, &ProfilesDialog::profileCreated, &organizer,
          &OrganizerCore::profileCreated);
  connect(this, &ProfilesDialog::profileRenamed, &organizer,
          &OrganizerCore::profileRenamed);
  connect(this, &ProfilesDialog::profileRemoved, &organizer,
          &OrganizerCore::profileRemoved);
}

ProfilesDialog::~ProfilesDialog()
{
  delete ui;
}

int ProfilesDialog::exec()
{
  GeometrySaver gs(Settings::instance(), this);
  return QDialog::exec();
}

void ProfilesDialog::showEvent(QShowEvent* event)
{
  TutorableDialog::showEvent(event);

  if (ui->profilesList->count() == 0) {
    QPoint pos = ui->profilesList->mapToGlobal(QPoint(0, 0));
    pos.rx() += ui->profilesList->width() / 2;
    pos.ry() += (ui->profilesList->height() / 2) - 20;
    QWhatsThis::showText(
        pos,
        QObject::tr(
            "Before you can use ModOrganizer, you need to create at least one profile. "
            "ATTENTION: Run the game at least once before creating a profile!"),
        ui->profilesList);
  }
}

void ProfilesDialog::on_close_clicked()
{
  close();
}

void ProfilesDialog::on_select_clicked()
{
  const Profile::Ptr currentProfile =
      ui->profilesList->currentItem()->data(Qt::UserRole).value<Profile::Ptr>();

  if (!currentProfile) {
    return;
  }

  m_Selected = currentProfile->name();
  close();
}

std::optional<QString> ProfilesDialog::selectedProfile() const
{
  return m_Selected;
}

QListWidgetItem* ProfilesDialog::addItem(const QString& name)
{
  QDir profileDir(name);
  QListWidgetItem* newItem =
      new QListWidgetItem(profileDir.dirName(), ui->profilesList);
  try {
    newItem->setData(Qt::UserRole, QVariant::fromValue(
                                       Profile::Ptr(new Profile(profileDir, m_Game))));
    m_FailState = false;
  } catch (const std::exception& e) {
    reportError(tr("failed to create profile: %1").arg(e.what()));
  }
  return newItem;
}

void ProfilesDialog::createProfile(const QString& name, bool useDefaultSettings)
{
  try {
    QListWidgetItem* newItem = new QListWidgetItem(name, ui->profilesList);
    auto profile = Profile::Ptr(new Profile(name, m_Game, useDefaultSettings));
    newItem->setData(Qt::UserRole, QVariant::fromValue(profile));
    ui->profilesList->addItem(newItem);
    m_FailState = false;
    ui->profilesList->setCurrentItem(newItem);
    emit profileCreated(profile.get());
  } catch (const std::exception&) {
    m_FailState = true;
    throw;
  }
}

void ProfilesDialog::createProfile(const QString& name, const Profile& reference)
{
  try {
    QListWidgetItem* newItem = new QListWidgetItem(name, ui->profilesList);
    auto profile = Profile::Ptr(Profile::createPtrFrom(name, reference, m_Game));
    newItem->setData(Qt::UserRole, QVariant::fromValue(profile));
    ui->profilesList->addItem(newItem);
    m_FailState = false;
    ui->profilesList->setCurrentItem(newItem);
    emit profileCreated(profile.get());
  } catch (const std::exception&) {
    m_FailState = true;
    throw;
  }
}

void ProfilesDialog::on_addProfileButton_clicked()
{
  ProfileInputDialog dialog(this);
  bool okClicked = dialog.exec();
  QString name   = dialog.getName();

  if (okClicked && (name.size() > 0)) {
    try {
      createProfile(name, dialog.getPreferDefaultSettings());
    } catch (const std::exception& e) {
      reportError(tr("failed to create profile: %1").arg(e.what()));
    }
  }
}

void ProfilesDialog::on_copyProfileButton_clicked()
{
  bool okClicked;
  QString name = QInputDialog::getText(this, tr("Name"),
                                       tr("Please enter a name for the new profile"),
                                       QLineEdit::Normal, QString(), &okClicked);
  fixDirectoryName(name);
  if (okClicked) {
    if (name.size() > 0) {
      try {
        const Profile::Ptr currentProfile =
            ui->profilesList->currentItem()->data(Qt::UserRole).value<Profile::Ptr>();
        createProfile(name, *currentProfile);
      } catch (const std::exception& e) {
        reportError(tr("failed to copy profile: %1").arg(e.what()));
      }
    } else {
      QMessageBox::warning(this, tr("Invalid name"), tr("Invalid profile name"));
    }
  }
}

void ProfilesDialog::on_removeProfileButton_clicked()
{
  Profile::Ptr profileToDelete =
      ui->profilesList->currentItem()->data(Qt::UserRole).value<Profile::Ptr>();
  if (profileToDelete->name() == m_ActiveProfileName) {
    QMessageBox::warning(this, tr("Deleting active profile"),
                         tr("Unable to delete active profile.  Please change to a "
                            "different profile first."));
    return;
  }

  QMessageBox confirmBox(QMessageBox::Question, tr("Confirm"),
                         tr("Are you sure you want to remove this profile (including "
                            "profile-specific save games, if any)?"),
                         QMessageBox::Yes | QMessageBox::No, this);

  if (confirmBox.exec() == QMessageBox::Yes) {
    QString profilePath;
    if (profileToDelete.get() == nullptr) {
      profilePath = Settings::instance().paths().profiles() + "/" +
                    ui->profilesList->currentItem()->text();
      if (QMessageBox::question(
              this, tr("Profile broken"),
              tr("This profile you're about to delete seems to be broken or the path "
                 "is invalid. "
                 "I'm about to delete the following folder: \"%1\". Proceed?")
                  .arg(profilePath),
              QMessageBox::Yes | QMessageBox::No) == QMessageBox::No) {
        return;
      }
    } else {
      // on destruction, the profile object would write the profile.ini file again, so
      // we have to get rid of the it before deleting the directory
      profilePath = profileToDelete->absolutePath();
    }
    QListWidgetItem* item = ui->profilesList->takeItem(ui->profilesList->currentRow());
    if (item != nullptr) {
      delete item;
    }
    if (!shellDelete(QStringList(profilePath))) {
      log::warn("Failed to shell-delete \"{}\" (errorcode {}), trying regular delete",
                profilePath, ::GetLastError());
      if (!removeDir(profilePath)) {
        log::warn("regular delete failed too");
      }
    }

    emit profileRemoved(profileToDelete->name());
  }
}

void ProfilesDialog::on_renameButton_clicked()
{
  Profile::Ptr currentProfile =
      ui->profilesList->currentItem()->data(Qt::UserRole).value<Profile::Ptr>();

  if (currentProfile->name() == m_ActiveProfileName) {
    QMessageBox::warning(this, tr("Renaming active profile"),
                         tr("The active profile cannot be renamed. Please change to a "
                            "different profile first."));
    return;
  }

  bool valid = false;
  QString name;

  while (!valid) {
    bool ok = false;
    name    = QInputDialog::getText(this, tr("Rename Profile"), tr("New Name"),
                                    QLineEdit::Normal, currentProfile->name(), &ok);
    valid   = fixDirectoryName(name);
    if (!ok) {
      return;
    }
  }

  ui->profilesList->currentItem()->setText(name);

  QString oldName = currentProfile->name();
  currentProfile->rename(name);

  emit profileRenamed(currentProfile.get(), oldName, name);
}

void ProfilesDialog::on_invalidationBox_stateChanged(int state)
{
  QListWidgetItem* currentItem = ui->profilesList->currentItem();
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
    const Profile::Ptr currentProfile =
        currentItem->data(Qt::UserRole).value<Profile::Ptr>();
    if (state == Qt::Unchecked) {
      currentProfile->deactivateInvalidation();
    } else {
      currentProfile->activateInvalidation();
    }
  } catch (const std::exception& e) {
    reportError(tr("failed to change archive invalidation state: %1").arg(e.what()));
  }
}

void ProfilesDialog::on_profilesList_currentItemChanged(QListWidgetItem* current,
                                                        QListWidgetItem*)
{
  if (current != nullptr) {
    if (!current->data(Qt::UserRole).isValid())
      return;
    const Profile::Ptr currentProfile =
        current->data(Qt::UserRole).value<Profile::Ptr>();

    try {
      bool invalidationSupported = false;
      ui->invalidationBox->blockSignals(true);
      ui->invalidationBox->setChecked(
          currentProfile->invalidationActive(&invalidationSupported));
      ui->invalidationBox->setEnabled(invalidationSupported);
      ui->invalidationBox->blockSignals(false);

      bool localSaves = currentProfile->localSavesEnabled();
      ui->transferButton->setEnabled(localSaves);
      // prevent the stateChanged-event for the saves-box from triggering, otherwise it
      // may think local saves were disabled and delete the files/rename the dir
      ui->localSavesBox->blockSignals(true);
      ui->localSavesBox->setChecked(localSaves);
      ui->localSavesBox->blockSignals(false);

      ui->copyProfileButton->setEnabled(true);
      ui->removeProfileButton->setEnabled(true);
      ui->renameButton->setEnabled(true);

      ui->localIniFilesBox->blockSignals(true);
      ui->localIniFilesBox->setChecked(currentProfile->localSettingsEnabled());
      ui->localIniFilesBox->blockSignals(false);
    } catch (const std::exception& E) {
      reportError(
          tr("failed to determine if invalidation is active: %1").arg(E.what()));
      ui->copyProfileButton->setEnabled(false);
      ui->removeProfileButton->setEnabled(false);
      ui->renameButton->setEnabled(false);
      ui->invalidationBox->setChecked(false);
    }
  } else {
    ui->invalidationBox->setChecked(false);
    ui->copyProfileButton->setEnabled(false);
    ui->removeProfileButton->setEnabled(false);
    ui->renameButton->setEnabled(false);
  }
}

void ProfilesDialog::on_profilesList_itemActivated(QListWidgetItem* item)
{
  on_select_clicked();
}

void ProfilesDialog::on_localSavesBox_stateChanged(int state)
{
  Profile::Ptr currentProfile =
      ui->profilesList->currentItem()->data(Qt::UserRole).value<Profile::Ptr>();

  if (currentProfile->enableLocalSaves(state == Qt::Checked)) {
    ui->transferButton->setEnabled(state == Qt::Checked);
  } else {
    // revert checkbox-state
    ui->localSavesBox->setChecked(state != Qt::Checked);
  }
}

void ProfilesDialog::on_transferButton_clicked()
{
  const Profile::Ptr currentProfile =
      ui->profilesList->currentItem()->data(Qt::UserRole).value<Profile::Ptr>();
  TransferSavesDialog transferDialog(*currentProfile, m_Game, this);
  transferDialog.exec();
}

void ProfilesDialog::on_localIniFilesBox_stateChanged(int state)
{
  Profile::Ptr currentProfile =
      ui->profilesList->currentItem()->data(Qt::UserRole).value<Profile::Ptr>();

  if (!currentProfile->enableLocalSettings(state == Qt::Checked)) {
    // revert checkbox-state
    ui->localIniFilesBox->setChecked(state != Qt::Checked);
  }
}
