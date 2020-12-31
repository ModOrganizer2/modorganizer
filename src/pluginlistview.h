#ifndef PLUGINLISTVIEW_H
#define PLUGINLISTVIEW_H

#include <QTreeView>
#include <QDragEnterEvent>
#include "viewmarkingscrollbar.h"

namespace Ui {
  class MainWindow;
}

class OrganizerCore;
class MainWindow;
class PluginListSortProxy;

class PluginListView : public QTreeView
{
  Q_OBJECT
public:
  explicit PluginListView(QWidget* parent = nullptr);
  void setModel(QAbstractItemModel* model) override;

  void setup(OrganizerCore& core, MainWindow* mw, Ui::MainWindow* mwui);

  // the column by which the plugin list is currently sorted
  //
  int sortColumn() const;

  // update the plugin counter
  //
  void updatePluginCount();

  // TODO: Move these to private when possible.
  // map from/to the view indexes to the model
  //
  QModelIndex indexModelToView(const QModelIndex& index) const;
  QModelIndexList indexModelToView(const QModelIndexList& index) const;
  QModelIndex indexViewToModel(const QModelIndex& index) const;
  QModelIndexList indexViewToModel(const QModelIndexList& index) const;


protected slots:

  void onFilterChanged(const QString& filter);

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

  ViewMarkingScrollBar* m_Scrollbar;
};

#endif // PLUGINLISTVIEW_H
