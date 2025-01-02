#ifndef MODLISTVIEW_H
#define MODLISTVIEW_H

#include <map>
#include <set>
#include <vector>

#include <QDragEnterEvent>
#include <QLCDNumber>
#include <QLabel>
#include <QTreeView>

#include "modlistsortproxy.h"
#include "qtgroupingproxy.h"
#include "viewmarkingscrollbar.h"

namespace Ui
{
class MainWindow;
}

class CategoryFactory;
class FilterList;
class OrganizerCore;
class MainWindow;
class Profile;
class ModListByPriorityProxy;
class ModListViewActions;
class PluginListView;

class ModListView : public QTreeView
{
  Q_OBJECT

public:
  // this is a public version of DropIndicatorPosition
  enum DropPosition
  {
    OnItem     = DropIndicatorPosition::OnItem,
    AboveItem  = DropIndicatorPosition::AboveItem,
    BelowItem  = DropIndicatorPosition::BelowItem,
    OnViewport = DropIndicatorPosition::OnViewport
  };

  // indiucate the groupby mode
  enum class GroupByMode
  {
    NONE,
    SEPARATOR,
    CATEGORY,
    NEXUS_ID
  };

public:
  explicit ModListView(QWidget* parent = 0);

  void setup(OrganizerCore& core, CategoryFactory& factory, MainWindow* mw,
             Ui::MainWindow* mwui);

  // restore/save the state between session
  //
  void restoreState(const Settings& s);
  void saveState(Settings& s) const;

  // check if collapsible separators are currently used
  //
  bool hasCollapsibleSeparators() const;

  // the column/order by which the mod list is currently sorted
  //
  int sortColumn() const;
  Qt::SortOrder sortOrder() const;

  // check if a filter is currently active
  //
  bool isFilterActive() const;

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

  // check if the given mod is visible, i.e. not filtered (returns true
  // for collapsed mods)
  //
  bool isModVisible(unsigned int index) const;
  bool isModVisible(ModInfo::Ptr mod) const;

  // focus the view, select the given index and scroll to it
  //
  void scrollToAndSelect(const QModelIndex& index);
  void scrollToAndSelect(const QModelIndexList& indexes,
                         const QModelIndex& current = QModelIndex());

  // refresh the view (to call when settings have been changed)
  //
  void refresh();

signals:

  // emitted for dragEnter events
  //
  void dragEntered(const QMimeData* mimeData);

  // emitted for dropEnter events, the boolean indicates if the drop target
  // is expanded and the position of the indicator
  //
  void dropEntered(const QMimeData* mimeData, bool dropExpanded, DropPosition position);

public slots:

  // invalidate (refresh) the filter (similar to a layout changed event)
  //
  void invalidateFilter();

  // set the filter criteria/options for mods
  //
  void setFilterCriteria(const std::vector<ModListSortProxy::Criteria>& criteria);
  void setFilterOptions(ModListSortProxy::FilterMode mode,
                        ModListSortProxy::SeparatorsMode sep);

  // update the mod counter
  //
  void updateModCount();

  // refresh the filters
  //
  void refreshFilters();

  // set highligth markers
  //
  void setHighlightedMods(const std::set<QString>& modNames);

protected:
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

  // re-implemented to fix indentation with collapsible separators
  //
  QRect visualRect(const QModelIndex& index) const override;
  void drawBranches(QPainter* painter, const QRect& rect,
                    const QModelIndex& index) const override;

