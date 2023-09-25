#ifndef MODLISTCONTEXTMENU_H
#define MODLISTCONTEXTMENU_H

#include <vector>

#include <QMenu>
#include <QModelIndex>
#include <QTreeView>

#include "modinfo.h"

class CategoryFactory;
class ModListView;
class ModListViewActions;
class OrganizerCore;

class ModListGlobalContextMenu : public QMenu
{
  Q_OBJECT
public:
  ModListGlobalContextMenu(OrganizerCore& core, ModListView* view,
                           QWidget* parent = nullptr);

protected:
  friend class ModListContextMenu;

  // populate the menu
  void populate(OrganizerCore& core, ModListView* view, const QModelIndex& index);

  // creates a "All mods" context menu for the given index (can be invalid).
  ModListGlobalContextMenu(OrganizerCore& core, ModListView* view,
                           const QModelIndex& index, QWidget* parent = nullptr);
};

class ModListChangeCategoryMenu : public QMenu
{
  Q_OBJECT
public:
  ModListChangeCategoryMenu(CategoryFactory* categories, ModInfo::Ptr mod,
                            QMenu* parent = nullptr);

  // return a list of pair <category id, enabled> from the menu
  //
  std::vector<std::pair<int, bool>> categories() const;

private:
  // populate the tree with the category, using the enabled/disabled state from the
  // given mod
  //
  bool populate(QMenu* menu, CategoryFactory* categories, ModInfo::Ptr mod,
                int targetId = 0);

  // internal implementation of categories() for recursion
  //
  std::vector<std::pair<int, bool>> categories(const QMenu* menu) const;
};

class ModListPrimaryCategoryMenu : public QMenu
{
  Q_OBJECT
public:
  ModListPrimaryCategoryMenu(CategoryFactory* categories, ModInfo::Ptr mod,
                             QMenu* parent = nullptr);

  // return the selected primary category
  //
  int primaryCategory() const;

private:
  // populate the categories
  //
  void populate(const CategoryFactory* categories, ModInfo::Ptr mod);
};

class ModListContextMenu : public QMenu
{
  Q_OBJECT

public:
  // creates a new context menu, the given index is the one for the click and should be
  // valid
  //
  ModListContextMenu(const QModelIndex& index, OrganizerCore& core,
                     CategoryFactory* categories, ModListView* modListView);

private:
  // adds the "Send to... " context menu
  //
  void addSendToContextMenu();

  // adds the categories menu (change/primary)
  //
  void addCategoryContextMenus(ModInfo::Ptr mod);

  // special menu for categories
  //
  void addMenuAsPushButton(QMenu* menu);

  // add actions/menus to this menu for each type of mod
  //
  void addOverwriteActions(ModInfo::Ptr mod);
  void addSeparatorActions(ModInfo::Ptr mod);
  void addForeignActions(ModInfo::Ptr mod);
  void addBackupActions(ModInfo::Ptr mod);
  void addRegularActions(ModInfo::Ptr mod);

  OrganizerCore& m_core;
  CategoryFactory* m_categories;
  QModelIndex m_index;
  QModelIndexList m_selected;
  ModListView* m_view;
  ModListViewActions& m_actions;  // shortcut for m_view->actions()
};

#endif
