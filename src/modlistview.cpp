#include "modlistview.h"
#include <QUrl>
#include <QMimeData>
#include <QProxyStyle>

#include <widgetutility.h>

#include "ui_mainwindow.h"

#include "organizercore.h"
#include "modlist.h"
#include "modlistsortproxy.h"
#include "modlistbypriorityproxy.h"
#include "log.h"
#include "modflagicondelegate.h"
#include "modconflicticondelegate.h"
#include "genericicondelegate.h"

class ModListProxyStyle : public QProxyStyle {
public:

  using QProxyStyle::QProxyStyle;

  void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget) const override
  {
    if (element == QStyle::PE_IndicatorItemViewItemDrop)
    {
      QStyleOption opt(*option);
      opt.rect.setLeft(0);
      if (auto* view = qobject_cast<const QTreeView*>(widget)) {
        opt.rect.setLeft(view->indentation());
        opt.rect.setRight(widget->width());
      }
      QProxyStyle::drawPrimitive(element, &opt, painter, widget);
    }
    else {
      QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
  }

};

class ModListStyledItemDelegated : public QStyledItemDelegate
{
  QTreeView* m_view;

public:

  ModListStyledItemDelegated(QTreeView* view) :
    QStyledItemDelegate(view), m_view(view) { }

  using QStyledItemDelegate::QStyledItemDelegate;
  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
    QStyleOptionViewItem opt(option);
    if (index.column() == 0) {
      if (!index.model()->hasChildren(index) && index.parent().isValid()) {
        auto parentIndex = index.parent().data(ModList::IndexRole).toInt();
        if (ModInfo::getByIndex(parentIndex)->isSeparator()) {
          opt.rect.adjust(-m_view->indentation(), 0, 0, 0);
        }
      }
    }
    QStyledItemDelegate::paint(painter, opt, index);
  }
};

ModListView::ModListView(QWidget* parent)
  : QTreeView(parent)
  , m_scrollbar(new ViewMarkingScrollBar(this->model(), this))
{
  setVerticalScrollBar(m_scrollbar);
  MOBase::setCustomizableColumns(this);
  setAutoExpandDelay(1000);

  setStyle(new ModListProxyStyle());
  setItemDelegate(new ModListStyledItemDelegated(this));
}

void ModListView::refreshStyle()
{
  // maybe there is a better way but I did not find one
  QString sheet = styleSheet();
  setStyleSheet("QTreeView { }");
  setStyleSheet(sheet);
}

void ModListView::setProfile(Profile* profile)
{
  m_sortProxy->setProfile(profile);
  m_byPriorityProxy->setProfile(profile);
}

bool ModListView::hasCollapsibleSeparators() const
{
  return m_sortProxy != nullptr && m_sortProxy->sourceModel() == m_byPriorityProxy;
}

int ModListView::sortColumn() const
{
  return m_sortProxy ? m_sortProxy->sortColumn() : -1;
}

int ModListView::nextMod(int modIndex) const
{
  const QModelIndex start = indexModelToView(m_core->modList()->index(modIndex, 0));

  auto index = start;

  for (;;) {
    index = model()->index((index.row() + 1) % model()->rowCount(), 0);
    modIndex = indexViewToModel(index).data(ModList::IndexRole).toInt();

    if (index == start || !index.isValid()) {
      // wrapped around, give up
      break;
    }

    ModInfo::Ptr mod = ModInfo::getByIndex(modIndex);

    // skip overwrite and backups and separators
    if (mod->hasFlag(ModInfo::FLAG_OVERWRITE) ||
      mod->hasFlag(ModInfo::FLAG_BACKUP) ||
      mod->hasFlag(ModInfo::FLAG_SEPARATOR)) {
      continue;
    }

    return modIndex;
  }

  return -1;
}

