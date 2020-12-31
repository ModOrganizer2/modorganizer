#include "pluginlistview.h"

#include <QUrl>
#include <QMimeData>

#include <widgetutility.h>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "organizercore.h"
#include "pluginlistsortproxy.h"
#include "genericicondelegate.h"
#include "modelutils.h"

PluginListView::PluginListView(QWidget *parent)
  : QTreeView(parent)
  , m_Scrollbar(new ViewMarkingScrollBar(this->model(), this))
{
  setVerticalScrollBar(m_Scrollbar);
  MOBase::setCustomizableColumns(this);
}

void PluginListView::setModel(QAbstractItemModel *model)
{
  QTreeView::setModel(model);
  setVerticalScrollBar(new ViewMarkingScrollBar(model, this));
}

int PluginListView::sortColumn() const
{
  return m_sortProxy ? m_sortProxy->sortColumn() : -1;
}

QModelIndex PluginListView::indexModelToView(const QModelIndex& index) const
{
  return ::indexModelToView(index, this);
}

QModelIndexList PluginListView::indexModelToView(const QModelIndexList& index) const
{
  return ::indexModelToView(index, this);
}

QModelIndex PluginListView::indexViewToModel(const QModelIndex& index) const
{
  return ::indexViewToModel(index, m_core->pluginList());
}

QModelIndexList PluginListView::indexViewToModel(const QModelIndexList& index) const
{
  return ::indexViewToModel(index, m_core->pluginList());
}

void PluginListView::updatePluginCount()
{
  int activeMasterCount = 0;
  int activeLightMasterCount = 0;
  int activeRegularCount = 0;
  int masterCount = 0;
  int lightMasterCount = 0;
  int regularCount = 0;
  int activeVisibleCount = 0;

  PluginList* list = m_core->pluginList();
  QString filter = ui.filter->text();

  for (QString plugin : list->pluginNames()) {
    bool active = list->isEnabled(plugin);
    bool visible = m_sortProxy->filterMatchesPlugin(plugin);
    if (list->isLight(plugin) || list->isLightFlagged(plugin)) {
      lightMasterCount++;
      activeLightMasterCount += active;
      activeVisibleCount += visible && active;
    }
    else if (list->isMaster(plugin)) {
      masterCount++;
      activeMasterCount += active;
      activeVisibleCount += visible && active;
    }
    else {
      regularCount++;
      activeRegularCount += active;
      activeVisibleCount += visible && active;
    }
  }

  int activeCount = activeMasterCount + activeLightMasterCount + activeRegularCount;
  int totalCount = masterCount + lightMasterCount + regularCount;

  ui.counter->display(activeVisibleCount);
  ui.counter->setToolTip(tr("<table cellspacing=\"6\">"
    "<tr><th>Type</th><th>Active      </th><th>Total</th></tr>"
    "<tr><td>All plugins:</td><td align=right>%1    </td><td align=right>%2</td></tr>"
    "<tr><td>ESMs:</td><td align=right>%3    </td><td align=right>%4</td></tr>"
    "<tr><td>ESPs:</td><td align=right>%7    </td><td align=right>%8</td></tr>"
    "<tr><td>ESMs+ESPs:</td><td align=right>%9    </td><td align=right>%10</td></tr>"
    "<tr><td>ESLs:</td><td align=right>%5    </td><td align=right>%6</td></tr>"
    "</table>")
    .arg(activeCount).arg(totalCount)
    .arg(activeMasterCount).arg(masterCount)
    .arg(activeLightMasterCount).arg(lightMasterCount)
    .arg(activeRegularCount).arg(regularCount)
    .arg(activeMasterCount + activeRegularCount).arg(masterCount + regularCount)
  );
}

void PluginListView::onFilterChanged(const QString& filter)
{
  if (!filter.isEmpty()) {
    setStyleSheet("QTreeView { border: 2px ridge #f00; }");
    ui.counter->setStyleSheet("QLCDNumber { border: 2px ridge #f00; }");
  }
  else {
    setStyleSheet("");
    ui.counter->setStyleSheet("");
  }
  updatePluginCount();
}

void PluginListView::setup(OrganizerCore& core, MainWindow* mw, Ui::MainWindow* mwui)
{
  m_core = &core;
  ui = { mwui->activePluginsCounter, mwui->espFilterEdit };

  m_sortProxy = new PluginListSortProxy(&core);
  m_sortProxy->setSourceModel(core.pluginList());
  setModel(m_sortProxy);

  sortByColumn(PluginList::COL_PRIORITY, Qt::AscendingOrder);
  setItemDelegateForColumn(PluginList::COL_FLAGS, new GenericIconDelegate(this));
  installEventFilter(core.pluginList());

  connect(ui.filter, &QLineEdit::textChanged, m_sortProxy, &PluginListSortProxy::updateFilter);
  connect(ui.filter, &QLineEdit::textChanged, this, &PluginListView::onFilterChanged);

}
