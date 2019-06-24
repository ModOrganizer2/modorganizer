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

#ifndef MODINFODIALOG_H
#define MODINFODIALOG_H


#include "modinfo.h"
#include "tutorabledialog.h"
#include "filerenamer.h"

namespace Ui { class ModInfoDialog; }
namespace MOShared { class FilesOrigin; }

class PluginContainer;
class OrganizerCore;
class Settings;
class ModInfoDialogTab;
class MainWindow;

bool canPreviewFile(PluginContainer& pluginContainer, bool isArchive, const QString& filename);
bool canOpenFile(bool isArchive, const QString& filename);
bool canHideFile(bool isArchive, const QString& filename);
bool canUnhideFile(bool isArchive, const QString& filename);

FileRenamer::RenameResults hideFile(FileRenamer& renamer, const QString &oldName);
FileRenamer::RenameResults unhideFile(FileRenamer& renamer, const QString &oldName);

int naturalCompare(const QString& a, const QString& b);


/**
 * this is a larger dialog used to visualise information about the mod.
 * @todo this would probably a good place for a plugin-system
 **/
class ModInfoDialog : public MOBase::TutorableDialog
{
    Q_OBJECT

public:
  enum ETabs {
    TAB_TEXTFILES,
    TAB_INIFILES,
    TAB_IMAGES,
    TAB_ESPS,
    TAB_CONFLICTS,
    TAB_CATEGORIES,
    TAB_NEXUS,
    TAB_NOTES,
    TAB_FILETREE
  };

 /**
  * @brief constructor
  *
  * @param modInfo info structure about the mod to display
  * @param parent parend widget
  **/
  ModInfoDialog(MainWindow* mw, OrganizerCore* core, PluginContainer* plugin);

  ~ModInfoDialog();

  /**
   * @brief retrieve the (user-modified) version of the mod
   *
   * @return the (user-modified) version of the mod
   **/
  QString getModVersion() const;

  /**
   * @brief retrieve the (user-modified) mod id
   *
   * @return the (user-modified) id of the mod
   **/
  const int getModID() const;

  void setMod(ModInfo::Ptr mod);
  void setMod(const QString& name);
  void setTab(ETabs id);

  int exec() override;

  void saveState(Settings& s) const;
  void restoreState(const Settings& s);

signals:
  void originModified(int originID);

private slots:
  void on_closeButton_clicked();
  void on_tabWidget_currentChanged(int index);
  void on_nextButton_clicked();
  void on_prevButton_clicked();

private:
  struct TabInfo
  {
    std::unique_ptr<ModInfoDialogTab> tab;
    int realPos;
    QWidget* widget;
    QString caption;
    QIcon icon;

    TabInfo(std::unique_ptr<ModInfoDialogTab> tab);
    bool isVisible() const;
  };

  std::unique_ptr<Ui::ModInfoDialog> ui;
  MainWindow* m_mainWindow;
  ModInfo::Ptr m_mod;
  OrganizerCore* m_core;
  PluginContainer* m_plugin;
  std::vector<TabInfo> m_tabs;
  ETabs m_initialTab;

  std::vector<TabInfo> createTabs();
  void restoreTabState(const QString& state);
  QString saveTabState() const;
  void update(bool firstTime=false);
  void onDeleteShortcut();
  MOShared::FilesOrigin* getOrigin();
  void setTabsVisibility(bool firstTime);
  void updateTabs(bool becauseOriginChanged=false);
  void feedFiles(bool becauseOriginChanged);
  void setTabsColors();
  void switchToTab(ETabs id);
  void reAddTabs(const std::vector<bool>& visibility, ETabs sel);
  std::vector<QString> getOrderedTabNames() const;
  void onOriginModified(std::size_t tabIndex, int originID);

  template <class T>
  std::unique_ptr<ModInfoDialogTab> createTab(int index)
  {
    return std::make_unique<T>(*m_core, *m_plugin, this, ui.get(), index);
  }
};

#endif // MODINFODIALOG_H