int ModListView::prevMod(int modIndex) const
{
  const QModelIndex start = indexModelToView(m_core->modList()->index(modIndex, 0));

  auto index = start;

  for (;;) {
    int row = index.row() - 1;
    if (row == -1) {
      row = model()->rowCount() - 1;
    }

    index = model()->index(row, 0);
    modIndex = indexViewToModel(index).data(ModList::IndexRole).toInt();

    if (index == start || !index.isValid()) {
      // wrapped around, give up
      break;
    }

    // skip overwrite and backups and separators
    ModInfo::Ptr mod = ModInfo::getByIndex(modIndex);

    if (mod->hasFlag(ModInfo::FLAG_OVERWRITE) ||
      mod->hasFlag(ModInfo::FLAG_BACKUP) ||
      mod->hasFlag(ModInfo::FLAG_SEPARATOR)) {
      continue;
    }

    return modIndex;
  }

  return -1;
}

void ModListView::invalidate()
{
  if (m_sortProxy) {
    m_sortProxy->invalidate();
  }
}

void ModListView::enableAllVisible()
{
  Profile* profile = m_core->currentProfile();

  QList<unsigned int> modsToEnable;
  for (auto& index : allIndex(model())) {
    modsToEnable.append(index.data(ModList::IndexRole).toInt());
  }
  profile->setModsEnabled(modsToEnable, {});
  invalidate();
}

void ModListView::disableAllVisible()
{
  MOBase::log::debug("disableAllVisible: {}", model()->rowCount());
  Profile* profile = m_core->currentProfile();

  QList<unsigned int> modsToDisable;
  for (auto& index : allIndex(model())) {
    modsToDisable.append(index.data(ModList::IndexRole).toInt());
  }
  profile->setModsEnabled({}, modsToDisable);
  invalidate();
}

void ModListView::enableSelected()
{
  Profile* profile = m_core->currentProfile();
  if (selectionModel()->hasSelection()) {
    QList<unsigned int> modsToEnable;
    for (auto row : selectionModel()->selectedRows(ModList::COL_PRIORITY)) {
      int modID = profile->modIndexByPriority(row.data().toInt());
      modsToEnable.append(modID);
    }
    profile->setModsEnabled(modsToEnable, {});
  }
  invalidate();
}

void ModListView::disableSelected()
{
  Profile* profile = m_core->currentProfile();
  if (selectionModel()->hasSelection()) {
    QList<unsigned int> modsToDisable;
    for (auto row : selectionModel()->selectedRows(ModList::COL_PRIORITY)) {
      int modID = profile->modIndexByPriority(row.data().toInt());
      modsToDisable.append(modID);
    }
    profile->setModsEnabled({}, modsToDisable);
  }
  invalidate();
}

void ModListView::setFilterCriteria(const std::vector<ModListSortProxy::Criteria>& criteria)
{
  m_sortProxy->setCriteria(criteria);
}

void ModListView::setFilterOptions(ModListSortProxy::FilterMode mode, ModListSortProxy::SeparatorsMode sep)
{
  m_sortProxy->setOptions(mode, sep);
}

bool ModListView::isModVisible(unsigned int index) const
{
  return m_sortProxy->filterMatchesMod(ModInfo::getByIndex(index), m_core->currentProfile()->modEnabled(index));
}

bool ModListView::isModVisible(ModInfo::Ptr mod) const
{
  return m_sortProxy->filterMatchesMod(mod, m_core->currentProfile()->modEnabled(ModInfo::getIndex(mod->name())));
}

QModelIndex ModListView::indexModelToView(const QModelIndex& index) const
{
  if (index.model() != m_core->modList()) {
    return QModelIndex();
  }

  // we need to stack the proxy
  std::vector<QAbstractProxyModel*> proxies;
  {
    auto* currentModel = model();
    while (auto* proxy = qobject_cast<QAbstractProxyModel*>(currentModel)) {
      proxies.push_back(proxy);
      currentModel = proxy->sourceModel();
    }
  }

  if (proxies.empty() || proxies.back()->sourceModel() != m_core->modList()) {
    return QModelIndex();
  }

  auto qindex = index;
  for (auto rit = proxies.rbegin(); rit != proxies.rend(); ++rit) {
    qindex = (*rit)->mapFromSource(qindex);
  }

  return qindex;
}

