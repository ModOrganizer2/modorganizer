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
#include "modinfodialogfwd.h"

namespace Ui { class ModInfoDialog; }
namespace MOShared { class FilesOrigin; }

class PluginContainer;
class OrganizerCore;
class Settings;
class ModInfoDialogTab;
class MainWindow;

/**
 * this is a larger dialog used to visualise information about the mod.
 * @todo this would probably a good place for a plugin-system
 **/
class ModInfoDialog : public MOBase::TutorableDialog
{
  Q_OBJECT;

  template <class T>
  friend std::unique_ptr<ModInfoDialogTab> createTab(
    ModInfoDialog& d, ModInfoTabIDs index);

public:
  ModInfoDialog(
    MainWindow* mw, OrganizerCore* core, PluginContainer* plugin,
    ModInfo::Ptr mod);

  ~ModInfoDialog();

  void setMod(ModInfo::Ptr mod);
  void setMod(const QString& name);

  void setTab(ModInfoTabIDs id);

  int exec() override;

  void saveState(Settings& s) const;
  void restoreState(const Settings& s);

signals:
  void originModified(int originID);

protected:
  void closeEvent(QCloseEvent* e);

private slots:
  void on_closeButton_clicked();
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
  ModInfoTabIDs m_initialTab;
  bool m_arrangingTabs;

  std::vector<TabInfo> createTabs();
  TabInfo* currentTab();
  void restoreTabState(const QString& state);
  QString saveTabState() const;
  void update(bool firstTime=false);
  void onDeleteShortcut();
  MOShared::FilesOrigin* getOrigin();
  void setTabsVisibility(bool firstTime);
  void updateTabs(bool becauseOriginChanged=false);
  void feedFiles(bool becauseOriginChanged);
  void setTabsColors();
  void switchToTab(ModInfoTabIDs id);
  void reAddTabs(const std::vector<bool>& visibility, ModInfoTabIDs sel);
  std::vector<QString> getOrderedTabNames() const;
  bool tryClose();

  void onOriginModified(int originID);
  void onTabChanged();
  void onTabMoved();
};

#endif // MODINFODIALOG_H
