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
class OrganizerCore;

class ModListViewActions : public QObject
{
  Q_OBJECT

public:

  // currently passing the main window itself because a lots of stuff needs it but
  // it would be nice to avoid passing it at some point
  //
  ModListViewActions(
    OrganizerCore& core,
    FilterList& filters,
    CategoryFactory& categoryFactory,
    MainWindow* mainWindow,
    ModListView* view);

  // install the mod from the given archive
  //
  void installMod(const QString& archivePath = "") const;

  // create an empty mod/a separator before the given mod or at
  // the end of the list if the index is -1
  //
  void createEmptyMod(int modIndex) const;
  void createSeparator(int modIndex) const;

  // check all mods for update
  //
  void checkModsForUpdates() const;

  // start the "Export Mod List" dialog
  //
  void exportModListCSV() const;

  // display mod information
  //
  void displayModInformation(const QString& modName, ModInfoTabIDs tabID = ModInfoTabIDs::None) const;
  void displayModInformation(unsigned int index, ModInfoTabIDs tab = ModInfoTabIDs::None) const;
  void displayModInformation(ModInfo::Ptr modInfo, unsigned int modIndex, ModInfoTabIDs tabID = ModInfoTabIDs::None) const;

  // move mods to top/bottom, start the "Send to priority" and "Send to separator" dialog
  //
  void sendModsToTop(const QModelIndexList& index) const;
  void sendModsToBottom(const QModelIndexList& index) const;
  void sendModsToPriority(const QModelIndexList& index) const;
  void sendModsToSeparator(const QModelIndexList& index) const;

private:

  OrganizerCore& m_core;
  FilterList& m_filters;
  CategoryFactory& m_categories;
  MainWindow* m_main;
  ModListView* m_view;
};

#endif
