#ifndef MODLISTVIEWACTIONS_H
#define MODLISTVIEWACTIONS_H

#include <QObject>
#include <QString>

class CategoryFactory;
class FilterList;
class ModListView;
class OrganizerCore;

class ModListViewActions : public QObject
{
  Q_OBJECT

public:

  // the nxmReceiver is a (hopefully temporary) "hack" because it would require a lots of change
  // to do otherwise since NXM is mostly based on the old Qt signal-slot system
  //
  ModListViewActions(
    OrganizerCore& core,
    FilterList& filters,
    CategoryFactory& categoryFactory,
    QObject* nxmReceiver,
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

private:

  OrganizerCore& m_core;
  FilterList& m_filters;
  CategoryFactory& m_categories;
  QObject* m_receiver;  // receiver for NXM signals (temporary)
  ModListView* m_view;
};

#endif