QModelIndex ModListView::indexViewToModel(const QModelIndex& index) const
{
  if (index.model() == m_core->modList()) {
    return index;
  }
  else if (auto* proxy = qobject_cast<const QAbstractProxyModel*>(index.model())) {
    return indexViewToModel(proxy->mapToSource(index));
  }
  else {
    return QModelIndex();
  }
}

std::vector<QModelIndex> ModListView::allIndex(
  const QAbstractItemModel* model, int column, const QModelIndex& parent) const
{
  std::vector<QModelIndex> index;
  for (std::size_t i = 0; i < model->rowCount(parent); ++i) {
    index.push_back(model->index(i, column, parent));

    auto cindex = allIndex(model, column, index.back());
    index.insert(index.end(), cindex.begin(), cindex.end());
  }
  return index;
}

void ModListView::expandItem(const QModelIndex& index) {
  if (index.model() == m_sortProxy->sourceModel()) {
    expand(m_sortProxy->mapFromSource(index));
  }
  else if (index.model() == model()) {
    expand(index);
  }
}

void ModListView::onModPrioritiesChanged(std::vector<int> const& indices)
{
  if (m_sortProxy != nullptr) {
    // expand separator whose priority has changed
    if (hasCollapsibleSeparators()) {
      for (auto index : indices) {
        ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
        if (modInfo->isSeparator()) {
          expand(indexModelToView(m_core->modList()->index(index, 0)));
        }
      }
    }
    m_sortProxy->invalidate();
  }
}

void ModListView::onModInstalled(const QString& modName)
{
  unsigned int index = ModInfo::getIndex(modName);

  if (index == UINT_MAX) {
    return;
  }

  QModelIndex qIndex = indexModelToView(m_core->modList()->index(index, 0));

  if (hasCollapsibleSeparators()) {
    expand(qIndex);
  }

  // focus, scroll to and select
  setFocus(Qt::OtherFocusReason);
  scrollTo(qIndex);
  setCurrentIndex(qIndex);
  selectionModel()->select(qIndex, QItemSelectionModel::Select | QItemSelectionModel::Rows);
}

void ModListView::onModFilterActive(bool filterActive)
{
  ui.clearFilters->setVisible(filterActive);
  if (filterActive) {
    setStyleSheet("QTreeView { border: 2px ridge #f00; }");
    ui.counter->setStyleSheet("QLCDNumber { border: 2px ridge #f00; }");
  }
  else if (ui.groupBy->currentIndex() != GroupBy::NONE) {
    setStyleSheet("QTreeView { border: 2px ridge #337733; }");
    ui.counter->setStyleSheet("");
  }
  else {
    setStyleSheet("");
    ui.counter->setStyleSheet("");
  }
}

