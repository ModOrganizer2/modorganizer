#include "pluginlistview.h"

#include <QUrl>
#include <QMimeData>

#include <report.h>
#include <widgetutility.h>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "organizercore.h"
#include "pluginlistsortproxy.h"
#include "pluginlistcontextmenu.h"
#include "modlistview.h"
#include "copyeventfilter.h"
#include "modlistviewactions.h"
#include "genericicondelegate.h"
#include "modelutils.h"

using namespace MOBase;

PluginListView::PluginListView(QWidget *parent)
  : QTreeView(parent)
  , m_sortProxy(nullptr)
  , m_Scrollbar(new ViewMarkingScrollBar(this, Qt::BackgroundRole))
  , m_didUpdateMasterList(false)
{
  setVerticalScrollBar(m_Scrollbar);
  MOBase::setCustomizableColumns(this);
  installEventFilter(new CopyEventFilter(this));
}

int PluginListView::sortColumn() const
{
  return m_sortProxy ? m_sortProxy->sortColumn() : -1;
}

QModelIndex PluginListView::indexModelToView(const QModelIndex& index) const
{
  return MOShared::indexModelToView(index, this);
}

QModelIndexList PluginListView::indexModelToView(const QModelIndexList& index) const
{
  return MOShared::indexModelToView(index, this);
}

QModelIndex PluginListView::indexViewToModel(const QModelIndex& index) const
{
  return MOShared::indexViewToModel(index, m_core->pluginList());
}

