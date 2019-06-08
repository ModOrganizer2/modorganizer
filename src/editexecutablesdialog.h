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

#ifndef EDITEXECUTABLESDIALOG_H
#define EDITEXECUTABLESDIALOG_H

#include "tutorabledialog.h"
#include <QListWidgetItem>
#include "executableslist.h"
#include "profile.h"
#include "iplugingame.h"
#include <QTimer>
#include <QAbstractButton>

namespace Ui {
    class EditExecutablesDialog;
}

class ModList;

/**
 * @brief Dialog to manage the list of executables
 **/
class EditExecutablesDialog : public MOBase::TutorableDialog
{
    Q_OBJECT

public:
  /**
   * @param executablesList current list of executables
   * @param parent parent widget
   **/
  explicit EditExecutablesDialog(
    const ExecutablesList &executablesList, const ModList &modList,
    Profile *profile, const MOBase::IPluginGame *game, QWidget *parent = 0);

  ~EditExecutablesDialog();

  /**
   * @brief retrieve the updated list of executables
   * @return updated list of executables
   **/
  ExecutablesList getExecutablesList() const;

private slots:
  void on_list_itemSelectionChanged();

  void on_add_clicked();
  void on_remove_clicked();

  void on_title_textChanged(const QString& s);
  void on_overwriteSteamAppID_toggled(bool checked);
  void on_createFilesInMod_toggled(bool checked);
  void on_forceLoadLibraries_toggled(bool checked);

  void on_browseBinary_clicked();
  void on_browseWorkingDirectory_clicked();
  void on_configureLibraries_clicked();

  void on_buttons_accepted();
  void on_buttons_rejected();

  void delayedRefresh();

private:
  std::unique_ptr<Ui::EditExecutablesDialog> ui;
  ExecutablesList m_executablesList;
  std::map<QString, QString> m_customOverwrites;
  Profile *m_profile;
  const MOBase::IPluginGame *m_gamePlugin;
  bool m_settingUI;

  QListWidgetItem *m_currentItem;
  QList<MOBase::ExecutableForcedLoadSetting> m_forcedLibraries;


  QListWidgetItem* selectedItem();
  Executable* selectedExe();

  void fillExecutableList();
  void updateUI(const Executable* e);
  void clearEdits();
  void setEdits(const Executable& e);
  void save();

  void resetInput();
  bool executableChanged();
  void updateButtonStates();
  void saveExecutable();

};

#endif // EDITEXECUTABLESDIALOG_H
