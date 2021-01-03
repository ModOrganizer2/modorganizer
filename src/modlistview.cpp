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
#include "copyeventfilter.h"
#include "shared/fileentry.h"
#include "shared/directoryentry.h"
#include "shared/filesorigin.h"
#include "mainwindow.h"
#include "modelutils.h"

using namespace MOBase;
using namespace MOShared;

// delegate to remove indentation for mods when using collapsible
// separator
//
// the delegate works by removing the indentation of the child items
// before drawing, but unfortunately this normally breaks event
// handling (e.g. checkbox, edit, etc.), so we also need to override
// the visualRect() function from the mod list view.
//
class ModListStyledItemDelegated : public QStyledItemDelegate
{
  ModListView* m_view;

public:

  ModListStyledItemDelegated(ModListView* view) :
    QStyledItemDelegate(view), m_view(view) { }

  void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override
  {
    // the parent version always overwrite the background brush, so
    // we need to save it and restore it
    auto backgroundColor = option->backgroundBrush.color();
    QStyledItemDelegate::initStyleOption(option, index);

    if (backgroundColor.isValid()) {
      option->backgroundBrush = backgroundColor;
    }
  }

  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
  {
    QStyleOptionViewItem opt(option);

    // remove items indentaiton when using collapsible separators
    if (index.column() == 0 && m_view->hasCollapsibleSeparators()) {
      if (!index.model()->hasChildren(index) && index.parent().isValid()) {
        auto parentIndex = index.parent().data(ModList::IndexRole).toInt();
        if (ModInfo::getByIndex(parentIndex)->isSeparator()) {
          opt.rect.adjust(-m_view->indentation(), 0, 0, 0);
        }
      }
    }

    // compute required color from children, otherwise fallback to the
    // color from the model, and draw the background here
    auto color = m_view->markerColor(index);
    if (!color.isValid()) {
      color = index.data(Qt::BackgroundRole).value<QColor>();
    }
    else {
      // disable alternating row if the color is from the children
      opt.features &= ~QStyleOptionViewItem::Alternate;
    }
    opt.backgroundBrush = color;

    // compute ideal foreground color for some rows
    if (color.isValid()) {
      if ((index.column() == ModList::COL_NAME
        && ModInfo::getByIndex(index.data(ModList::IndexRole).toInt())->isSeparator())
        || index.column() == ModList::COL_NOTES) {
        opt.palette.setBrush(QPalette::Text, ColorSettings::idealTextColor(color));
      }
    }

    QStyledItemDelegate::paint(painter, opt, index);
  }
};

class ModListViewMarkingScrollBar : public ViewMarkingScrollBar {
  ModListView* m_view;
public:
  ModListViewMarkingScrollBar(ModListView* view) :
    ViewMarkingScrollBar(view, ModList::ScrollMarkRole), m_view(view) { }


  QColor color(const QModelIndex& index) const override
  {
    auto color = m_view->markerColor(index);
    if (!color.isValid()) {
      color = ViewMarkingScrollBar::color(index);
    }
    return color;
  }

};

ModListView::ModListView(QWidget* parent)
  : QTreeView(parent)
  , m_core(nullptr)
  , m_sortProxy(nullptr)
  , m_byPriorityProxy(nullptr)
  , m_byCategoryProxy(nullptr)
  , m_byNexusIdProxy(nullptr)
  , m_markers{ {}, {}, {}, {}, {}, {} }
  , m_scrollbar(new ModListViewMarkingScrollBar(this))
{
  setVerticalScrollBar(m_scrollbar);
  MOBase::setCustomizableColumns(this);
  setAutoExpandDelay(1000);

  setItemDelegate(new ModListStyledItemDelegated(this));

  connect(this, &ModListView::doubleClicked, this, &ModListView::onDoubleClicked);
  connect(this, &ModListView::customContextMenuRequested, this, &ModListView::onCustomContextMenuRequested);

  installEventFilter(new CopyEventFilter(this, [=](auto& index) {
    QVariant mIndex = index.data(ModList::IndexRole);
    QString name = index.data(Qt::DisplayRole).toString();
    if (mIndex.isValid() && hasCollapsibleSeparators()) {
      ModInfo::Ptr info = ModInfo::getByIndex(mIndex.toInt());
      if (info->isSeparator()) {
        name = "[" + name + "]";
      }
    }
    else if (model()->hasChildren(index)) {
      name = "[" + name + "]";
    }
    return name;
  }));
}