QModelIndexList PluginListView::indexViewToModel(const QModelIndexList& index) const
{
  return MOShared::indexViewToModel(index, m_core->pluginList());
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
    if (list->hasLightExtension(plugin) || list->isLightFlagged(plugin)) {
      lightMasterCount++;
      activeLightMasterCount += active;
      activeVisibleCount += visible && active;
    }
    else if (list->hasMasterExtension(plugin) || list->isMasterFlagged(plugin)) {
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

void PluginListView::onSortButtonClicked()
{
  const bool offline = m_core->settings().network().offlineMode();

  auto r = QMessageBox::No;

  if (offline) {
    r = QMessageBox::question(
      topLevelWidget(), tr("Sorting plugins"),
      tr("Are you sure you want to sort your plugins list?") + "\r\n\r\n" +
      tr("Note: You are currently in offline mode and LOOT will not update the master list."),
      QMessageBox::Yes | QMessageBox::No);
  }
  else {
    r = QMessageBox::question(
      topLevelWidget(), tr("Sorting plugins"),
      tr("Are you sure you want to sort your plugins list?"),
      QMessageBox::Yes | QMessageBox::No);
  }

  if (r != QMessageBox::Yes) {
    return;
  }

  m_core->savePluginList();

  topLevelWidget()->setEnabled(false);
  Guard g([=]() { topLevelWidget()->setEnabled(true); });

  // don't try to update the master list in offline mode
  const bool didUpdateMasterList = offline ? true : m_didUpdateMasterList;

  if (runLoot(topLevelWidget(), *m_core, didUpdateMasterList)) {
    // don't assume the master list was updated in offline mode
    if (!offline) {
      m_didUpdateMasterList = true;
    }

    m_core->refreshESPList(false);
    m_core->savePluginList();
  }
}

std::pair<QModelIndex, QModelIndexList> PluginListView::selected() const
{
  return { indexViewToModel(currentIndex()), indexViewToModel(selectionModel()->selectedRows()) };
}

void PluginListView::setSelected(const QModelIndex& current, const QModelIndexList& selected)
{
  setCurrentIndex(indexModelToView(current));
  for (auto idx : selected) {
    selectionModel()->select(indexModelToView(idx), QItemSelectionModel::Select | QItemSelectionModel::Rows);
  }
}


void PluginListView::setup(OrganizerCore& core, MainWindow* mw, Ui::MainWindow* mwui)
{
  m_core = &core;
  ui = { mwui->activePluginsCounter, mwui->espFilterEdit };
  m_modActions = &mwui->modList->actions();

  m_sortProxy = new PluginListSortProxy(&core);
  m_sortProxy->setSourceModel(core.pluginList());
  setModel(m_sortProxy);

  sortByColumn(PluginList::COL_PRIORITY, Qt::AscendingOrder);
  setItemDelegateForColumn(PluginList::COL_FLAGS, new GenericIconDelegate(this));

  // counter
  connect(core.pluginList(), &PluginList::writePluginsList, [=]{ updatePluginCount(); });
  connect(core.pluginList(), &PluginList::esplist_changed, [=]{ updatePluginCount(); });

  // sort
  connect(mwui->bossButton, &QPushButton::clicked, [=]{ onSortButtonClicked(); });

  // filter
  connect(ui.filter, &QLineEdit::textChanged, m_sortProxy, &PluginListSortProxy::updateFilter);
  connect(ui.filter, &QLineEdit::textChanged, this, &PluginListView::onFilterChanged);

  // highligth mod list when selected
  connect(selectionModel(), &QItemSelectionModel::selectionChanged, [=](auto&& selected) {
    std::vector<unsigned int> pluginIndices;
    for (auto& idx : indexViewToModel(selectionModel()->selectedRows())) {
      pluginIndices.push_back(idx.row());
    }
    mwui->modList->setHighlightedMods(pluginIndices);
  });

  // using a lambda here to avoid storing the mod list actions
  connect(this, &QTreeView::customContextMenuRequested, [=](auto&& pos) { onCustomContextMenuRequested(pos); });
  connect(this, &QTreeView::doubleClicked, [=](auto&& index) { onDoubleClicked(index); });
}

void PluginListView::onCustomContextMenuRequested(const QPoint& pos)
{
  try {
    PluginListContextMenu menu(indexViewToModel(indexAt(pos)), *m_core, this);
    connect(&menu, &PluginListContextMenu::openModInformation, [=](auto&& modIndex) {
      m_modActions->displayModInformation(modIndex); });
    menu.exec(viewport()->mapToGlobal(pos));
  }
  catch (const std::exception& e) {
    reportError(tr("Exception: ").arg(e.what()));
  }
  catch (...) {
    reportError(tr("Unknown exception"));
  }
}

void PluginListView::onDoubleClicked(const QModelIndex& index)
{
  if (!index.isValid()) {
    return;
  }

  if (m_core->pluginList()->timeElapsedSinceLastChecked() <= QApplication::doubleClickInterval()) {
    // don't interpret double click if we only just checked a plugin
    return;
  }

  try {
    if (selectionModel()->hasSelection() && selectionModel()->selectedRows().count() == 1) {

      QModelIndex idx = selectionModel()->currentIndex();
      QString fileName = idx.data().toString();

      if (ModInfo::getIndex(m_core->pluginList()->origin(fileName)) == UINT_MAX) {
        return;
      }

      auto modIndex = ModInfo::getIndex(m_core->pluginList()->origin(fileName));
      ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);

      if (modInfo->isRegular() || modInfo->isOverwrite()) {

        Qt::KeyboardModifiers modifiers = QApplication::queryKeyboardModifiers();
        if (modifiers.testFlag(Qt::ControlModifier)) {
          m_modActions->openExplorer({ m_core->modList()->index(modIndex, 0) });
        }
        else {
          m_modActions->displayModInformation(ModInfo::getIndex(m_core->pluginList()->origin(fileName)));
        }

        // workaround to cancel the editor that might have opened because of
        // selection-click
        closePersistentEditor(index);
      }
    }
  }
  catch (const std::exception& e) {
    reportError(e.what());
  }
}

bool PluginListView::moveSelection(int key)
{
  auto [cindex, sourceRows] = selected();

  int offset = key == Qt::Key_Up ? -1 : 1;
  if (m_sortProxy->sortOrder() == Qt::DescendingOrder) {
    offset = -offset;
  }

  m_core->pluginList()->shiftPluginsPriority(sourceRows, offset);

  // reset the selection and the index
  setSelected(cindex, sourceRows);

  return true;
}

bool PluginListView::toggleSelectionState()
{
  if (!selectionModel()->hasSelection()) {
    return true;
  }
  m_core->pluginList()->toggleState(indexViewToModel(selectionModel()->selectedRows()));
  return true;
}

bool PluginListView::event(QEvent* event)
{
  if (event->type() == QEvent::KeyPress) {
    QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

    if (keyEvent->modifiers() == Qt::ControlModifier
      && (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)) {
      if (selectionModel()->hasSelection() && selectionModel()->selectedRows().count() == 1) {
        QModelIndex idx = selectionModel()->currentIndex();
        QString fileName = idx.data().toString();

        if (ModInfo::getIndex(m_core->pluginList()->origin(fileName)) == UINT_MAX) {
          return false;
        }

        auto modIndex = ModInfo::getIndex(m_core->pluginList()->origin(fileName));
        m_modActions->openExplorer({ m_core->modList()->index(modIndex, 0) });
        return true;
      }
    }
    else if (keyEvent->modifiers() == Qt::ControlModifier
      && (sortColumn() == PluginList::COL_PRIORITY || sortColumn() == PluginList::COL_MODINDEX)
      && (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down)) {
      return moveSelection(keyEvent->key());
    }
    else if (keyEvent->key() == Qt::Key_Space) {
      return toggleSelectionState();
    }
    return QTreeView::event(event);
  }
  return QTreeView::event(event);
}