void ModListView::updateModCount()
{
  int activeCount = 0;
  int visActiveCount = 0;
  int backupCount = 0;
  int visBackupCount = 0;
  int foreignCount = 0;
  int visForeignCount = 0;
  int separatorCount = 0;
  int visSeparatorCount = 0;
  int regularCount = 0;
  int visRegularCount = 0;

  QStringList allMods = m_core->modList()->allMods();

  auto hasFlag = [](std::vector<ModInfo::EFlag> flags, ModInfo::EFlag filter) {
    return std::find(flags.begin(), flags.end(), filter) != flags.end();
  };

  bool isEnabled;
  bool isVisible;
  for (QString mod : allMods) {
    int modIndex = ModInfo::getIndex(mod);
    ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);
    std::vector<ModInfo::EFlag> modFlags = modInfo->getFlags();
    isEnabled = m_core->currentProfile()->modEnabled(modIndex);
    isVisible = m_sortProxy->filterMatchesMod(modInfo, isEnabled);

    for (auto flag : modFlags) {
      switch (flag) {
      case ModInfo::FLAG_BACKUP: backupCount++;
        if (isVisible)
          visBackupCount++;
        break;
      case ModInfo::FLAG_FOREIGN: foreignCount++;
        if (isVisible)
          visForeignCount++;
        break;
      case ModInfo::FLAG_SEPARATOR: separatorCount++;
        if (isVisible)
          visSeparatorCount++;
        break;
      }
    }

    if (!hasFlag(modFlags, ModInfo::FLAG_BACKUP) &&
      !hasFlag(modFlags, ModInfo::FLAG_FOREIGN) &&
      !hasFlag(modFlags, ModInfo::FLAG_SEPARATOR) &&
      !hasFlag(modFlags, ModInfo::FLAG_OVERWRITE)) {
      if (isEnabled) {
        activeCount++;
        if (isVisible)
          visActiveCount++;
      }
      if (isVisible)
        visRegularCount++;
      regularCount++;
    }
  }

  ui.counter->display(visActiveCount);
  ui.counter->setToolTip(tr("<table cellspacing=\"5\">"
    "<tr><th>Type</th><th>All</th><th>Visible</th>"
    "<tr><td>Enabled mods:&emsp;</td><td align=right>%1 / %2</td><td align=right>%3 / %4</td></tr>"
    "<tr><td>Unmanaged/DLCs:&emsp;</td><td align=right>%5</td><td align=right>%6</td></tr>"
    "<tr><td>Mod backups:&emsp;</td><td align=right>%7</td><td align=right>%8</td></tr>"
    "<tr><td>Separators:&emsp;</td><td align=right>%9</td><td align=right>%10</td></tr>"
    "</table>")
    .arg(activeCount)
    .arg(regularCount)
    .arg(visActiveCount)
    .arg(visRegularCount)
    .arg(foreignCount)
    .arg(visForeignCount)
    .arg(backupCount)
    .arg(visBackupCount)
    .arg(separatorCount)
    .arg(visSeparatorCount)
  );
}

void ModListView::updateGroupByProxy(int groupIndex)
{
  // if the index is -1, we do not refresh unless we are grouping
  // by separator
  if (groupIndex == -1) {
    if (ui.groupBy->currentIndex() != GroupBy::NONE) {
      return;
    }
    groupIndex = ui.groupBy->currentIndex();
  }

  if (groupIndex == GroupBy::CATEGORY) {
    m_byCategoryProxy->setGroupedColumn(ModList::COL_CATEGORY);
    m_sortProxy->setSourceModel(m_byCategoryProxy);
  }
  else if (groupIndex == GroupBy::NEXUS_ID) {
    m_byNexusIdProxy->setGroupedColumn(ModList::COL_MODID);
    m_sortProxy->setSourceModel(m_byNexusIdProxy);
  }
  else if (m_sortProxy->sortColumn() == ModList::COL_PRIORITY
    && m_sortProxy->sortOrder() == Qt::AscendingOrder) {
    m_sortProxy->setSourceModel(m_byPriorityProxy);
    m_byPriorityProxy->refresh();
  }
  else {
    m_sortProxy->setSourceModel(m_core->modList());
  }
}

