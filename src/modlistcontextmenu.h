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

private:

  friend class ModListView;

  // creates a new context menu that will act on the given mod list
  // index (those should be index from the modlist)
  ModListContextMenu(OrganizerCore& core, const QModelIndexList& index, ModListView* modListView);

  OrganizerCore& m_core;
  QModelIndexList m_index;

};

#endif
