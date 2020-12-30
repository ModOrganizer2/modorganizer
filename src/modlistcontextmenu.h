#ifndef MODLISTCONTEXTMENU_H
#define MODLISTCONTEXTMENU_H

#include <vector>

#include <QMenu>
#include <QModelIndex>
#include <QTreeView>

#include "modinfo.h"

class ModListView;
class ModListViewActions;
class OrganizerCore;

class ModListGlobalContextMenu : public QMenu
{
  Q_OBJECT
public:

  ModListGlobalContextMenu(OrganizerCore& core, ModListView* modListView, QWidget* parent = nullptr);

};

class ModListContextMenu : public QMenu
{
  Q_OBJECT

public:

  // creates a new context menu, the given index is the one for the click and should be valid
  //
  ModListContextMenu(OrganizerCore& core, const QModelIndex& index, ModListView* modListView);

public: // TODO: Move this to private when all is done

  // create the "Send to... " context menu
  //
  QMenu* createSendToContextMenu();

  // add actions/menus to this menu for each type of mod
  //
  void addOverwriteActions(ModInfo::Ptr mod);
  void addSeparatorActions(ModInfo::Ptr mod);
  void addForeignActions(ModInfo::Ptr mod);
  void addBackupActions(ModInfo::Ptr mod);
  void addRegularActions(ModInfo::Ptr mod);

  OrganizerCore& m_core;
  QModelIndex m_index;
  QModelIndexList m_selected;
  ModListView* m_view;
  ModListViewActions& m_actions; // shortcut for m_view->actions()

};

#endif
