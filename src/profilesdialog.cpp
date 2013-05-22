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
#include "profile.h"
#include "report.h"
#include "utility.h"
#include "transfersavesdialog.h"
#include "profileinputdialog.h"
#include "mainwindow.h"
#include <gameinfo.h>
#include <QListWidgetItem>
#include <QInputDialog>
#include <QLineEdit>
#include <QDirIterator>
#include <QMessageBox>
#include <QWhatsThis>


using namespace MOBase;
using namespace MOShared;

Q_DECLARE_METATYPE(Profile::Ptr)


ProfilesDialog::ProfilesDialog(const QString &gamePath, QWidget *parent)
  : TutorableDialog("Profiles", parent), ui(new Ui::ProfilesDialog), m_GamePath(gamePath), m_FailState(false)
{
  ui->setupUi(this);

  QDir profilesDir(QDir::fromNativeSeparators(ToQString(GameInfo::instance().getProfilesDir())));
  profilesDir.setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);
  m_ProfilesList = findChild<QListWidget*>("profilesList");

  QDirIterator profileIter(profilesDir);

  while (profileIter.hasNext()) {
    profileIter.next();
    addItem(profileIter.filePath());
  }

  QCheckBox *invalidationBox = findChild<QCheckBox*>("invalidationBox");
  if (!GameInfo::instance().requiresBSAInvalidation()) {
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


void ProfilesDialog::addItem(const QString &name)
{
  QDir profileDir(name);
  QListWidgetItem *newItem = new QListWidgetItem(profileDir.dirName(), m_ProfilesList);
  try {
    newItem->setData(Qt::UserRole, QVariant::fromValue(Profile::Ptr(new Profile(profileDir))));
    m_FailState = false;
  } catch (const std::exception& e) {
    reportError(tr("failed to create profile: %1").arg(e.what()));
  }
}


void ProfilesDialog::createProfile(const QString &name, bool useDefaultSettings)
{
  try {
    QListWidget *profilesList = findChild<QListWidget*>("profilesList");
    QListWidgetItem *newItem = new QListWidgetItem(name, profilesList);
    newItem->setData(Qt::UserRole, QVariant::fromValue(Profile::Ptr(new Profile(name, useDefaultSettings))));
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
    newItem->setData(Qt::UserRole, QVariant::fromValue(Profile::Ptr(Profile::createPtrFrom(name, reference))));
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
  if (okClicked && (name.size() > 0)) {
    QListWidget *profilesList = findChild<QListWidget*>("profilesList");

    try {
      const Profile::Ptr currentProfile = profilesList->currentItem()->data(Qt::UserRole).value<Profile::Ptr>();
      createProfile(name, *currentProfile);
    } catch (const std::exception &e) {
      reportError(tr("failed to copy profile: %1").arg(e.what()));
    }
  }
}

void ProfilesDialog::on_removeProfileButton_clicked()
{
  QMessageBox confirmBox(QMessageBox::Question, tr("Confirm"), tr("Are you sure you want to remove this profile?"),
                         QMessageBox::Yes | QMessageBox::No);

  if (confirmBox.exec() == QMessageBox::Yes) {
    QListWidget *profilesList = findChild<QListWidget*>("profilesList");

    Profile::Ptr currentProfile = profilesList->currentItem()->data(Qt::UserRole).value<Profile::Ptr>();

    // on destruction, the profile object would write the profile.ini file again, so
    // we have to get rid of the it before deleting the directory
    QString profilePath = currentProfile->getPath();
    QListWidgetItem* item = profilesList->takeItem(profilesList->currentRow());
    if (item != NULL) {
      delete item;
    }
    shellDelete(QStringList(profilePath));
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
                                 QLineEdit::Normal, currentProfile->getName(),
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
  QListWidget *profilesList = findChild<QListWidget*>("profilesList");

  QListWidgetItem *currentItem = profilesList->currentItem();
  if (currentItem == NULL) {
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
      currentProfile->activateInvalidation(m_GamePath + "/data");
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

  if (current != NULL) {
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
  TransferSavesDialog transferDialog(*currentProfile, this);
  transferDialog.exec();
}
