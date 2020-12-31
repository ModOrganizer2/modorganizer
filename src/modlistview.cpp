#include "modlistview.h"
#include <QUrl>
#include <QMimeData>
#include <QProxyStyle>

#include <widgetutility.h>

#include <utility.h>
#include <report.h>

#include "ui_mainwindow.h"

#include "filterlist.h"
#include "organizercore.h"
#include "modlist.h"
#include "modlistsortproxy.h"
#include "modlistbypriorityproxy.h"
#include "log.h"
#include "modflagicondelegate.h"
#include "modconflicticondelegate.h"
#include "modlistviewactions.h"
#include "modlistdropinfo.h"
#include "modlistcontextmenu.h"
#include "genericicondelegate.h"
#include "shared/directoryentry.h"
#include "shared/filesorigin.h"

using namespace MOBase;

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
  ModListView* m_view;

public:

  ModListStyledItemDelegated(ModListView* view) :
    QStyledItemDelegate(view), m_view(view) { }

  using QStyledItemDelegate::QStyledItemDelegate;
  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
    QStyleOptionViewItem opt(option);
    if (index.column() == 0 && m_view->hasCollapsibleSeparators()) {
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
  , m_core(nullptr)
  , m_sortProxy(nullptr)
  , m_byPriorityProxy(nullptr)
  , m_byCategoryProxy(nullptr)
  , m_byNexusIdProxy(nullptr)
  , m_scrollbar(new ViewMarkingScrollBar(this->model(), this))
{
  setVerticalScrollBar(m_scrollbar);
  MOBase::setCustomizableColumns(this);
  setAutoExpandDelay(1000);

  setStyle(new ModListProxyStyle(style()));
  setItemDelegate(new ModListStyledItemDelegated(this));

  connect(this, &ModListView::doubleClicked, this, &ModListView::onDoubleClicked);
  connect(this, &ModListView::customContextMenuRequested, this, &ModListView::onCustomContextMenuRequested);
}

void ModListView::refresh()
{
  updateGroupByProxy(-1);

  // since we use a proxy, modifying the stylesheet messes things
  // up by and this fixes it (force update style and fix drop indicator
  // after changing the stylesheet)
  if (auto* proxy = qobject_cast<QProxyStyle*>(style())) {
    auto* s = proxy->baseStyle();
    setStyle(new ModListProxyStyle(s));
  }
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

ModListViewActions& ModListView::actions() const
{
  return *m_actions;
}

std::optional<unsigned int> ModListView::nextMod(unsigned int modIndex) const
{
  const QModelIndex start = indexModelToView(m_core->modList()->index(modIndex, 0));

  auto index = start;

  for (;;) {
    index = nextIndex(index);

    if (index == start || !index.isValid()) {
      // wrapped around, give up
      break;
    }

    modIndex = index.data(ModList::IndexRole).toInt();

    ModInfo::Ptr mod = ModInfo::getByIndex(modIndex);

    // skip overwrite and backups and separators
    if (mod->hasFlag(ModInfo::FLAG_OVERWRITE) ||
      mod->hasFlag(ModInfo::FLAG_BACKUP) ||
      mod->hasFlag(ModInfo::FLAG_SEPARATOR)) {
      continue;
    }

    return modIndex;
  }

  return {};
}

std::optional<unsigned int> ModListView::prevMod(unsigned int  modIndex) const
{
  const QModelIndex start = indexModelToView(m_core->modList()->index(modIndex, 0));

  auto index = start;

  for (;;) {
    index = prevIndex(index);

    if (index == start || !index.isValid()) {
      // wrapped around, give up
      break;
    }

    modIndex = index.data(ModList::IndexRole).toInt();

    // skip overwrite and backups and separators
    ModInfo::Ptr mod = ModInfo::getByIndex(modIndex);

    if (mod->hasFlag(ModInfo::FLAG_OVERWRITE) ||
      mod->hasFlag(ModInfo::FLAG_BACKUP) ||
      mod->hasFlag(ModInfo::FLAG_SEPARATOR)) {
      continue;
    }

    return modIndex;
  }

  return {};
}

void ModListView::enableAllVisible()
{
  m_core->modList()->setActive(indexViewToModel(allIndex(model())), true);
}

void ModListView::disableAllVisible()
{
  m_core->modList()->setActive(indexViewToModel(allIndex(model())), false);
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

QModelIndexList ModListView::indexModelToView(const QModelIndexList& index) const
{
  QModelIndexList result;
  for (auto& idx : index) {
    result.append(indexModelToView(idx));
  }
  return result;
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

QModelIndexList ModListView::indexViewToModel(const QModelIndexList& index) const
{
  QModelIndexList result;
  for (auto& idx : index) {
    result.append(indexViewToModel(idx));
  }
  return result;
}

QModelIndex ModListView::nextIndex(const QModelIndex& index) const
{
  auto* model = index.model();

  if (model->rowCount(index) > 0) {
    return model->index(0, index.column(), index);
  }

  if (index.parent().isValid()) {
    if (index.row() + 1 < model->rowCount(index.parent())) {
      return index.model()->index(index.row() + 1, index.column(), index.parent());
    }
    else {
      return index.model()->index((index.parent().row() + 1) % model->rowCount(index.parent().parent()), index.column(), index.parent().parent());;
    }
  }
  else {
    return index.model()->index((index.row() + 1) % model->rowCount(index.parent()), index.column(), index.parent());
  }
}

QModelIndex ModListView::prevIndex(const QModelIndex& index) const
{
  if (index.row() == 0 && index.parent().isValid()) {
    return index.parent();
  }

  auto* model = index.model();
  auto prev = model->index((index.row() - 1) % model->rowCount(index.parent()), index.column(), index.parent());

  if (model->rowCount(prev) > 0) {
    return model->index(model->rowCount(prev) - 1, index.column(), prev);
  }

  return prev;
}

QModelIndexList ModListView::allIndex(
  const QAbstractItemModel* model, int column, const QModelIndex& parent) const
{
  QModelIndexList index;
  for (std::size_t i = 0; i < model->rowCount(parent); ++i) {
    index.append(model->index(i, column, parent));
    index.append(allIndex(model, column, index.back()));
  }
  return index;
}

std::pair<QModelIndex, QModelIndexList> ModListView::selected() const
{
  return { indexViewToModel(currentIndex()), indexViewToModel(selectionModel()->selectedRows()) };
}

void ModListView::setSelected(const QModelIndex& current, const QModelIndexList& selected)
{
  // reset the selection and the index
  setCurrentIndex(indexModelToView(current));
  for (auto idx : selected) {
    selectionModel()->select(indexModelToView(idx), QItemSelectionModel::Select | QItemSelectionModel::Rows);
  }
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
  // expand separator whose priority has changed
  for (auto index : indices) {
    auto idx = indexModelToView(m_core->modList()->index(index, 0));
    if (hasCollapsibleSeparators() && model()->hasChildren(idx)) {
      expand(idx);
    }
    if (idx.parent().isValid()) {
      expand(idx.parent());
    }
  }

  for (unsigned int i = 0; i < m_core->currentProfile()->numMods(); ++i) {
    int priority = m_core->currentProfile()->getModPriority(i);
    if (m_core->currentProfile()->modEnabled(i)) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(i);
      // priorities in the directory structure are one higher because data is 0
      m_core->directoryStructure()->getOriginByName(MOBase::ToWString(modInfo->internalName())).setPriority(priority + 1);
    }
  }
  m_core->refreshBSAList();
  m_core->currentProfile()->writeModlist();
  m_core->directoryStructure()->getFileRegister()->sortOrigins();

  { // refresh selection
    QModelIndex current = currentIndex();
    if (current.isValid()) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(current.data(ModList::IndexRole).toInt());
      // clear caches on all mods conflicting with the moved mod
      for (int i : modInfo->getModOverwrite()) {
        ModInfo::getByIndex(i)->clearCaches();
      }
      for (int i : modInfo->getModOverwritten()) {
        ModInfo::getByIndex(i)->clearCaches();
      }
      for (int i : modInfo->getModArchiveOverwrite()) {
        ModInfo::getByIndex(i)->clearCaches();
      }
      for (int i : modInfo->getModArchiveOverwritten()) {
        ModInfo::getByIndex(i)->clearCaches();
      }
      for (int i : modInfo->getModArchiveLooseOverwrite()) {
        ModInfo::getByIndex(i)->clearCaches();
      }
      for (int i : modInfo->getModArchiveLooseOverwritten()) {
        ModInfo::getByIndex(i)->clearCaches();
      }
      // update conflict check on the moved mod
      modInfo->doConflictCheck();
      m_core->modList()->setOverwriteMarkers(modInfo->getModOverwrite(), modInfo->getModOverwritten());
      m_core->modList()->setArchiveOverwriteMarkers(modInfo->getModArchiveOverwrite(), modInfo->getModArchiveOverwritten());
      m_core->modList()->setArchiveLooseOverwriteMarkers(modInfo->getModArchiveLooseOverwrite(), modInfo->getModArchiveLooseOverwritten());
      verticalScrollBar()->repaint();
    }
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
  selectionModel()->select(qIndex, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
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
  TimeThis tt("updateModCount");

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

void ModListView::refreshFilters()
{
  auto [current, sourceRows] = selected();

  int idxRow = currentIndex().row();
  QVariant currentIndexName = model()->index(idxRow, 0).data();
  setCurrentIndex(QModelIndex());

  m_filters->refresh();

  setSelected(current, sourceRows);
}

void ModListView::onExternalFolderDropped(const QUrl& url, int priority)
{
  QFileInfo fileInfo(url.toLocalFile());

  GuessedValue<QString> name;
  name.setFilter(&fixDirectoryName);
  name.update(fileInfo.fileName(), GUESS_PRESET);

  do {
    bool ok;
    name.update(QInputDialog::getText(this, tr("Copy Folder..."),
      tr("This will copy the content of %1 to a new mod.\n"
        "Please enter the name:").arg(fileInfo.fileName()), QLineEdit::Normal, name, &ok),
      GUESS_USER);
    if (!ok) {
      return;
    }
  } while (name->isEmpty());

  if (m_core->modList()->getMod(name) != nullptr) {
    reportError(tr("A mod with this name already exists."));
    return;
  }

  IModInterface* newMod = m_core->createMod(name);
  if (!newMod) {
    return;
  }

  if (!copyDir(fileInfo.absoluteFilePath(), newMod->absolutePath(), true)) {
    return;
  }

  m_core->refresh();

  if (priority != -1) {
    m_core->modList()->changeModPriority(ModInfo::getIndex(name), priority);
  }
}

bool ModListView::moveSelection(int key)
{
  auto [cindex, sourceRows] = selected();

  int offset = key == Qt::Key_Up ? -1 : 1;
  if (m_sortProxy->sortOrder() == Qt::DescendingOrder) {
    offset = -offset;
  }

  m_core->modList()->shiftModsPriority(sourceRows, offset);

  // reset the selection and the index
  setSelected(cindex, sourceRows);

  return true;
}

bool ModListView::removeSelection()
{
  m_actions->removeMods(indexViewToModel(selectionModel()->selectedRows()));
  return true;
}

bool ModListView::toggleSelectionState()
{
  if (!selectionModel()->hasSelection()) {
    return true;
  }
  return m_core->modList()->toggleState(indexViewToModel(selectionModel()->selectedRows()));
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
  else if (m_core->settings().interface().collapsibleSeparators()
    && m_sortProxy->sortColumn() == ModList::COL_PRIORITY
    && m_sortProxy->sortOrder() == Qt::AscendingOrder) {
    m_sortProxy->setSourceModel(m_byPriorityProxy);
    m_byPriorityProxy->refresh();
  }
  else {
    m_sortProxy->setSourceModel(m_core->modList());
  }
}

void ModListView::setup(OrganizerCore& core, CategoryFactory& factory, MainWindow* mw, Ui::MainWindow* mwui)
{
  // attributes
  m_core = &core;
  m_filters.reset(new FilterList(mwui, core, factory));
  m_categories = &factory;
  m_actions = new ModListViewActions(core, *m_filters, factory, mw, this);
  ui = { mwui->groupCombo, mwui->activeModsCounter, mwui->modFilterEdit, mwui->currentCategoryLabel, mwui->clearFiltersButton };

  connect(m_core, &OrganizerCore::modInstalled, this, &ModListView::onModInstalled);
  connect(core.modList(), &ModList::modPrioritiesChanged, this, &ModListView::onModPrioritiesChanged);
  connect(core.modList(), &ModList::clearOverwrite, m_actions, &ModListViewActions::clearOverwrite);

  m_byPriorityProxy = new ModListByPriorityProxy(core.currentProfile(), core, this);
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

  // update the proxy when changing the sort column/direction and the group
  connect(m_sortProxy, &QAbstractItemModel::layoutAboutToBeChanged, [this](auto&& parents, auto&& hint) {
      if (hint == QAbstractItemModel::VerticalSortHint) {
        updateGroupByProxy(-1);
      }
    });
  connect(ui.groupBy, QOverload<int>::of(&QComboBox::currentIndexChanged), [=](int index) {
    updateGroupByProxy(index);
    onModFilterActive(m_sortProxy->isFilterActive());
    });
  sortByColumn(ModList::COL_PRIORITY, Qt::AscendingOrder);

  connect(this, &ModListView::dragEntered, core.modList(), &ModList::onDragEnter);
  connect(this, &ModListView::dropEntered, m_byPriorityProxy, &ModListByPriorityProxy::onDropEnter);

  connect(model(), &QAbstractItemModel::layoutChanged, this, &ModListView::updateModCount);

  connect(header(), &QHeaderView::sortIndicatorChanged, [=](int, Qt::SortOrder) { verticalScrollBar()->repaint(); });
  connect(header(), &QHeaderView::sectionResized, [=](int logicalIndex, int oldSize, int newSize) {
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

  connect(m_core->modList(), &ModList::downloadArchiveDropped, [=](int row, int priority) {
    m_core->installDownload(row, priority);
  });
  connect(m_core->modList(), &ModList::externalArchiveDropped, [=](const QUrl& url, int priority) {
    m_core->installArchive(url.toLocalFile(), priority, false, nullptr);
  });
  connect(m_core->modList(), &ModList::externalFolderDropped, this, &ModListView::onExternalFolderDropped);

  connect(selectionModel(), &QItemSelectionModel::selectionChanged, this, &ModListView::onSelectionChanged);

  // filters
  connect(m_sortProxy, &ModListSortProxy::filterActive, this, &ModListView::onModFilterActive);
  connect(m_filters.get(), &FilterList::criteriaChanged, [=](auto&& v) { onFiltersCriteria(v); });
  connect(m_filters.get(), &FilterList::optionsChanged, [=](auto&& mode, auto&& sep) { setFilterOptions(mode, sep); });
  connect(ui.filter, &QLineEdit::textChanged, m_sortProxy, &ModListSortProxy::updateFilter);
  connect(ui.clearFilters, &QPushButton::clicked, [=]() {
      ui.filter->clear();
      m_filters->clearSelection();
    });
  connect(m_sortProxy, &ModListSortProxy::filterInvalidated, [=]() {
    if (hasCollapsibleSeparators()) {
      m_byPriorityProxy->refreshExpandedItems();
    }
  });
}

void ModListView::restoreState(const Settings& s)
{
  s.geometry().restoreState(header());

  s.widgets().restoreIndex(ui.groupBy);
  s.widgets().restoreTreeState(this);

  m_filters->restoreState(s);
}

void ModListView::saveState(Settings& s) const
{
  s.geometry().saveState(header());

  s.widgets().saveIndex(ui.groupBy);
  s.widgets().saveTreeState(this);

  m_filters->saveState(s);
}

void ModListView::setModel(QAbstractItemModel* model)
{
  QTreeView::setModel(model);
  setVerticalScrollBar(new ViewMarkingScrollBar(model, this));
}

QRect ModListView::visualRect(const QModelIndex& index) const
{
  QRect rect = QTreeView::visualRect(index);
  if (hasCollapsibleSeparators()) {
    if (index.isValid() && !index.model()->hasChildren(index) && index.parent().isValid()) {
      auto parentIndex = index.parent().data(ModList::IndexRole).toInt();
      if (ModInfo::getByIndex(parentIndex)->isSeparator()) {
        rect.adjust(-indentation(), 0, 0, 0);
      }
    }
  }
  return rect;
}

QModelIndexList ModListView::selectedIndexes() const
{
  return m_inDragMoveEvent ? QModelIndexList() : QTreeView::selectedIndexes();
}

void ModListView::onCustomContextMenuRequested(const QPoint& pos)
{
  try {
    QModelIndex contextIdx = indexViewToModel(indexAt(pos));

    if (!contextIdx.isValid()) {
      // no selection
      ModListGlobalContextMenu(*m_core, this).exec(viewport()->mapToGlobal(pos));
    }
    else {
      ModListContextMenu(contextIdx, *m_core, *m_categories, this).exec(viewport()->mapToGlobal(pos));
    }
  }
  catch (const std::exception& e) {
    reportError(tr("Exception: ").arg(e.what()));
  }
  catch (...) {
    reportError(tr("Unknown exception"));
  }
}

void ModListView::onDoubleClicked(const QModelIndex& index)
{
  if (!index.isValid()) {
    return;
  }

  if (m_core->modList()->timeElapsedSinceLastChecked() <= QApplication::doubleClickInterval()) {
    // don't interpret double click if we only just checked a mod
    return;
  }

  bool indexOk = false;
  int modIndex = index.data(ModList::IndexRole).toInt(&indexOk);

  if (!indexOk || modIndex < 0 || modIndex >= ModInfo::getNumMods()) {
    return;
  }

  ModInfo::Ptr modInfo = ModInfo::getByIndex(modIndex);

  Qt::KeyboardModifiers modifiers = QApplication::queryKeyboardModifiers();
  if (modifiers.testFlag(Qt::ControlModifier)) {
    try {
      shell::Explore(modInfo->absolutePath());

      // workaround to cancel the editor that might have opened because of
      // selection-click
      closePersistentEditor(index);
    }
    catch (const std::exception& e) {
      reportError(e.what());
    }
  }
  else if (modifiers.testFlag(Qt::ShiftModifier)) {
    try {
      QModelIndex idx = m_core->modList()->index(modIndex, 0);
      actions().visitNexusOrWebPage({ idx });
      closePersistentEditor(index);
    }
    catch (const std::exception& e) {
      reportError(e.what());
    }
  }
  else if (hasCollapsibleSeparators() && modInfo->isSeparator()) {
    setExpanded(index, !isExpanded(index));
  }
  else {
    try {
      auto tab = ModInfoTabIDs::None;

      switch (index.column()) {
      case ModList::COL_NOTES: tab = ModInfoTabIDs::Notes; break;
      case ModList::COL_VERSION: tab = ModInfoTabIDs::Nexus; break;
      case ModList::COL_MODID: tab = ModInfoTabIDs::Nexus; break;
      case ModList::COL_GAME: tab = ModInfoTabIDs::Nexus; break;
      case ModList::COL_CATEGORY: tab = ModInfoTabIDs::Categories; break;
      case ModList::COL_CONFLICTFLAGS: tab = ModInfoTabIDs::Conflicts; break;
      }

      actions().displayModInformation(modIndex, tab);
      // workaround to cancel the editor that might have opened because of
      // selection-click
      closePersistentEditor(index);
    }
    catch (const std::exception& e) {
      reportError(e.what());
    }
  }
}

void ModListView::onSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
  if (hasCollapsibleSeparators()) {
    for (auto& idx : selected.indexes()) {
      if (idx.parent().isValid() && !isExpanded(idx.parent())) {
        setExpanded(idx.parent(), true);
      }
    }
  }

  if (selected.count()) {
    auto index = selected.indexes().last();
    ModInfo::Ptr selectedMod = ModInfo::getByIndex(index.data(ModList::IndexRole).toInt());
    m_core->modList()->setOverwriteMarkers(selectedMod->getModOverwrite(), selectedMod->getModOverwritten());
    m_core->modList()->setArchiveOverwriteMarkers(selectedMod->getModArchiveOverwrite(), selectedMod->getModArchiveOverwritten());
    m_core->modList()->setArchiveLooseOverwriteMarkers(selectedMod->getModArchiveLooseOverwrite(), selectedMod->getModArchiveLooseOverwritten());
  }
  else {
    m_core->modList()->setOverwriteMarkers({}, {});
    m_core->modList()->setArchiveOverwriteMarkers({}, {});
    m_core->modList()->setArchiveLooseOverwriteMarkers({}, {});
  }
  verticalScrollBar()->repaint();

}

void ModListView::onFiltersCriteria(const std::vector<ModListSortProxy::Criteria>& criteria)
{
  setFilterCriteria(criteria);

  QString label = "?";

  if (criteria.empty()) {
    label = "";
  }
  else if (criteria.size() == 1) {
    const auto& c = criteria[0];

    if (c.type == ModListSortProxy::TypeContent) {
      const auto* content = m_core->modDataContents().findById(c.id);
      label = content ? content->name() : QString();
    }
    else {
      label = m_categories->getCategoryNameByID(c.id);
    }

    if (label.isEmpty()) {
      log::error("category {}:{} not found", c.type, c.id);
    }
  }
  else {
    label = tr("<Multiple>");
  }

  ui.currentCategory->setText(label);
}

void ModListView::dragEnterEvent(QDragEnterEvent* event)
{
  emit dragEntered(event->mimeData());
  QTreeView::dragEnterEvent(event);


  ModListDropInfo dropInfo(event->mimeData(), *m_core);

  if (dropInfo.isValid() && !dropInfo.isLocalFileDrop()
    && sortColumn() != ModList::COL_PRIORITY) {
    log::warn("Drag&Drop is only supported when sorting by priority.");
  }
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

bool ModListView::event(QEvent* event)
{
  Profile* profile = m_core->currentProfile();
  if (event->type() == QEvent::KeyPress && profile) {
    QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

    if (keyEvent->modifiers() == Qt::ControlModifier
      && sortColumn() == ModList::COL_PRIORITY
      && (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down)) {
      return moveSelection(keyEvent->key());
    }
    else if (keyEvent->key() == Qt::Key_Delete) {
      return removeSelection();
    }
    else if (keyEvent->key() == Qt::Key_Space) {
      return toggleSelectionState();
    }
    return QTreeView::event(event);
  }
  return QTreeView::event(event);
}