void ModListView::refresh()
{
  updateGroupByProxy(-1);
}

void ModListView::setProfile(Profile* profile)
{
  m_sortProxy->setProfile(profile);
  m_byPriorityProxy->setProfile(profile);
}

bool ModListView::hasCollapsibleSeparators() const
{
  return groupByMode() == GroupByMode::SEPARATOR;
}

int ModListView::sortColumn() const
{
  return m_sortProxy ? m_sortProxy->sortColumn() : -1;
}

ModListView::GroupByMode ModListView::groupByMode() const
{
  if (m_sortProxy == nullptr) {
    return GroupByMode::NONE;
  }
  else if (m_sortProxy->sourceModel() == m_byPriorityProxy) {
    return GroupByMode::SEPARATOR;
  }
  else if (m_sortProxy->sourceModel() == m_byCategoryProxy) {
    return GroupByMode::CATEGORY;
  }
  else if (m_sortProxy->sourceModel() == m_byNexusIdProxy) {
    return GroupByMode::NEXUS_ID;
  }
  else {
    return GroupByMode::NONE;
  }
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
  m_core->modList()->setActive(indexViewToModel(flatIndex(model())), true);
}

void ModListView::disableAllVisible()
{
  m_core->modList()->setActive(indexViewToModel(flatIndex(model())), false);
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
  return ::indexModelToView(index, this);
}

QModelIndexList ModListView::indexModelToView(const QModelIndexList& index) const
{
  return ::indexModelToView(index, this);
}

QModelIndex ModListView::indexViewToModel(const QModelIndex& index) const
{
  return ::indexViewToModel(index, m_core->modList());
}

