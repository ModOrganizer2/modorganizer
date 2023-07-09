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

#include "executableslist.h"
#include "iplugingame.h"
#include "profile.h"
#include "tutorabledialog.h"
#include <QAbstractButton>
#include <QListWidgetItem>
#include <QTimer>
#include <optional>

namespace Ui
{
class EditExecutablesDialog;
}

class ModList;
class OrganizerCore;

/** helper class to manage custom overwrites within the edit executables
 *  dialog, stores a T and a bool in map indexed by a QString
 **/
template <class T>
class ToggableMap
{
public:
  struct Value
  {
    bool enabled;
    T value;

    Value(bool b, T&& v) : enabled(b), value(std::forward<T>(v)) {}
  };

  /**
   * returns the Value associated with the given title, or empty
   **/
  std::optional<Value> find(const QString& title) const
  {
    auto itor = m_map.find(title);
    if (itor == m_map.end()) {
      return {};
    }

    return itor->second;
  }

  /**
   * sets the given value, adds it if not found
   **/
  void set(QString title, bool b, T value)
  {
    m_map.insert_or_assign(std::move(title), Value(b, std::move(value)));
  }

  /**
   * sets whether the given value is enabled, inserts it if not found
   **/
  void setEnabled(const QString& title, bool b)
  {
    auto itor = m_map.find(title);

    if (itor == m_map.end()) {
      m_map.emplace(title, Value(b, {}));
    } else {
      itor->second.enabled = b;
    }
  }

  /**
   * sets the given value, inserts it enabled if not found
   **/
  void setValue(const QString& title, T value)
  {
    auto itor = m_map.find(title);

    if (itor == m_map.end()) {
      m_map.emplace(title, Value(true, std::move(value)));
    } else {
      itor->second.value = std::move(value);
    }
  }

  /**
   * renames the given value, ignored if not found
   **/
  void rename(const QString& oldTitle, QString newTitle)
  {
    auto itor = m_map.find(oldTitle);
    if (itor == m_map.end()) {
      return;
    }

    // move to new title, erase old
    m_map.emplace(std::move(newTitle), std::move(itor->second));
    m_map.erase(itor);
  }

  /**
   * removes the given value, ignored if not found
   **/
  void remove(const QString& title)
  {
    auto itor = m_map.find(title);
    if (itor == m_map.end()) {
      return;
    }

    m_map.erase(itor);
  }

private:
  std::map<QString, Value> m_map;
};

/**
 * @brief Dialog to manage the list of executables
 **/
class EditExecutablesDialog : public MOBase::TutorableDialog
{
  Q_OBJECT;
  friend class IgnoreChanges;

public:
  using CustomOverwrites = ToggableMap<QString>;
  using ForcedLibraries  = ToggableMap<QList<MOBase::ExecutableForcedLoadSetting>>;

  explicit EditExecutablesDialog(OrganizerCore& oc, int selection = -1,
                                 QWidget* parent = nullptr);

  ~EditExecutablesDialog();

  // also saves and restores geometry
  //
  int exec() override;

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
  void on_title_editingFinished();
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

  // copy of the original executables, used to clear the current settings when
  // committing changes
  const ExecutablesList m_originalExecutables;

  // current executable list
  ExecutablesList m_executablesList;

  // custom overwrites set in the dialog
  CustomOverwrites m_customOverwrites;

  // forced libraries set in the dialog
  ForcedLibraries m_forcedLibraries;

  // remembers the last executable title that made sense, reverts to this when
  // the widget loses focus if it's empty
  QString m_lastGoodTitle;

  // true when the change events being triggered are in response to loading
  // the executable's data into the UI, not from a user change
  bool m_settingUI;

  void loadCustomOverwrites();
  void loadForcedLibraries();

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
  bool isTitleConflicting(const QString& s);
  bool commitChanges();
  void setDirty(bool b);
  void selectIndex(int i);
  bool checkOutputMods(const ExecutablesList& exes);

  void addFromFile();
  void addEmpty();
  void clone();
  void addNew(Executable e);

  QFileInfo browseBinary(const QString& initial);
  void setBinary(const QFileInfo& binary);
  void setJarBinary(const QFileInfo& binary);
};

#endif  // EDITEXECUTABLESDIALOG_H
