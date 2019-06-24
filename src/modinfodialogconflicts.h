#ifndef MODINFODIALOGCONFLICTS_H
#define MODINFODIALOGCONFLICTS_H

#include "modinfodialogtab.h"
#include "expanderwidget.h"
#include "filterwidget.h"
#include "directoryentry.h"
#include <QTreeWidget>

class ConflictsTab;
class OrganizerCore;

class GeneralConflictsTab : public QObject
{
  Q_OBJECT;

public:
  GeneralConflictsTab(
    ConflictsTab* tab, Ui::ModInfoDialog* ui, OrganizerCore& oc);

  void clear();
  void saveState(Settings& s);
  void restoreState(const Settings& s);

  bool update();

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

  void update();

signals:
  void modOpen(QString name);

private:
  ConflictsTab* m_tab;
  Ui::ModInfoDialog* ui;
  OrganizerCore& m_core;
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
    OrganizerCore& oc, PluginContainer& plugin,
    QWidget* parent, Ui::ModInfoDialog* ui, int index);

  void update() override;
  void clear() override;
  void saveState(Settings& s) override;
  void restoreState(const Settings& s) override;
  bool canHandleUnmanaged() const override;

  void openItems(const QList<QTreeWidgetItem*>& items);
  void previewItems(const QList<QTreeWidgetItem*>& items);

  void changeItemsVisibility(
    const QList<QTreeWidgetItem*>& items, bool visible);

  void showContextMenu(const QPoint &pos, QTreeWidget* tree);

private:
  struct Actions
  {
    QAction* hide = nullptr;
    QAction* unhide = nullptr;
    QAction* open = nullptr;
    QAction* preview = nullptr;
    QMenu* gotoMenu = nullptr;
    std::vector<QAction*> gotoActions;
  };

  GeneralConflictsTab m_general;
  AdvancedConflictsTab m_advanced;

  Actions createMenuActions(const QList<QTreeWidgetItem*>& selection);

  std::vector<QAction*> createGotoActions(
    const QList<QTreeWidgetItem*>& selection);
};

#endif // MODINFODIALOGCONFLICTS_H
