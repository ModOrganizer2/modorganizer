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
#include <optional>

namespace Ui {
    class EditExecutablesDialog;
}

class ModList;


/** helper class to manage custom overwrites within the edit executables
 *  dialog
 **/
class CustomOverwrites
{
public:
  void load(Profile* p, const ExecutablesList& exes);
  std::optional<QString> find(const QString& title) const;

  void set(const QString& title, const QString& mod);
  void rename(const QString& oldTitle, const QString& newTitle);
  void remove(const QString& title);

private:
  std::map<QString, QString> m_map;
};


/** helper class to manage forced libraries within the edit executables dialog
 **/
class ForcedLibraries
{
public:
  using list_type = QList<MOBase::ExecutableForcedLoadSetting>;

  void load(Profile* p, const ExecutablesList& exes);
  std::optional<list_type> find(const QString& title) const;

  void set(const QString& title, const list_type& list);
  void rename(const QString& oldTitle, const QString& newTitle);
  void remove(const QString& title);

private:
  std::map<QString, list_type> m_map;
};


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
  CustomOverwrites m_customOverwrites;
  ForcedLibraries m_forcedLibraries;
  Profile *m_profile;
  const MOBase::IPluginGame *m_gamePlugin;
  bool m_settingUI;

  QListWidgetItem *m_currentItem;


  QListWidgetItem* selectedItem();
  Executable* selectedExe();

  void fillExecutableList();
  QListWidgetItem* createListItem(const Executable& exe);
  void updateUI(const Executable* e);
  void clearEdits();
  void setEdits(const Executable& e);
  void save();
  void setJarBinary(const QString& binaryName);
  QString newExecutableTitle();

  bool executableChanged();
  void updateButtonStates();
  void saveExecutable();

};

#endif // EDITEXECUTABLESDIALOG_H
