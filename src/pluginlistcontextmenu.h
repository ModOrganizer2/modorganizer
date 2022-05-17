#ifndef PLUGINLISTCONTEXTMENU_H
#define PLUGINLISTCONTEXTMENU_H

#include <QMenu>
#include <QModelIndex>
#include <QTreeView>

#include "modinfo.h"

class PluginListView;
class OrganizerCore;

class PluginListContextMenu : public QMenu
{
  Q_OBJECT

public:
  // creates a new context menu, the given index is the one for the click and should be
  // valid
  //
  PluginListContextMenu(const QModelIndex& index, OrganizerCore& core,
                        PluginListView* modListView);

signals:

  // emitted to open a mod information
  //
  void openModInformation(unsigned int modIndex);

public:
  // create the "Send to... " context menu
  //
  QMenu* createSendToContextMenu();
  void sendPluginsToPriority(const QModelIndexList& indices);

  // set ESP lock on the given plugins
  //
  void setESPLock(const QModelIndexList& indices, bool locked);

  // open explorer or mod information for the origin of the plugins
  //
  void openOriginExplorer(const QModelIndexList& indices);
  void openOriginInformation(const QModelIndex& index);

  OrganizerCore& m_core;
  QModelIndex m_index;
  QModelIndexList m_selected;
  PluginListView* m_view;
};

#endif
