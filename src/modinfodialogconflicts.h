#ifndef MODINFODIALOGCONFLICTS_H
#define MODINFODIALOGCONFLICTS_H

#include "modinfodialog.h"
#include "expanderwidget.h"

class ConflictsTab;

class GeneralConflictsTab : public QObject
{
  Q_OBJECT;

public:
  GeneralConflictsTab(
    ConflictsTab* tab, Ui::ModInfoDialog* ui, OrganizerCore& oc);

  void clear();
  void saveState(Settings& s);
  void restoreState(const Settings& s);

  void setMod(ModInfo::Ptr mod, MOShared::FilesOrigin* origin);
  void update();

signals:
  void modOpen(QString name);

private:
  struct Expanders
  {
    ExpanderWidget overwrite, overwritten, nonconflict;
  };

  ConflictsTab* m_tab;
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
  void onOverwrittenActivated(QTreeWidgetItem *item, int);

  void onOverwriteTreeContext(const QPoint &pos);
  void onOverwrittenTreeContext(const QPoint &pos);
  void onNoConflictTreeContext(const QPoint &pos);
};


class AdvancedConflictsTab : public QObject
{
  Q_OBJECT;

public:
  AdvancedConflictsTab(
    ConflictsTab* tab, Ui::ModInfoDialog* ui, OrganizerCore& oc);

  void clear();
  void saveState(Settings& s);
  void restoreState(const Settings& s);

  void setMod(ModInfo::Ptr mod, MOShared::FilesOrigin* origin);
  void update();

signals:
  void modOpen(QString name);

private:
  ConflictsTab* m_tab;
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
  Q_OBJECT;

public:
  ConflictsTab(
    QWidget* parent, Ui::ModInfoDialog* ui,
    OrganizerCore& oc, PluginContainer& plugin);

  void setMod(ModInfo::Ptr mod, MOShared::FilesOrigin* origin) override;
  void update() override;

  void clear() override;
  bool feedFile(const QString& rootPath, const QString& filename) override;
  void saveState(Settings& s) override;
  void restoreState(const Settings& s) override;

  void openItems(const QList<QTreeWidgetItem*>& items);
  void previewItems(const QList<QTreeWidgetItem*>& items);

  void changeItemsVisibility(
    const QList<QTreeWidgetItem*>& items, bool visible);

  void showContextMenu(const QPoint &pos, QTreeWidget* tree);

private:
  struct Actions
  {
    QAction* hide;
    QAction* unhide;
    QAction* open;
    QAction* preview;
    QMenu* gotoMenu;
    std::vector<QAction*> gotoActions;

    Actions() :
      hide(nullptr), unhide(nullptr), open(nullptr), preview(nullptr),
      gotoMenu(nullptr)
    {
    }
  };

  QWidget* m_parent;
  Ui::ModInfoDialog* ui;
  OrganizerCore& m_core;
  PluginContainer& m_plugin;
  ModInfo::Ptr m_mod;
  MOShared::FilesOrigin* m_origin;
  GeneralConflictsTab m_general;
  AdvancedConflictsTab m_advanced;

  Actions createMenuActions(const QList<QTreeWidgetItem*>& selection);

  std::vector<QAction*> createGotoActions(
    const QList<QTreeWidgetItem*>& selection);
};

#endif // MODINFODIALOGCONFLICTS_H
