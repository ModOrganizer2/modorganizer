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

public: // TODO: Move this to private when all is done

  // create the "Send to... " context menu
  //
  QMenu* createSendToContextMenu();

  // add actions/menus to this menu for each type of mod
  //
  void addOverwriteActions();
  void addSeparatorActions();
  void addForeignActions();
  void addBackupActions();
  void addRegularActions();

  OrganizerCore& m_core;
  QModelIndexList m_index;
  ModListView* m_view;

};

#endif
