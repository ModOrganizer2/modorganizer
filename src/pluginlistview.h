#ifndef PLUGINLISTVIEW_H
#define PLUGINLISTVIEW_H

#include "viewmarkingscrollbar.h"
#include <QDragEnterEvent>
#include <QTreeView>

namespace Ui
{
class MainWindow;
}

class OrganizerCore;
class MainWindow;
class ModListViewActions;
class PluginListSortProxy;

class PluginListView : public QTreeView
{
  Q_OBJECT
public:
  explicit PluginListView(QWidget* parent = nullptr);

  void activated();
  void setup(OrganizerCore& core, MainWindow* mw, Ui::MainWindow* mwui);

  // the column by which the plugin list is currently sorted
  //
  int sortColumn() const;

  // update the plugin counter
  //
  void updatePluginCount();

protected slots:

  void onCustomContextMenuRequested(const QPoint& pos);
  void onDoubleClicked(const QModelIndex& index);

  void onFilterChanged(const QString& filter);
  void onSortButtonClicked();

protected:
  friend class PluginListContextMenu;

  // map from/to the view indexes to the model
  //
  QModelIndex indexModelToView(const QModelIndex& index) const;
  QModelIndexList indexModelToView(const QModelIndexList& index) const;
  QModelIndex indexViewToModel(const QModelIndex& index) const;
  QModelIndexList indexViewToModel(const QModelIndexList& index) const;

  // method to react to various key events
  //
  bool moveSelection(int key);
  bool toggleSelectionState();

  // get/set the selected items on the view, this method return/take indices
  // from the mod list model, not the view, so it's safe to restore
  //
  std::pair<QModelIndex, QModelIndexList> selected() const;
  void setSelected(const QModelIndex& current, const QModelIndexList& selected);

  bool event(QEvent* event) override;

private:
  struct PluginListViewUi
  {
    // the plguin counter
    QLCDNumber* counter;

    // the filter
    QLineEdit* filter;
  };

  OrganizerCore* m_core;
  PluginListViewUi ui;

  PluginListSortProxy* m_sortProxy;
  ModListViewActions* m_modActions;
  ViewMarkingScrollBar* m_Scrollbar;

  bool m_didUpdateMasterList;
};

#endif  // PLUGINLISTVIEW_H