void ModListView::setup(OrganizerCore& core, Ui::MainWindow* mwui)
{
  // attributes
  m_core = &core;
  ui = { mwui->groupCombo, mwui->activeModsCounter, mwui->modFilterEdit, mwui->clearFiltersButton };

  connect(m_core, &OrganizerCore::modInstalled, this, &ModListView::onModInstalled);
  connect(core.modList(), &ModList::modPrioritiesChanged, this, &ModListView::onModPrioritiesChanged);

  m_byPriorityProxy = new ModListByPriorityProxy(core.currentProfile(), &core);
  m_byPriorityProxy->setSourceModel(core.modList());
  connect(this, &QTreeView::expanded, m_byPriorityProxy, &ModListByPriorityProxy::expanded);
  connect(this, &QTreeView::collapsed, m_byPriorityProxy, &ModListByPriorityProxy::collapsed);
  connect(m_byPriorityProxy, &ModListByPriorityProxy::expandItem, this, &ModListView::expandItem);

  m_byCategoryProxy = new QtGroupingProxy(core.modList(), QModelIndex(), ModList::COL_CATEGORY,
    Qt::UserRole, 0, Qt::UserRole + 2);
  connect(this, &QTreeView::expanded, m_byCategoryProxy, &QtGroupingProxy::expanded);
  connect(this, &QTreeView::collapsed, m_byCategoryProxy, &QtGroupingProxy::collapsed);
  connect(m_byCategoryProxy, &QtGroupingProxy::expandItem, this, &ModListView::expandItem);
  m_byNexusIdProxy = new QtGroupingProxy(core.modList(), QModelIndex(), ModList::COL_MODID, Qt::DisplayRole,
      QtGroupingProxy::FLAG_NOGROUPNAME | QtGroupingProxy::FLAG_NOSINGLE, Qt::UserRole + 2);
  connect(this, &QTreeView::expanded, m_byNexusIdProxy, &QtGroupingProxy::expanded);
  connect(this, &QTreeView::collapsed, m_byNexusIdProxy, &QtGroupingProxy::collapsed);
  connect(m_byNexusIdProxy, &QtGroupingProxy::expandItem, this, &ModListView::expandItem);

  m_sortProxy = new ModListSortProxy(core.currentProfile(), &core);
  setModel(m_sortProxy);

  connect(m_sortProxy, &QAbstractItemModel::layoutAboutToBeChanged,
    this, [this](const QList<QPersistentModelIndex>& parents, QAbstractItemModel::LayoutChangeHint hint) {
      if (hint == QAbstractItemModel::VerticalSortHint) {
        updateGroupByProxy(-1);
      }
    });
  sortByColumn(ModList::COL_PRIORITY, Qt::AscendingOrder);

  connect(ui.groupBy, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [&](int index) {
    updateGroupByProxy(index);
    onModFilterActive(m_sortProxy->isFilterActive());
  });

  connect(this, &ModListView::dragEntered, core.modList(), &ModList::onDragEnter);
  connect(this, &ModListView::dropEntered, m_byPriorityProxy, &ModListByPriorityProxy::onDropEnter);

  connect(model(), &QAbstractItemModel::layoutChanged, this, &ModListView::updateModCount);

  connect(header(), &QHeaderView::sortIndicatorChanged, this, [&](int, Qt::SortOrder) {
    verticalScrollBar()->repaint(); });
  connect(header(), &QHeaderView::sectionResized, this, [&](int logicalIndex, int oldSize, int newSize) {
    m_sortProxy->setColumnVisible(logicalIndex, newSize != 0); });

  GenericIconDelegate* contentDelegate = new GenericIconDelegate(this, Qt::UserRole + 3, ModList::COL_CONTENT, 150);
  ModFlagIconDelegate* flagDelegate = new ModFlagIconDelegate(this, ModList::COL_FLAGS, 120);
  ModConflictIconDelegate* conflictFlagDelegate = new ModConflictIconDelegate(this, ModList::COL_CONFLICTFLAGS, 80);

  connect(header(), &QHeaderView::sectionResized, contentDelegate, &GenericIconDelegate::columnResized);
  connect(header(), &QHeaderView::sectionResized, flagDelegate, &ModFlagIconDelegate::columnResized);
  connect(header(), &QHeaderView::sectionResized, conflictFlagDelegate, &ModConflictIconDelegate::columnResized);

  setItemDelegateForColumn(ModList::COL_FLAGS, flagDelegate);
  setItemDelegateForColumn(ModList::COL_CONFLICTFLAGS, conflictFlagDelegate);
  setItemDelegateForColumn(ModList::COL_CONTENT, contentDelegate);

  // TODO: Check if this is really useful.
  header()->installEventFilter(m_core->modList());

  if (m_core->settings().geometry().restoreState(header())) {
    // hack: force the resize-signal to be triggered because restoreState doesn't seem to do that
    for (int column = 0; column <= ModList::COL_LASTCOLUMN; ++column) {
      int sectionSize = header()->sectionSize(column);
      header()->resizeSection(column, sectionSize + 1);
      header()->resizeSection(column, sectionSize);
    }
  }
  else {
    // hide these columns by default
    header()->setSectionHidden(ModList::COL_CONTENT, true);
    header()->setSectionHidden(ModList::COL_MODID, true);
    header()->setSectionHidden(ModList::COL_GAME, true);
    header()->setSectionHidden(ModList::COL_INSTALLTIME, true);
    header()->setSectionHidden(ModList::COL_NOTES, true);

    // resize mod list to fit content
    for (int i = 0; i < header()->count(); ++i) {
      header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }

    header()->setSectionResizeMode(ModList::COL_NAME, QHeaderView::Stretch);
  }

  // prevent the name-column from being hidden
  header()->setSectionHidden(ModList::COL_NAME, false);

  // TODO: Move the event filter in ModListView.
  installEventFilter(core.modList());

  connect(m_core->modList(), &ModList::downloadArchiveDropped, this, [this](int row, int priority) {
    m_core->installDownload(row, priority);
  });

  connect(m_sortProxy, &ModListSortProxy::filterActive, this, &ModListView::onModFilterActive);
  connect(ui.filter, &QLineEdit::textChanged, m_sortProxy, &ModListSortProxy::updateFilter);
  connect(m_sortProxy, &QAbstractItemModel::layoutChanged, this, [&]() {
    if (hasCollapsibleSeparators()) {
      m_byPriorityProxy->refreshExpandedItems();
    }
  });
}