  void timerEvent(QTimerEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dropEvent(QDropEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  bool event(QEvent* event) override;

protected slots:

  void onCustomContextMenuRequested(const QPoint& pos);
  void onDoubleClicked(const QModelIndex& index);
  void onFiltersCriteria(const std::vector<ModListSortProxy::Criteria>& filters);
  void onProfileChanged(Profile* oldProfile, Profile* newProfile);

  void commitData(QWidget* editor) override;

private:  // friend classes
  friend class ModConflictIconDelegate;
  friend class ModContentIconDelegate;
  friend class ModFlagIconDelegate;
  friend class ModListContextMenu;
  friend class ModListStyledItemDelegate;
  friend class ModListViewActions;
  friend class ModListViewMarkingScrollBar;

private:  // private structures
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

    // the plugin list (for highlights)
    PluginListView* pluginList;
  };

  struct MarkerInfos
  {
    // conflicts
    std::set<unsigned int> overwrite;
    std::set<unsigned int> overwritten;
    std::set<unsigned int> archiveOverwrite;
    std::set<unsigned int> archiveOverwritten;
    std::set<unsigned int> archiveLooseOverwrite;
    std::set<unsigned int> archiveLooseOverwritten;

    // selected plugins
    std::set<unsigned int> highlight;
  };

  struct ModCounters
  {
    int active    = 0;
    int backup    = 0;
    int foreign   = 0;
    int separator = 0;
    int regular   = 0;

    struct
    {
      int active    = 0;
      int backup    = 0;
      int foreign   = 0;
      int separator = 0;
      int regular   = 0;
    } visible;
  };

  // index in the groupby combo
  //
  enum GroupBy
  {
    NONE     = 0,
    CATEGORY = 1,
    NEXUS_ID = 2
  };

private:  // private functions
  void onModPrioritiesChanged(const QModelIndexList& indices);
  void onModInstalled(const QString& modName);
  void onModFilterActive(bool filterActive);

  // refresh the overwrite markers and the highlighted plugins from
  // the current selection
  //
  void refreshMarkersAndPlugins();

  // clear overwrite markers (without repainting)
  //
  void clearOverwriteMarkers();

  // set overwrite markers from the mod in the given list and repaint (if the list
  // is empty, clear overwrite and repaint)
  //
  void setOverwriteMarkers(const QModelIndexList& indexes);

  // retrieve the marker color for the given index
  //
  QColor markerColor(const QModelIndex& index) const;

  // retrieve the mod flags for the given index
  //
  std::vector<ModInfo::EFlag> modFlags(const QModelIndex& index,
                                       bool* forceCompact = nullptr) const;

  // retrieve the conflicts flags for the given index
  //
  std::vector<ModInfo::EConflictFlag> conflictFlags(const QModelIndex& index,
                                                    bool* forceCompact = nullptr) const;

  // retrieve the content icons and tooltip for the given index
  //
  std::set<int> contents(const QModelIndex& index, bool* includeChildren) const;
  QList<QString> contentsIcons(const QModelIndex& index,
                               bool* forceCompact = nullptr) const;
  QString contentsTooltip(const QModelIndex& index) const;

  // compute the counters for mods according to the current filter
  //
  ModCounters counters() const;

  // get/set the selected items on the view, this method return/take indices
  // from the mod list model, not the view, so it's safe to restore
  //
  std::pair<QModelIndex, QModelIndexList> selected() const;
  void setSelected(const QModelIndex& current, const QModelIndexList& selected);

  // refresh stored expanded items for the current intermediate proxy
  //
  void refreshExpandedItems();

  // refresh the group-by proxy, if the index is -1 will refresh the
  // current one (e.g. when changing the sort column)
  //
  void updateGroupByProxy();

public:  // member variables
  OrganizerCore* m_core;
  std::unique_ptr<FilterList> m_filters;
  CategoryFactory* m_categories;
  ModListViewUi ui;
  ModListViewActions* m_actions;

  ModListSortProxy* m_sortProxy;
  ModListByPriorityProxy* m_byPriorityProxy;
  QtGroupingProxy* m_byCategoryProxy;
  QtGroupingProxy* m_byNexusIdProxy;

  // marker used to avoid calling refreshing markers to many
  // time in a row
  QTimer m_refreshMarkersTimer;

  // maintain collapsed items for each proxy to avoid
  // losing them on model reset
  std::map<QAbstractItemModel*, std::set<QString>> m_collapsed;

  MarkerInfos m_markers;
  ViewMarkingScrollBar* m_scrollbar;

  bool m_inDragMoveEvent = false;

  // replace the auto-expand timer from QTreeView to avoid
  // auto-collapsing
  QBasicTimer m_openTimer;
};

#endif  // MODLISTVIEW_H
