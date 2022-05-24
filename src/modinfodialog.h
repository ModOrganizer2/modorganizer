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

#include "filerenamer.h"
#include "modinfo.h"
#include "modinfodialogfwd.h"
#include "tutorabledialog.h"

namespace Ui
{
class ModInfoDialog;
}
namespace MOShared
{
class FilesOrigin;
}

class PluginManager;
class OrganizerCore;
class Settings;
class ModInfoDialogTab;
class ModListView;

/**
 * this is a larger dialog used to visualise information about the mod.
 * @todo this would probably a good place for a plugin-system
 **/
class ModInfoDialog : public MOBase::TutorableDialog
{
  Q_OBJECT;

  // creates a tab, it's a friend because it uses a bunch of member variables
  // to create ModInfoDialogTabContext
  //
  template <class T>
  friend std::unique_ptr<ModInfoDialogTab> createTab(ModInfoDialog& d,
                                                     ModInfoTabIDs index);

public:
  ModInfoDialog(OrganizerCore& core, PluginManager& plugins, ModInfo::Ptr mod,
                ModListView* view, QWidget* parent = nullptr);

  ~ModInfoDialog();

  // switches to the tab with the given id
  //
  void selectTab(ModInfoTabIDs id);

  // updates all tabs, selects the initial tab, opens the dialog and
  // saves/restores geometry
  //
  int exec() override;

signals:
  // emitted when a tab changes the origin
  //
  void originModified(int originID);

  // emitted when the mod of the dialog is changed
  //
  void modChanged(unsigned int modIndex);

protected:
  // forwards to tryClose()
  //
  void closeEvent(QCloseEvent* e);

private:
  // represents a single tab
  //
  struct TabInfo
  {
    // tab implementation
    std::unique_ptr<ModInfoDialogTab> tab;

    // actual position in the tab bar, updated every time a tab is moved
    int realPos;

    // widget used by the QTabWidget for this tab
    //
    // because QTabWidget doesn't support simply hiding tabs, they have to be
    // completely removed from the widget when they don't support the current
    // mod
    //
    // therefore, `widget, `caption` and `icon` are remembered so tabs can be
    // removed and re-added when navigating between mods
    //
    // `widget` is also used figure out which tab is where when they're
    // re-ordered
    QWidget* widget;

    // caption for this tab, see `widget`
    QString caption;

    // icon for this tab, see `widget`
    QIcon icon;

    TabInfo(std::unique_ptr<ModInfoDialogTab> tab);

    // returns whether this tab is part of the tab widget
    //
    bool isVisible() const;
  };

  std::unique_ptr<Ui::ModInfoDialog> ui;
  OrganizerCore& m_core;
  PluginManager& m_plugins;
  ModListView* m_modListView;
  ModInfo::Ptr m_mod;
  std::vector<TabInfo> m_tabs;

  // initial tab requested by the main window when the dialog is opened; whether
  // the request can be honoured depends on what tabs are present
  ModInfoTabIDs m_initialTab;

  // set to true when tabs are being removed and re-added while navigating
  // between mods; since the current index changes while this is happening,
  // onTabSelectionChanged() will be called repeatedly
  //
  // however, it will check this flag and ignore the event so first activations
  // are not fired incorrectly
  bool m_arrangingTabs;

  // creates all the tabs and connects events
  //
  void createTabs();

  // saves the dialog state and calls saveState() on all tabs
  //
  void saveState() const;

  // restores the dialog state and calls restoreState() on all tabs
  //
  void restoreState();

  // sets the currently selected mod; resets first activation, but doesn't
  // update anything
  //
  void setMod(ModInfo::Ptr mod);

  // sets the currently selected mod, if found; forwards to setMod() above
  //
  void setMod(const QString& name);

  // returns the origin of the current mod, may be null
  //
  MOShared::FilesOrigin* getOrigin();

  // returns the currently selected tab, taking re-ordering in to account;
  // shouldn't be null, but could be
  //
  TabInfo* currentTab();

  // fully updates the dialog; sets the title, the tab visibility and updates
  // all the tabs; used when the current mod changes
  //
  // see setTabsVisibility() for firstTime
  //
  void update(bool firstTime = false);

  // builds the list of visible tabs; if the list is different from what's
  // currently displayed, or firstTime is true, forwards to reAddTabs()
  void setTabsVisibility(bool firstTime);

  // clears the tab widgets and re-adds the tabs having the visible flag in
  // the given vector, following the tab order from the settings
  //
  void reAddTabs(const std::vector<bool>& visibility, ModInfoTabIDs sel);

  // called by update(); clears tabs, feeds files and calls update() on all
  // tabs, then setTabsColors()
  //
  void updateTabs(bool becauseOriginChanged = false);

  // goes through all files on the filesystem for the current mod and calls
  // feedFile() on every tab until one accepts it
  //
  void feedFiles(std::vector<TabInfo*>& interestedTabs);

  // goes through all tabs and sets the tab text colour depending on whether
  // they have data or not
  //
  void setTabsColors();

  // called when the delete key is pressed anywhere in the dialog; forwards to
  // ModInfoDialogTab::deleteRequest() for the currently selected tab
  //
  void onDeleteShortcut();

  // finds the tab with the given id and selects it
  //
  void switchToTab(ModInfoTabIDs id);

  // saves the current tab order; used by saveState(), but also by
  // setTabsVisibility() to make sure any changes to order are saved before
  // re-adding tabs
  //
  void saveTabOrder() const;

  // asks all the tabs if they accept closing the dialog, returns false if one
  // objected
  //
  bool tryClose();

  // called when the user clicks the close button; closing the dialog by other
  // means ends up in closeEvent(); forwards to tryClose()
  //
  void onCloseButton();

  // called when the user clicks the previous button; asks the main window for
  // the previous mod in the list and loads it
  //
  void onPreviousMod();

  // called when the user clicks the next button; asks the main window for the
  // next mod in the list and loads it
  //
  void onNextMod();

  // called when the selects a tab; handles first activation
  //
  void onTabSelectionChanged();

  // called when the user re-orders tabs; sets the correct TabInfo::realPos for
  // all tabs
  //
  void onTabMoved();

  // called when a tab has modified the origin; emits originModified() and
  // updates all the tabs that use origin files
  //
  void onOriginModified(int originID);
};

#endif  // MODINFODIALOG_H