QRect ModListView::visualRect(const QModelIndex& index) const
{
  QRect rect = QTreeView::visualRect(index);
  if (index.isValid() && !index.model()->hasChildren(index) && index.parent().isValid()) {
    auto parentIndex = index.parent().data(ModList::IndexRole).toInt();
    if (ModInfo::getByIndex(parentIndex)->isSeparator()) {
      rect.adjust(-indentation(), 0, 0, 0);
    }
  }
  return rect;
}

void ModListView::setModel(QAbstractItemModel* model)
{
  QTreeView::setModel(model);
  setVerticalScrollBar(new ViewMarkingScrollBar(model, this));
}

QModelIndexList ModListView::selectedIndexes() const
{
  return m_inDragMoveEvent ? QModelIndexList() : QTreeView::selectedIndexes();
}

void ModListView::dragEnterEvent(QDragEnterEvent* event)
{
  emit dragEntered(event->mimeData());
  QTreeView::dragEnterEvent(event);
}

void ModListView::dragMoveEvent(QDragMoveEvent* event)
{
  if (autoExpandDelay() >= 0) {
    m_openTimer.start(autoExpandDelay(), this);
  }

  // bypass QTreeView
  m_inDragMoveEvent = true;
  QAbstractItemView::dragMoveEvent(event);
  m_inDragMoveEvent = false;
}

void ModListView::dropEvent(QDropEvent* event)
{
  emit dropEntered(event->mimeData(), static_cast<DropPosition>(dropIndicatorPosition()));

  m_inDragMoveEvent = true;
  QTreeView::dropEvent(event);
  m_inDragMoveEvent = false;
}

void ModListView::timerEvent(QTimerEvent* event)
{
  if (event->timerId() == m_openTimer.timerId()) {
    QPoint pos = viewport()->mapFromGlobal(QCursor::pos());
    if (state() == QAbstractItemView::DraggingState
      && viewport()->rect().contains(pos)) {
      QModelIndex index = indexAt(pos);
      setExpanded(index, true);
    }
    m_openTimer.stop();
  }
  else {
    QTreeView::timerEvent(event);
  }
}
