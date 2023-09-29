#ifndef MODLISTVIEWACTIONS_H
#define MODLISTVIEWACTIONS_H

#include <QObject>
#include <QString>

#include "modinfo.h"
#include "modinfodialogfwd.h"

class CategoryFactory;
class FilterList;
class MainWindow;
class ModListView;
class PluginListView;
class OrganizerCore;
class DownloadManager;

class ModListViewActions : public QObject
{
  Q_OBJECT

public:
  // currently passing the main window itself because a lots of stuff needs it but
  // it would be nice to avoid passing it at some point
  //
  ModListViewActions(OrganizerCore& core, FilterList& filters,
                     CategoryFactory& categoryFactory, ModListView* view,
                     PluginListView* pluginView, QObject* nxmReceiver);

  // install mod, create an empty mod or a separator with at priority based on the
  // index and the current sorting of the view
  // - if the list is not sorted by priority or the index is invalid, use the
  //   highest priority
  // - otherwise
  //   - create separators above the given index (the priority is not the
  //     same depending on the current sorting)
  //   - create/install mods above or below other mods depending on the sort order, or
  //     at the end of the separator if index corresponds to the separator
  //
  // if the archivePath is empty for installMod, ask the user for the archive
  // to install
  //
  void installMod(const QString& archivePath = "",
                  const QModelIndex& index   = QModelIndex()) const;
  void createEmptyMod(const QModelIndex& index = QModelIndex()) const;
  void createSeparator(const QModelIndex& index = QModelIndex()) const;

  // enable/disable all non-filtered mods
  //
  void setAllMatchingModsEnabled(bool enabled) const;

  // check all mods for update
  //
  void checkModsForUpdates() const;
  void checkModsForUpdates(const QModelIndexList& indices) const;

  // auto-assign categories based on nexus ID
  //
  void assignCategories() const;

  // start the "Export Mod List" dialog
  //
  void exportModListCSV() const;

  // display mod information
  //
  void displayModInformation(const QString& modName,
                             ModInfoTabIDs tabID = ModInfoTabIDs::None) const;
  void displayModInformation(unsigned int index,
                             ModInfoTabIDs tab = ModInfoTabIDs::None) const;
  void displayModInformation(ModInfo::Ptr modInfo, unsigned int modIndex,
                             ModInfoTabIDs tabID = ModInfoTabIDs::None) const;

  // move mods to top/bottom, start the "Send to priority" and "Send to separator"
  // dialog
  //
  void sendModsToTop(const QModelIndexList& index) const;
  void sendModsToBottom(const QModelIndexList& index) const;
  void sendModsToPriority(const QModelIndexList& index) const;
  void sendModsToSeparator(const QModelIndexList& index) const;
  void sendModsToFirstConflict(const QModelIndexList& index) const;
  void sendModsToLastConflict(const QModelIndexList& index) const;

  // actions for most regular mods
  //
  void renameMod(const QModelIndex& index) const;
  void removeMods(const QModelIndexList& indices) const;
  void ignoreMissingData(const QModelIndexList& indices) const;
  void setIgnoreUpdate(const QModelIndexList& indices, bool ignore) const;
  void changeVersioningScheme(const QModelIndex& indices) const;
  void markConverted(const QModelIndexList& indices) const;
  void visitOnNexus(const QModelIndexList& indices) const;
  void visitWebPage(const QModelIndexList& indices) const;
  void visitNexusOrWebPage(const QModelIndexList& indices) const;
  void reinstallMod(const QModelIndex& index) const;
  void createBackup(const QModelIndex& index) const;
  void restoreHiddenFiles(const QModelIndexList& indices) const;
  void setTracked(const QModelIndexList& indices, bool tracked) const;
  void setEndorsed(const QModelIndexList& indices, bool endorsed) const;
  void willNotEndorsed(const QModelIndexList& indices) const;
  void remapCategory(const QModelIndexList& indices) const;

  // set/reset color of the given selection, using the given reference index (index
  // at which the context menu was shown)
  //
  void setColor(const QModelIndexList& indices, const QModelIndex& refIndex) const;
  void resetColor(const QModelIndexList& indices) const;

  // set the category of the mods in the given list, using the given index as reference
  // - the categories are set as-is on the refernce mod
  // - for the other mods, the category is only set if the current state of the category
  //   on the reference is different
  //
  void setCategories(const QModelIndexList& selected, const QModelIndex& ref,
                     const std::vector<std::pair<int, bool>>& categories) const;

  // set the primary category of the mods in the given list, adding the appropriate
  // category to the mods unless force is false, in which case the primary category
  // is set only on mods that have this category
  //
  void setPrimaryCategory(const QModelIndexList& selected, int category,
                          bool force = true);

  // open the Windows explorer for the specified mods
  //
  void openExplorer(const QModelIndexList& index) const;

  // backup-specific actions
  //
  void restoreBackup(const QModelIndex& index) const;

  // overwrite-specific actions
  //
  void createModFromOverwrite() const;
  void moveOverwriteContentToExistingMod() const;
  void clearOverwrite() const;

signals:

  // emitted when the overwrite mod has been clear
  //
  void overwriteCleared() const;

  // emitted when the origin of a file is modified
  //
  void originModified(int originId) const;

  // emitted when the mod info dialog has been shown and closed
  //
  void modInfoDisplayed() const;

private:
  // find the priority where to install or create a mod for the
  // given index (e.g. above, below, inside separator or at the end)
  //
  int findInstallPriority(const QModelIndex& current) const;

  // move the contents of the overwrite to the given path
  //
  void moveOverwriteContentsTo(const QString& absolutePath) const;

  // set the category of the given mod based on the given array
  //
  void setCategories(ModInfo::Ptr mod,
                     const std::vector<std::pair<int, bool>>& categories) const;

  // set the category of the given mod if the category from the reference mod does not
  // match the one in the array of categories
  //
  void setCategoriesIf(ModInfo::Ptr mod, ModInfo::Ptr ref,
                       const std::vector<std::pair<int, bool>>& categories) const;

  // check the given mods from update, the map should map game names to nexus ID
  //
  void checkModsForUpdates(std::multimap<QString, int> const& IDs) const;

private:
  OrganizerCore& m_core;
  FilterList& m_filters;
  CategoryFactory& m_categories;
  ModListView* m_view;
  PluginListView* m_pluginView;
  QObject* m_receiver;
  QWidget* m_parent;
};

#endif
