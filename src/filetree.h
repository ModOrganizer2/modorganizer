#ifndef MODORGANIZER_FILETREE_INCLUDED
#define MODORGANIZER_FILETREE_INCLUDED

#include "modinfo.h"
#include "modinfodialogfwd.h"

namespace MOShared { class FileEntry; }

class OrganizerCore;
class PluginContainer;
class FileTreeModel;
class FileTreeItem;

class FileTree : public QObject
{
  Q_OBJECT;

public:
  FileTree(OrganizerCore& core, PluginContainer& pc, QTreeView* tree);

  FileTreeModel* model();
  void refresh();
  void clear();

  void open();
  void openHooked();
  void preview();

  void addAsExecutable();
  void exploreOrigin();
  void openModInfo();

  void toggleVisibility();
  void hide();
  void unhide();

  void dumpToFile() const;

signals:
  void executablesChanged();
  void originModified(int originID);
  void displayModInformation(ModInfo::Ptr m, unsigned int i, ModInfoTabIDs tab);

private:
  OrganizerCore& m_core;
  PluginContainer& m_plugins;
  QTreeView* m_tree;
  FileTreeModel* m_model;

  FileTreeItem* singleSelection();
  void onExpandedChanged(const QModelIndex& index, bool expanded);
  void onContextMenu(const QPoint &pos);
  void showShellMenu(const MOShared::FileEntry& file, QPoint pos);

  void addDirectoryMenus(QMenu& menu, FileTreeItem& item);
  void addFileMenus(QMenu& menu, const MOShared::FileEntry& file, int originID);
  void addOpenMenus(QMenu& menu, const MOShared::FileEntry& file);
  void addCommonMenus(QMenu& menu);

  void toggleVisibility(bool b);

  void dumpToFile(
    QFile& out, const QString& parentPath,
    const MOShared::DirectoryEntry& entry) const;
};

#endif // MODORGANIZER_FILETREE_INCLUDED
