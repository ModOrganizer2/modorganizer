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

class OrganizerCore;
class Profile;
class ModListByPriorityProxy;

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

  void setup(OrganizerCore& core, Ui::MainWindow* mwui);

  // set the current profile
  //
  void setProfile(Profile* profile);

  // check if collapsible separators are currently used
  //
  bool hasCollapsibleSeparators() const;

  // the column by which the mod list is currently sorted
  //
  int sortColumn() const;

  // retrieve the next/previous mod in the current view, the given index
  // should be a mod index (not a model row), and the return value will be
  // a mod index or -1 if no mod was found
  //
  int nextMod(int index) const;
  int prevMod(int index) const;

  // invalidate the top-level model
  //
  void invalidate();

  // enable/disable all visible mods
  //
  void enableAllVisible();
  void disableAllVisible();

  // enable/disable all selected mods
  //
  void enableSelected();
  void disableSelected();

  // set the filter criteria/options for mods
  //
  void setFilterCriteria(const std::vector<ModListSortProxy::Criteria>& criteria);
  void setFilterOptions(ModListSortProxy::FilterMode mode, ModListSortProxy::SeparatorsMode sep);

  // check if the given mod is visible
  //
  bool isModVisible(unsigned int index) const;
  bool isModVisible(ModInfo::Ptr mod) const;

  // re-implemented to fix indentation with collapsible separators
  //
  QRect visualRect(const QModelIndex& index) const override;

  // refresh the style of the mod list, this needs to be called when the
  // stylesheet is changed
  //
  void refreshStyle();

signals:

  void dragEntered(const QMimeData* mimeData);
  void dropEntered(const QMimeData* mimeData, DropPosition position);

public slots:

  void updateModCount();

protected:

  // map from/to the view indexes to the model
  //
  QModelIndex indexModelToView(const QModelIndex& index) const;
  QModelIndex indexViewToModel(const QModelIndex& index) const;

  // all index for the given model under the given index, recursively
  //
  std::vector<QModelIndex> allIndex(
    const QAbstractItemModel* model, int column = 0, const QModelIndex& index = QModelIndex()) const;

  // re-implemented to fake the return value to allow drag-and-drop on
  // itself for separators
  //
  QModelIndexList selectedIndexes() const;

  void timerEvent(QTimerEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  void dropEvent(QDropEvent* event) override;

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
