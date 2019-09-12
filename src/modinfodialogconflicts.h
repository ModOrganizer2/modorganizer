#ifndef MODINFODIALOGCONFLICTS_H
#define MODINFODIALOGCONFLICTS_H

#include "modinfodialogtab.h"
#include "expanderwidget.h"
#include "filterwidget.h"
#include "directoryentry.h"
#include <QTreeWidget>
#include <optional>

class ConflictsTab;
class OrganizerCore;
class ConflictItem;
class ConflictListModel;

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
    MOBase::ExpanderWidget overwrite, overwritten, nonconflict;
  };

  ConflictsTab* m_tab;
  Ui::ModInfoDialog* ui;
  OrganizerCore& m_core;
  Expanders m_expanders;

  ConflictListModel* m_overwriteModel;
  ConflictListModel* m_overwrittenModel;
  ConflictListModel* m_noConflictModel;

  ConflictItem createOverwriteItem(
    MOShared::FileEntry::Index index, bool archive,
    QString fileName, QString relativeName,
    const MOShared::FileEntry::AlternativesVector& alternatives);

  ConflictItem createNoConflictItem(
    MOShared::FileEntry::Index index, bool archive,
    QString fileName, QString relativeName);

  ConflictItem createOverwrittenItem(
    MOShared::FileEntry::Index index, int fileOrigin, bool archive,
    QString fileName, QString relativeName);

  void onOverwriteActivated(const QModelIndex& index);
  void onOverwrittenActivated(const QModelIndex& index);

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
  ConflictListModel* m_model;

  std::optional<ConflictItem> createItem(
    MOShared::FileEntry::Index index, int fileOrigin, bool archive,
    QString fileName, QString relativeName,
    const MOShared::FileEntry::AlternativesVector& alternatives);
};


class ConflictsTab : public ModInfoDialogTab
{
  Q_OBJECT;

public:
  ConflictsTab(ModInfoDialogTabContext cx);

  void update() override;
  void clear() override;
  void saveState(Settings& s) override;
  void restoreState(const Settings& s) override;
  bool canHandleUnmanaged() const override;

  void openItems(QTreeView* tree);
  void previewItems(QTreeView* tree);
  void exploreItems(QTreeView* tree);

  void changeItemsVisibility(QTreeView* tree, bool visible);

  void showContextMenu(const QPoint &pos, QTreeView* tree);

private:
  struct Actions
  {
    QAction* hide = nullptr;
    QAction* unhide = nullptr;
    QAction* open = nullptr;
    QAction* preview = nullptr;
    QAction* explore = nullptr;
    QMenu* gotoMenu = nullptr;

    std::vector<QAction*> gotoActions;
  };

  GeneralConflictsTab m_general;
  AdvancedConflictsTab m_advanced;

  Actions createMenuActions(QTreeView* tree);
  std::vector<QAction*> createGotoActions(const ConflictItem* item);
};

#endif // MODINFODIALOGCONFLICTS_H
