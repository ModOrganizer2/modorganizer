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
class OrganizerCore;

/** helper class to manage custom overwrites within the edit executables
 *  dialog
 **/
class CustomOverwrites
{
public:
  struct Info
  {
    bool enabled;
    QString modName;
  };

  void load(Profile* p, const ExecutablesList& exes);
  std::optional<Info> find(const QString& title) const;

  void setEnabled(const QString& title, bool b);
  void setMod(const QString& title, const QString& mod);
  void rename(const QString& oldTitle, const QString& newTitle);
  void remove(const QString& title);

private:
  std::map<QString, Info> m_map;
};


/** helper class to manage forced libraries within the edit executables dialog
 **/
class ForcedLibraries
{
public:
  using list_type = QList<MOBase::ExecutableForcedLoadSetting>;

  struct Info
  {
    bool enabled;
    list_type list;
  };

  void load(Profile* p, const ExecutablesList& exes);
  std::optional<Info> find(const QString& title) const;

  void setEnabled(const QString& title, bool b);
  void setList(const QString& title, const list_type& list);
  void rename(const QString& oldTitle, const QString& newTitle);
  void remove(const QString& title);

private:
  std::map<QString, Info> m_map;
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
  explicit EditExecutablesDialog(OrganizerCore& oc, QWidget* parent=nullptr);

  ~EditExecutablesDialog();

  ExecutablesList getExecutablesList() const;
  const CustomOverwrites& getCustomOverwrites() const;
  const ForcedLibraries& getForcedLibraries() const;

private slots:
  void on_list_itemSelectionChanged();

  void on_reset_clicked();
  void on_add_clicked();
  void on_remove_clicked();
  void on_up_clicked();
  void on_down_clicked();

  void on_title_textChanged(const QString& s);
  void on_overwriteSteamAppID_toggled(bool checked);
  void on_createFilesInMod_toggled(bool checked);
  void on_forceLoadLibraries_toggled(bool checked);

  void on_browseBinary_clicked();
  void on_browseWorkingDirectory_clicked();
  void on_configureLibraries_clicked();

  void on_buttons_clicked(QAbstractButton* b);

private:
  std::unique_ptr<Ui::EditExecutablesDialog> ui;
  OrganizerCore& m_organizerCore;
  const ExecutablesList m_originalExecutables;
  ExecutablesList m_executablesList;
  CustomOverwrites m_customOverwrites;
  ForcedLibraries m_forcedLibraries;
  bool m_settingUI;


  QListWidgetItem* selectedItem();
  Executable* selectedExe();

  void fillList();
  QListWidgetItem* createListItem(const Executable& exe);
  void updateUI(const QListWidgetItem* item, const Executable* e);
  void clearEdits();
  void setEdits(const Executable& e);
  void setButtons(const QListWidgetItem* item, const Executable* e);
  void save();
  void saveOrder();
  bool canMove(const QListWidgetItem* item, int direction);
  void move(QListWidgetItem* item, int direction);
  void setJarBinary(const QString& binaryName);
  bool isTitleConflicting(const QString& s);
  void commitChanges();
  void setDirty(bool b);
};

#endif // EDITEXECUTABLESDIALOG_H
