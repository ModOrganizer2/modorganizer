#ifndef MODINFODIALOGCONFLICTS_H
#define MODINFODIALOGCONFLICTS_H

#include "expanderwidget.h"
#include "filterwidget.h"
#include "modinfodialogtab.h"
#include "shared/fileregisterfwd.h"
#include <QTreeWidget>
#include <optional>

using namespace MOBase;

class ConflictsTab;
class OrganizerCore;
class ConflictItem;
class ConflictListModel;

class GeneralConflictsTab : public QObject
{
  Q_OBJECT;

public:
  GeneralConflictsTab(ConflictsTab* tab, Ui::ModInfoDialog* ui, OrganizerCore& oc);

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

  FilterWidget m_filterOverwrite;
  FilterWidget m_filterOverwritten;
  FilterWidget m_filterNoConflicts;

  struct GeneralConflictNumbers
  {
    int numTotalFiles            = 0;
    int numTotalLoose            = 0;
    int numTotalArchive          = 0;
    int numNonConflicting        = 0;
    int numNonConflictingLoose   = 0;
    int numNonConflictingArchive = 0;
    int numOverwrite             = 0;
    int numOverwriteLoose        = 0;
    int numOverwriteArchive      = 0;
    int numOverwritten           = 0;
    int numOverwrittenLoose      = 0;
    int numOverwrittenArchive    = 0;

    void clear() { *this = {}; };
  };

  GeneralConflictNumbers m_counts;

  ConflictItem createOverwriteItem(MOShared::FileIndex index, bool archive,
                                   QString fileName, QString relativeName,
                                   const MOShared::AlternativesVector& alternatives);

  ConflictItem createNoConflictItem(MOShared::FileIndex index, bool archive,
                                    QString fileName, QString relativeName);

  ConflictItem createOverwrittenItem(MOShared::FileIndex index, int fileOrigin,
                                     bool archive, QString fileName,
                                     QString relativeName);

  void updateUICounters();

  void onOverwriteActivated(const QModelIndex& index);
  void onOverwrittenActivated(const QModelIndex& index);
};

class AdvancedConflictsTab : public QObject
{
  Q_OBJECT;

public:
  AdvancedConflictsTab(ConflictsTab* tab, Ui::ModInfoDialog* ui, OrganizerCore& oc);

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

  std::optional<ConflictItem>
  createItem(MOShared::FileIndex index, int fileOrigin, bool archive, QString fileName,
             QString relativeName, const MOShared::AlternativesVector& alternatives);
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

  void activateItems(QTreeView* tree);
  void openItems(QTreeView* tree, bool hooked);
  void previewItems(QTreeView* tree);
  void exploreItems(QTreeView* tree);

  void openItem(const ConflictItem* item, bool hooked);
  void previewItem(const ConflictItem* item);
  void changeItemsVisibility(QTreeView* tree, bool visible);

  void showContextMenu(const QPoint& pos, QTreeView* tree);

private:
  struct Actions
  {
    QAction* hide      = nullptr;
    QAction* unhide    = nullptr;
    QAction* open      = nullptr;
    QAction* runHooked = nullptr;
    QAction* preview   = nullptr;
    QAction* explore   = nullptr;
    QMenu* gotoMenu    = nullptr;

    std::vector<QAction*> gotoActions;
  };

  GeneralConflictsTab m_general;
  AdvancedConflictsTab m_advanced;

  Actions createMenuActions(QTreeView* tree);
  std::vector<QAction*> createGotoActions(const ConflictItem* item);
};

#endif  // MODINFODIALOGCONFLICTS_H
