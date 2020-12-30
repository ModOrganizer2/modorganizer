#ifndef MODLISTCONTEXTMENU_H
#define MODLISTCONTEXTMENU_H

#include <vector>

#include <QMenu>
#include <QModelIndex>
#include <QTreeView>

class ModListView;
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

private:

  // add actions/menus to this menu for each type of mod
  //
  void addOverwriteActions(OrganizerCore& core, ModListView* modListView);
  void addSeparatorActions(OrganizerCore& core, ModListView* modListView);
  void addForeignActions(OrganizerCore& core, ModListView* modListView);
  void addBackupActions(OrganizerCore& core, ModListView* modListView);
  void addRegularActions(OrganizerCore& core, ModListView* modListView);

  OrganizerCore& m_core;
  QModelIndexList m_index;

};

#endif
