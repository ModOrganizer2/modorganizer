#ifndef MODLISTVIEW_H
#define MODLISTVIEW_H

#include <vector>

#include <QTreeView>
#include <QDragEnterEvent>
#include <QLCDNumber>

#include "qtgroupingproxy.h"
#include "viewmarkingscrollbar.h"
#include "modlistsortproxy.h"

namespace Ui { class MainWindow; }

class FilterList;
class OrganizerCore;
class Profile;
class ModListByPriorityProxy;
class ModListViewActions;

class ModListView : public QTreeView
{
  Q_OBJECT

public:

  // this is a public version of DropIndicatorPosition
  enum DropPosition {
    OnItem = DropIndicatorPosition::OnItem,
    AboveItem = DropIndicatorPosition::AboveItem,
    BelowItem = DropIndicatorPosition::BelowItem,
    OnViewport = DropIndicatorPosition::OnViewport
  };

public:
  explicit ModListView(QWidget* parent = 0);
  void setModel(QAbstractItemModel* model) override;

  void setup(OrganizerCore& core, ModListViewActions* actions, Ui::MainWindow* mwui);

  // set the current profile
  //
  void setProfile(Profile* profile);

  // check if collapsible separators are currently used
  //
  bool hasCollapsibleSeparators() const;

  // the column by which the mod list is currently sorted
  //
  int sortColumn() const;

  // retrieve the actions from the view
  //
  ModListViewActions& actions() const;

  // retrieve the next/previous mod in the current view, the given index
  // should be a mod index (not a model row), and the return value will be
  // a mod index or -1 if no mod was found
  //
  int nextMod(int index) const;
  int prevMod(int index) const;

  // check if the given mod is visible
  //
  bool isModVisible(unsigned int index) const;
  bool isModVisible(ModInfo::Ptr mod) const;

  // re-implemented to fix indentation with collapsible separators
  //
  QRect visualRect(const QModelIndex& index) const override;

  // refresh the view (to call when settings have been changed)
  //
  void refresh();

signals:

  void dragEntered(const QMimeData* mimeData);
  void dropEntered(const QMimeData* mimeData, DropPosition position);

  // emitted when selected mods must be removed
  //
  void removeSelectedMods();

public slots:

  // enable/disable all visible mods
  //
  void enableAllVisible();
  void disableAllVisible();

  // set the filter criteria/options for mods
  //
  void setFilterCriteria(const std::vector<ModListSortProxy::Criteria>& criteria);
  void setFilterOptions(ModListSortProxy::FilterMode mode, ModListSortProxy::SeparatorsMode sep);

  // update the mod counter
  //
  void updateModCount();

  // map from/to the view indexes to the model
  //
  QModelIndex indexModelToView(const QModelIndex& index) const;
  QModelIndexList indexModelToView(const QModelIndexList& index) const;
  QModelIndex indexViewToModel(const QModelIndex& index) const;
  QModelIndexList indexViewToModel(const QModelIndexList& index) const;

protected:

  // returns the next/previous index of the given index
  //
  QModelIndex nextIndex(const QModelIndex& index) const;
  QModelIndex prevIndex(const QModelIndex& index) const;

  // all index for the given model under the given index, recursively
  //
  QModelIndexList allIndex(
    const QAbstractItemModel* model, int column = 0, const QModelIndex& index = QModelIndex()) const;

  // re-implemented to fake the return value to allow drag-and-drop on
  // itself for separators
  //
  QModelIndexList selectedIndexes() const;

  // drop from external folder
  //
  void onExternalFolderDropped(const QUrl& url, int priority);

  // method to react to various key events
  //
  bool moveSelection(int key);
  bool removeSelection();
  bool toggleSelectionState();

  void timerEvent(QTimerEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dropEvent(QDropEvent* event) override;
  bool event(QEvent* event) override;

protected slots:

private:

  void onModPrioritiesChanged(std::vector<int> const& indices);
  void onModInstalled(const QString& modName);
  void onModFilterActive(bool filterActive);

  // call expand() after fixing the index if it comes from the source
  // of the proxy
  //
  void expandItem(const QModelIndex& index);

  // refresh the group-by proxy, if the index is -1 will refresh the
  // current one (e.g. when changing the sort column)
  //
  void updateGroupByProxy(int groupIndex);

  enum GroupBy {
    NONE = 0,
    CATEGORY = 1,
    NEXUS_ID = 2
  };

private:

  struct ModListViewUi
  {
    // the group by combo box
    QComboBox* groupBy;

    // the mod counter
    QLCDNumber* counter;

    // the text filter and clear filter button
    QLineEdit* filter;
    QPushButton* clearFilters;
  };

  OrganizerCore* m_core;
  ModListViewUi ui;
  ModListViewActions* m_actions;

  ModListSortProxy* m_sortProxy;
  ModListByPriorityProxy* m_byPriorityProxy;

  QtGroupingProxy* m_byCategoryProxy;
  QtGroupingProxy* m_byNexusIdProxy;

  ViewMarkingScrollBar* m_scrollbar;

  bool m_inDragMoveEvent = false;

  // replace the auto-expand timer from QTreeView to avoid
  // auto-collapsing
  QBasicTimer m_openTimer;

};

#endif // MODLISTVIEW_H
