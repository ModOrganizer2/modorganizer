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

#ifndef PROFILESDIALOG_H
#define PROFILESDIALOG_H

#include "tutorabledialog.h"
class Profile;
class OrganizerCore;

class QListWidget;
class QListWidgetItem;
#include <QObject>
class QString;

namespace Ui
{
class ProfilesDialog;
}

namespace MOBase
{
class IPluginGame;
}

/**
 * @brief Dialog that can be used to create/delete/modify profiles
 **/
class ProfilesDialog : public MOBase::TutorableDialog
{
  Q_OBJECT

public:
  /**
   * @brief constructor
   *
   * @param profileName currently enabled profile
   * @param organizer
   * @param parent parent widget
   **/
  explicit ProfilesDialog(const QString& profileName, OrganizerCore& organizer,
                          QWidget* parent = 0);
  ~ProfilesDialog();

  // also saves and restores geometry
  //
  int exec() override;

  /**
   * @return true if creation of a new profile failed
   * @todo the notion of a fail state makes little sense in the current dialog
   **/
  bool failed() const { return m_FailState; }

  // if the dialog was closed with the 'select' button, returns the name of the
  // selected profile; if the dialog was closed with 'cancel', returns empty
  //
  std::optional<QString> selectedProfile() const;

signals:

  /**
   * @brief Signal emitted when a profile is created.
   */
  void profileCreated(Profile* profile);

  /**
   * @brief Signal emitted when a profile is renamed.
   */
  void profileRenamed(Profile* profile, QString const& oldName, QString const& newName);

  /**
   * @brief Signal emitted when a profile has been removed.
   */
  void profileRemoved(QString const& profileName);

protected:
  virtual void showEvent(QShowEvent* event);

private slots:
  void on_localIniFilesBox_stateChanged(int state);

private:
  QListWidgetItem* addItem(const QString& name);
  void createProfile(const QString& name, bool useDefaultSettings);
  void createProfile(const QString& name, const Profile& reference);

private slots:

  void on_close_clicked();
  void on_select_clicked();

  void on_addProfileButton_clicked();

  void on_invalidationBox_stateChanged(int arg1);

  void on_copyProfileButton_clicked();

  void on_profilesList_currentItemChanged(QListWidgetItem* current,
                                          QListWidgetItem* previous);
  void on_profilesList_itemActivated(QListWidgetItem* item);

  void on_removeProfileButton_clicked();

  void on_localSavesBox_stateChanged(int arg1);

  void on_transferButton_clicked();

  void on_renameButton_clicked();

private:
  Ui::ProfilesDialog* ui;
  QListWidget* m_ProfilesList;
  bool m_FailState;
  MOBase::IPluginGame const* m_Game;
  QString m_ActiveProfileName;
  std::optional<QString> m_Selected;
};

#endif  // PROFILESDIALOG_H
