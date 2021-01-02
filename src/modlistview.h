#ifndef MODLISTVIEW_H
#define MODLISTVIEW_H

#include <vector>

#include <QLabel>
#include <QTreeView>
#include <QDragEnterEvent>
#include <QLCDNumber>

#include "qtgroupingproxy.h"
#include "viewmarkingscrollbar.h"
#include "modlistsortproxy.h"

namespace Ui { class MainWindow; }

class CategoryFactory;
class FilterList;
class OrganizerCore;
class MainWindow;
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

  // indiucate the groupby mode
  enum class GroupByMode {
    NONE,
    SEPARATOR,
    CATEGORY,
    NEXUS_ID
  };

public:
  explicit ModListView(QWidget* parent = 0);

  void setup(OrganizerCore& core, CategoryFactory& factory, MainWindow* mw, Ui::MainWindow* mwui);

  // restore/save the state between session
  //
  void restoreState(const Settings& s);
  void saveState(Settings& s) const;

  // set the current profile
  //
  void setProfile(Profile* profile);

  // check if collapsible separators are currently used
  //
  bool hasCollapsibleSeparators() const;

  // the column by which the mod list is currently sorted
  //
  int sortColumn() const;

  // the current group mode
  //
  GroupByMode groupByMode() const;

  // retrieve the actions from the view
  //
  ModListViewActions& actions() const;

  // retrieve the next/previous mod in the current view, the given index
  // should be a mod index (not a model row)
  //
  std::optional<unsigned int> nextMod(unsigned int index) const;
  std::optional<unsigned int> prevMod(unsigned int index) const;

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

  // refresh the filters
  //
  void refreshFilters();

protected:

  friend class ModListContextMenu;
  friend class ModListViewActions;

  // map from/to the view indexes to the model
  //
  QModelIndex indexModelToView(const QModelIndex& index) const;
  QModelIndexList indexModelToView(const QModelIndexList& index) const;
  QModelIndex indexViewToModel(const QModelIndex& index) const;
  QModelIndexList indexViewToModel(const QModelIndexList& index) const;

  // returns the next/previous index of the given index
  //
  QModelIndex nextIndex(const QModelIndex& index) const;
  QModelIndex prevIndex(const QModelIndex& index) const;

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

  void onCustomContextMenuRequested(const QPoint& pos);
  void onDoubleClicked(const QModelIndex& index);
  void onSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
  void onFiltersCriteria(const std::vector<ModListSortProxy::Criteria>& filters);

private:

  void onModPrioritiesChanged(const QModelIndexList& indices);
  void onModInstalled(const QString& modName);
  void onModFilterActive(bool filterActive);

  // get/set the selected items on the view, this method return/take indices
  // from the mod list model, not the view, so it's safe to restore
  //
  std::pair<QModelIndex, QModelIndexList> selected() const;
  void setSelected(const QModelIndex& current, const QModelIndexList& selected);

  // call expand() after fixing the index if it comes from the source
  // of the proxy
  //
  void expandItem(const QModelIndex& index);

  // refresh the group-by proxy, if the index is -1 will refresh the
  // current one (e.g. when changing the sort column)
  //
  void updateGroupByProxy(int groupIndex);

  // index in the groupby combo
  //
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

    // filters related
    QLineEdit* filter;
    QLabel* currentCategory;
    QPushButton* clearFilters;
    QComboBox* filterSeparators;
  };

  OrganizerCore* m_core;
  std::unique_ptr<FilterList> m_filters;
  CategoryFactory* m_categories;
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