QModelIndexList ModListView::indexViewToModel(const QModelIndexList& index) const
{
  return ::indexViewToModel(index, m_core->modList());
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

std::pair<QModelIndex, QModelIndexList> ModListView::selected() const
{
  return { indexViewToModel(currentIndex()), indexViewToModel(selectionModel()->selectedRows()) };
}

void ModListView::setSelected(const QModelIndex& current, const QModelIndexList& selected)
{
  setCurrentIndex(indexModelToView(current));
  for (auto idx : selected) {
    selectionModel()->select(indexModelToView(idx), QItemSelectionModel::Select | QItemSelectionModel::Rows);
  }
}

void ModListView::expandItem(const QModelIndex& index)
{
  // the group proxy (QtGroupingProxy and ModListByPriorityProxy) emit
  // those with their own index, so we need to do this manually
  if (index.model() == m_sortProxy->sourceModel()) {
    setExpanded(m_sortProxy->mapFromSource(index), true);
  }
  else if (index.model() == model()) {
    setExpanded(index, true);
  }
}

void ModListView::onModPrioritiesChanged(const QModelIndexList& indices)
{
  // expand separator whose priority has changed and parents
  for (auto index : indices) {
    auto idx = indexModelToView(index);
    if (hasCollapsibleSeparators() && model()->hasChildren(idx)) {
      setExpanded(idx, true);
    }
    if (idx.parent().isValid()) {
      setExpanded(idx.parent(), true);
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
      setOverwriteMarkers(modInfo);
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
    setExpanded(qIndex, true);
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

  // TODO: this is currently a silent copy, which can take some time, but there is
  // no clean method to do this in uibase
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
  }
  else {
    m_sortProxy->setSourceModel(m_core->modList());
  }

  if (hasCollapsibleSeparators()) {
    ui.filterSeparators->setCurrentIndex(ModListSortProxy::SeparatorFilter);
    m_byPriorityProxy->refresh();
    ui.filterSeparators->setEnabled(false);
  }
  else {
    ui.filterSeparators->setEnabled(true);
  }
}

void ModListView::setup(OrganizerCore& core, CategoryFactory& factory, MainWindow* mw, Ui::MainWindow* mwui)
{
  // attributes
  m_core = &core;
  m_filters.reset(new FilterList(mwui, core, factory));
  m_categories = &factory;
  m_actions = new ModListViewActions(core, *m_filters, factory, this, mwui->espList, mw);
  ui = {
    mwui->groupCombo, mwui->activeModsCounter, mwui->modFilterEdit,
    mwui->currentCategoryLabel, mwui->clearFiltersButton, mwui->filtersSeparators
  };

  connect(m_core, &OrganizerCore::modInstalled, [=](auto&& name) { onModInstalled(name); });
  connect(core.modList(), &ModList::modPrioritiesChanged, [=](auto&& indices) { onModPrioritiesChanged(indices); });
  connect(core.modList(), &ModList::clearOverwrite, [=] { m_actions->clearOverwrite(); });
  connect(core.modList(), &ModList::modStatesChanged, [=] { updateModCount(); });
  connect(core.modList(), &ModList::modelReset, [=] { clearOverwriteMarkers(); });

  m_byPriorityProxy = new ModListByPriorityProxy(core.currentProfile(), core, this);
  m_byPriorityProxy->setSourceModel(core.modList());
  connect(this, &QTreeView::expanded, [=](auto&& name) { m_byPriorityProxy->expanded(name); });
  connect(this, &QTreeView::collapsed, [=](auto&& name) { m_byPriorityProxy->collapsed(name); });
  connect(m_byPriorityProxy, &ModListByPriorityProxy::expandItem, [=](auto&& index) { expandItem(index); });

  m_byCategoryProxy = new QtGroupingProxy(core.modList(), QModelIndex(), ModList::COL_CATEGORY,
    ModList::GroupingRole, 0, ModList::AggrRole);
  connect(this, &QTreeView::expanded, m_byCategoryProxy, &QtGroupingProxy::expanded);
  connect(this, &QTreeView::collapsed, m_byCategoryProxy, &QtGroupingProxy::collapsed);
  connect(m_byCategoryProxy, &QtGroupingProxy::expandItem, this, &ModListView::expandItem);
  m_byNexusIdProxy = new QtGroupingProxy(core.modList(), QModelIndex(), ModList::COL_MODID,
    ModList::GroupingRole, QtGroupingProxy::FLAG_NOGROUPNAME | QtGroupingProxy::FLAG_NOSINGLE,
    ModList::AggrRole);
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

  GenericIconDelegate* contentDelegate = new GenericIconDelegate(this, ModList::ContentsRole, ModList::COL_CONTENT, 150);
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

  // highligth plugins
  connect(selectionModel(), &QItemSelectionModel::selectionChanged, [=](auto&& selected) {
    std::vector<unsigned int> modIndices;
    for (auto& idx : selectionModel()->selectedRows()) {
      modIndices.push_back(idx.data(ModList::IndexRole).toInt());
    }
    m_core->pluginList()->highlightPlugins(modIndices, *m_core->directoryStructure());
    mwui->espList->verticalScrollBar()->repaint();
  });

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
  s.widgets().restoreTreeExpandState(this);

  m_filters->restoreState(s);
}

void ModListView::saveState(Settings& s) const
{
  s.geometry().saveState(header());

  s.widgets().saveIndex(ui.groupBy);
  s.widgets().saveTreeExpandState(this);

  m_filters->saveState(s);
}

QRect ModListView::visualRect(const QModelIndex& index) const
{
  // this shift the visualRect() from QTreeView to match the new actual
  // zone after removing indentation (see the ModListStyledItemDelegated)
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

void ModListView::drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const
{
  // the branches are the small indicator left to the row (there are none in the default style, and
  // the VS dark style only has background for these)
  //
  // the branches are not shifted left with the visualRect() change and since MO2 uses stylesheet,
  // it is not possible to shift those in the proxy style so we have to shift it here.
  //
  QRect r(rect);
  if (hasCollapsibleSeparators() && index.parent().isValid()) {
    r.adjust(-indentation(), 0, 0 -indentation(), 0);
  }
  QTreeView::drawBranches(painter, r, index);
}

QModelIndexList ModListView::selectedIndexes() const
{
  // during drag&drop events, we fake the return value of selectedIndexes()
  // to allow drag&drop of a parent into its children
  //
  // this is only "active" during the actual dragXXXEvent and dropEvent method,
  // not during the whole drag&drop event
  //
  // selectedIndexes() is a protected method from QTreeView which is little
  // used so this should not break anything
  //
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

  const auto modifiers = QApplication::queryKeyboardModifiers();
  if (modifiers.testFlag(Qt::ControlModifier)) {
    try {
      shell::Explore(modInfo->absolutePath());
    }
    catch (const std::exception& e) {
      reportError(e.what());
    }
  }
  else if (modifiers.testFlag(Qt::ShiftModifier)) {
    try {
      actions().visitNexusOrWebPage({ indexViewToModel(index) });
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
    }
    catch (const std::exception& e) {
      reportError(e.what());
    }
  }

  // workaround to cancel the editor that might have opened because of
  // selection-click
  closePersistentEditor(index);
}

void ModListView::clearOverwriteMarkers()
{
  m_markers.overwrite.clear();
  m_markers.overwritten.clear();
  m_markers.archiveOverwrite.clear();
  m_markers.archiveOverwritten.clear();
  m_markers.archiveLooseOverwrite.clear();
  m_markers.archiveLooseOverwritten.clear();
}

void ModListView::setOverwriteMarkers(const std::set<unsigned int>& overwrite, const std::set<unsigned int>& overwritten)
{
  m_markers.overwrite = overwrite;
  m_markers.overwritten = overwritten;
}

void ModListView::setArchiveOverwriteMarkers(const std::set<unsigned int>& overwrite, const std::set<unsigned int>& overwritten)
{
  m_markers.archiveOverwrite = overwrite;
  m_markers.archiveOverwritten = overwritten;
}

void ModListView::setArchiveLooseOverwriteMarkers(const std::set<unsigned int>& overwrite, const std::set<unsigned int>& overwritten)
{
  m_markers.archiveLooseOverwrite = overwrite;
  m_markers.archiveLooseOverwritten = overwritten;
}

void ModListView::setOverwriteMarkers(ModInfo::Ptr mod)
{
  if (mod) {
    setOverwriteMarkers(mod->getModOverwrite(), mod->getModOverwritten());
    setArchiveOverwriteMarkers(mod->getModArchiveOverwrite(), mod->getModArchiveOverwritten());
    setArchiveLooseOverwriteMarkers(mod->getModArchiveLooseOverwrite(), mod->getModArchiveLooseOverwritten());
  }
  else {
    setOverwriteMarkers({}, {});
    setArchiveOverwriteMarkers({}, {});
    setArchiveLooseOverwriteMarkers({}, {});
  }
  dataChanged(model()->index(0, 0), model()->index(model()->rowCount(), model()->columnCount()));
  verticalScrollBar()->repaint();
}

void ModListView::setHighlightedMods(const std::vector<unsigned int>& pluginIndices)
{
  m_markers.highlight.clear();
  auto& directoryEntry = *m_core->directoryStructure();
  for (auto idx : pluginIndices) {
    QString pluginName = m_core->pluginList()->getName(idx);

    const MOShared::FileEntryPtr fileEntry = directoryEntry.findFile(pluginName.toStdWString());
    if (fileEntry.get() != nullptr) {
      QString originName = QString::fromStdWString(directoryEntry.getOriginByID(fileEntry->getOrigin()).getName());
      const auto index = ModInfo::getIndex(originName);
      if (index != UINT_MAX) {
        m_markers.highlight.insert(index);
      }
    }
  }
  dataChanged(model()->index(0, 0), model()->index(model()->rowCount(), model()->columnCount()));
  verticalScrollBar()->repaint();
}

QColor ModListView::markerColor(const QModelIndex& index) const
{
  unsigned int modIndex = index.data(ModList::IndexRole).toInt();
  bool highligth = m_markers.highlight.find(modIndex) != m_markers.highlight.end();
  bool overwrite = m_markers.overwrite.find(modIndex) != m_markers.overwrite.end();
  bool archiveOverwrite = m_markers.archiveOverwrite.find(modIndex) != m_markers.archiveOverwrite.end();
  bool archiveLooseOverwrite = m_markers.archiveOverwritten.find(modIndex) != m_markers.archiveOverwritten.end();
  bool overwritten = m_markers.overwritten.find(modIndex) != m_markers.overwritten.end();
  bool archiveOverwritten = m_markers.archiveOverwritten.find(modIndex) != m_markers.archiveOverwritten.end();
  bool archiveLooseOverwritten = m_markers.archiveLooseOverwritten.find(modIndex) != m_markers.archiveLooseOverwritten.end();

  if (highligth) {
    return Settings::instance().colors().modlistContainsPlugin();
  }
  else if (overwritten || archiveLooseOverwritten) {
    return Settings::instance().colors().modlistOverwritingLoose();
  }
  else if (overwrite || archiveLooseOverwrite) {
    return Settings::instance().colors().modlistOverwrittenLoose();
  }
  else if (archiveOverwritten) {
    return Settings::instance().colors().modlistOverwritingArchive();
  }
  else if (archiveOverwrite) {
    return Settings::instance().colors().modlistOverwrittenArchive();
  }

  // collapsed separator
  auto rowIndex = index.sibling(index.row(), 0);
  if (hasCollapsibleSeparators() && model()->hasChildren(rowIndex) && !isExpanded(rowIndex)) {

    std::vector<QColor> colors;
    for (int i = 0; i < model()->rowCount(rowIndex); ++i) {
      auto childColor = markerColor(model()->index(i, index.column(), rowIndex));
      if (childColor.isValid()) {
        colors.push_back(childColor);
      }
    }

    if (colors.empty()) {
      return QColor();
    }

    int r = 0, g = 0, b = 0, a = 0;
    for (auto& color : colors) {
      r += color.red();
      g += color.green();
      b += color.blue();
      a += color.alpha();
    }

    return QColor(r / colors.size(), g / colors.size(), b / colors.size(), a / colors.size());
  }

  return QColor();
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
    setOverwriteMarkers(selectedMod);
  }
  else {
    setOverwriteMarkers(nullptr);
  }

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
  // this event is used by the modlist to check if we are draggin
  // to a mod (local files) or to a priority (mods, downloads, external
  // files)
  emit dragEntered(event->mimeData());
  QTreeView::dragEnterEvent(event);

  // there is no drop event for invalid data since canDropMimeData
  // returns false, so we notify user on drag enter
  ModListDropInfo dropInfo(event->mimeData(), *m_core);

  if (dropInfo.isValid() && !dropInfo.isLocalFileDrop()
    && sortColumn() != ModList::COL_PRIORITY) {
    log::warn("Drag&Drop is only supported when sorting by priority.");
  }
}

void ModListView::dragMoveEvent(QDragMoveEvent* event)
{
  // this replace the openTimer from QTreeView to prevent
  // auto-collapse of items
  if (autoExpandDelay() >= 0) {
    m_openTimer.start(autoExpandDelay(), this);
  }

  // see selectedIndexes()
  m_inDragMoveEvent = true;
  QAbstractItemView::dragMoveEvent(event);
  m_inDragMoveEvent = false;
}

void ModListView::dropEvent(QDropEvent* event)
{
  // this event is used by the byPriorityProxy to know if allow
  // dropping mod between a separator and its first mod (there
  // is no way to deduce this except using dropIndicatorPosition())
  emit dropEntered(event->mimeData(), static_cast<DropPosition>(dropIndicatorPosition()));

  // see selectedIndexes()
  m_inDragMoveEvent = true;
  QTreeView::dropEvent(event);
  m_inDragMoveEvent = false;
}

void ModListView::timerEvent(QTimerEvent* event)
{
  // prevent auto-collapse, see dragMoveEvent()
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
  if (event->type() == QEvent::KeyPress
    && m_core->currentProfile()
    && selectionModel()->hasSelection()) {
    QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

    auto index = selectionModel()->currentIndex();

    if (keyEvent->modifiers() == Qt::ControlModifier) {
      // ctrl+enter open explorer
      if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
        if (selectionModel()->selectedRows().count() == 1) {
          m_actions->openExplorer({ indexViewToModel(index) });
          return true;
        }
      }
      // ctrl+up/down move selection
      else if (sortColumn() == ModList::COL_PRIORITY
        && (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down)) {
        return moveSelection(keyEvent->key());
      }
    }
    else if (keyEvent->modifiers() == Qt::ShiftModifier) {
      // shift+enter expand
      if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
        && selectionModel()->selectedRows().count() == 1) {
        if (model()->hasChildren(index)) {
          setExpanded(index, !isExpanded(index));
        }
        else if (index.parent().isValid()) {
          setExpanded(index.parent(), false);
          selectionModel()->select(index.parent(),
            QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
          setCurrentIndex(index.parent());
        }
      }
    }
    else {
      if (keyEvent->key() == Qt::Key_Delete) {
        return removeSelection();
      }
      else if (keyEvent->key() == Qt::Key_Space) {
        return toggleSelectionState();
      }
    }
    return QTreeView::event(event);
  }
  return QTreeView::event(event);
}
