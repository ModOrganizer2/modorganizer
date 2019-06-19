#ifndef MODINFODIALOGCONFLICTS_H
#define MODINFODIALOGCONFLICTS_H

#include "modinfodialog.h"
#include "expanderwidget.h"

class GeneralConflictsTab
{
public:
  GeneralConflictsTab(Ui::ModInfoDialog* ui, OrganizerCore& oc);

  void saveState(Settings& s);
  void restoreState(const Settings& s);

  void rebuild(ModInfo::Ptr mod, MOShared::FilesOrigin* origin);
  void update();

private:
  struct Expanders
  {
    ExpanderWidget overwrite, overwritten, nonconflict;
  };

  Ui::ModInfoDialog* ui;
  OrganizerCore& m_core;
  ModInfo::Ptr m_mod;
  MOShared::FilesOrigin* m_origin;
  Expanders m_expanders;

  QTreeWidgetItem* createOverwriteItem(
    MOShared::FileEntry::Index index, bool archive,
    const QString& fileName, const QString& relativeName,
    const MOShared::FileEntry::AlternativesVector& alternatives);

  QTreeWidgetItem* createNoConflictItem(
    MOShared::FileEntry::Index index, bool archive,
    const QString& fileName, const QString& relativeName);

  QTreeWidgetItem* createOverwrittenItem(
    MOShared::FileEntry::Index index, int fileOrigin, bool archive,
    const QString& fileName, const QString& relativeName);

  void onOverwriteActivated(QTreeWidgetItem* item, int);
};


class AdvancedConflictsTab
{
public:
  AdvancedConflictsTab(Ui::ModInfoDialog* ui, OrganizerCore& oc);

  void saveState(Settings& s);
  void restoreState(const Settings& s);

  void rebuild(ModInfo::Ptr mod, MOShared::FilesOrigin* origin);
  void update();

private:
  Ui::ModInfoDialog* ui;
  OrganizerCore& m_core;
  ModInfo::Ptr m_mod;
  MOShared::FilesOrigin* m_origin;
  FilterWidget m_filter;

  QTreeWidgetItem* createItem(
    MOShared::FileEntry::Index index, int fileOrigin, bool archive,
    const QString& fileName, const QString& relativeName,
    const MOShared::FileEntry::AlternativesVector& alternatives);
};


class ConflictsTab : public ModInfoDialogTab
{
public:
  ConflictsTab(Ui::ModInfoDialog* ui, OrganizerCore& oc);

  void saveState(Settings& s) override;
  void restoreState(const Settings& s) override;

private:
  Ui::ModInfoDialog* ui;
  GeneralConflictsTab m_general;
  AdvancedConflictsTab m_advanced;
};

#endif // MODINFODIALOGCONFLICTS_H
