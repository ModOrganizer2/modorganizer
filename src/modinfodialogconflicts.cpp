#include "modinfodialogconflicts.h"
#include "ui_modinfodialog.h"
#include "modinfodialog.h"
#include "utility.h"
#include "settings.h"
#include "organizercore.h"

using namespace MOShared;
using namespace MOBase;

// if there are more than 50 selected items in the conflict tree, don't bother
// checking whether menu items apply to them, just show all of them
const std::size_t max_small_selection = 50;

// in mainwindow.cpp
void setDefaultActivationActionForFile(QAction* open, QAction* preview);


class ConflictItem
{
public:
  ConflictItem(
    QString before, QString relativeName, QString after,
    FileEntry::Index index,  QString fileName,
    bool hasAltOrigins, QString altOrigin,  bool archive) :
      m_before(std::move(before)),
      m_relativeName(std::move(relativeName)),
      m_after(std::move(after)),
      m_index(index),
      m_fileName(std::move(fileName)),
      m_hasAltOrigins(hasAltOrigins),
      m_altOrigin(std::move(altOrigin)),
      m_isArchive(archive)
  {
  }

  const QString& before() const
  {
    return m_before;
  }

  const QString& relativeName() const
  {
    return m_relativeName;
  }

  const QString& after() const
  {
    return m_after;
  }

  const QString& fileName() const
  {
    return m_fileName;
  }

  const QString& altOrigin() const
  {
    return m_altOrigin;
  }

  bool hasAlts() const
  {
    return m_hasAltOrigins;
  }

  bool isArchive() const
  {
    return m_isArchive;
  }

  FileEntry::Index fileIndex() const
  {
    return m_index;
  }

  bool canHide() const
  {
    return canHideFile(isArchive(), fileName());
  }

  bool canUnhide() const
  {
    return canUnhideFile(isArchive(), fileName());
  }

  bool canRun() const
  {
    return canRunFile(isArchive(), fileName());
  }

  bool canOpen() const
  {
    return canOpenFile(isArchive(), fileName());
  }

  bool canPreview(PluginContainer& pluginContainer) const
  {
    return canPreviewFile(pluginContainer, isArchive(), fileName());
  }

  bool canExplore() const
  {
    return canExploreFile(isArchive(), fileName());
  }

private:
  QString m_before;
  QString m_relativeName;
  QString m_after;
  FileEntry::Index m_index;
  QString m_fileName;
  bool m_hasAltOrigins;
  QString m_altOrigin;
  bool m_isArchive;
};


class ConflictListModel : public QAbstractItemModel
{
public:
  struct Column
  {
    QString caption;
    const QString& (ConflictItem::*getText)() const;
  };

  ConflictListModel(QTreeView* tree, std::vector<Column> columns) :
    m_tree(tree), m_columns(std::move(columns)),
    m_sortColumn(-1), m_sortOrder(Qt::AscendingOrder)
  {
    m_tree->setModel(this);
  }

  void clear()
  {
    m_items.clear();
    endResetModel();
  }

  void reserve(std::size_t s)
  {
    m_items.reserve(s);
  }

  QModelIndex index(int row, int col, const QModelIndex& ={}) const override
  {
    return createIndex(row, col);
  }

  QModelIndex parent(const QModelIndex&) const override
  {
    return {};
  }

  int rowCount(const QModelIndex& parent={}) const override
  {
    if (parent.isValid()) {
      return 0;
    }

    return static_cast<int>(m_items.size());
  }

  int columnCount(const QModelIndex& ={}) const override
  {
    return static_cast<int>(m_columns.size());
  }

  QVariant data(const QModelIndex& index, int role) const override
  {
    if (role == Qt::DisplayRole || role == Qt::FontRole) {
      const auto row = index.row();
      if (row < 0) {
        return {};
      }

      const auto i = static_cast<std::size_t>(row);
      if (i >= m_items.size()) {
        return {};
      }

      const auto col = index.column();
      if (col < 0) {
        return {};
      }

      const auto c = static_cast<std::size_t>(col);
      if (c >= m_columns.size()) {
        return {};
      }

      const auto& item = m_items[i];

      if (role == Qt::DisplayRole) {
        return (item.*m_columns[c].getText)();
      } else if (role == Qt::FontRole) {
        if (item.isArchive()) {
          QFont f = m_tree->font();
          f.setItalic(true);
          return f;
        }
      }
    }

    return {};
  }

  QVariant headerData(int col, Qt::Orientation, int role) const
  {
    if (role == Qt::DisplayRole) {
      if (col < 0) {
        return {};
      }

      const auto i = static_cast<std::size_t>(col);
      if (i >= m_columns.size()) {
        return {};
      }

      return m_columns[i].caption;
    }

    return {};
  }

  void sort(int colIndex, Qt::SortOrder order=Qt::AscendingOrder)
  {
    m_sortColumn = colIndex;
    m_sortOrder = order;

    doSort();
    emit layoutChanged({}, QAbstractItemModel::VerticalSortHint);
  }

  void add(ConflictItem item)
  {
    m_items.emplace_back(std::move(item));
  }

  void finished()
  {
    endResetModel();
    doSort();
  }

  const ConflictItem* getItem(std::size_t row) const
  {
    if (row >= m_items.size()) {
      return nullptr;
    }

    return &m_items[row];
  }

private:
  QTreeView* m_tree;
  std::vector<Column> m_columns;
  std::vector<ConflictItem> m_items;
  int m_sortColumn;
  Qt::SortOrder m_sortOrder;

  void doSort()
  {
    if (m_items.empty()) {
      return;
    }

    if (m_sortColumn < 0) {
      return;
    }

    const auto c = static_cast<std::size_t>(m_sortColumn);
    if (c >= m_columns.size()) {
      return;
    }

    const auto& col = m_columns[c];

    // avoids branching on sort order while sorting
    auto sortAsc = [&](const auto& a, const auto& b) {
      return (naturalCompare((a.*col.getText)(), (b.*col.getText)()) < 0);
    };

    auto sortDesc = [&](const auto& a, const auto& b) {
      return (naturalCompare((a.*col.getText)(), (b.*col.getText)()) > 0);
    };

    if (m_sortOrder == Qt::AscendingOrder) {
      std::sort(m_items.begin(), m_items.end(), sortAsc);
    } else {
      std::sort(m_items.begin(), m_items.end(), sortDesc);
    }
  }
};


class OverwriteConflictListModel : public ConflictListModel
{
public:
  OverwriteConflictListModel(QTreeView* tree)
    : ConflictListModel(tree, {
        {tr("File"), &ConflictItem::relativeName},
        {tr("Overwritten Mods"), &ConflictItem::before}
      })
  {
  }
};


class OverwrittenConflictListModel : public ConflictListModel
{
public:
  OverwrittenConflictListModel(QTreeView* tree)
    : ConflictListModel(tree, {
        {tr("File"), &ConflictItem::relativeName},
        {tr("Providing Mod"), &ConflictItem::after}
      })
  {
  }
};


class NoConflictListModel : public ConflictListModel
{
public:
  NoConflictListModel(QTreeView* tree)
    : ConflictListModel(tree, {
        {tr("File"), &ConflictItem::relativeName}
      })
  {
  }
};


class AdvancedConflictListModel : public ConflictListModel
{
public:
  AdvancedConflictListModel(QTreeView* tree)
    : ConflictListModel(tree, {
        {tr("Overwrites"), &ConflictItem::before},
        {tr("File"), &ConflictItem::relativeName},
        {tr("Overwritten By"), &ConflictItem::after}
      })
  {
  }
};


std::size_t smallSelectionSize(const QTreeView* tree)
{
  const std::size_t too_many = std::numeric_limits<std::size_t>::max();

  std::size_t n = 0;
  const auto* sel = tree->selectionModel();

  for (const auto& range : sel->selection()) {
    n += range.height();

    if (n >= max_small_selection) {
      return too_many;
    }
  }

  return n;
}

template <class F>
void for_each_in_selection(QTreeView* tree, F&& f)
{
  const auto* sel = tree->selectionModel();
  const auto* model = dynamic_cast<ConflictListModel*>(tree->model());

  if (!model) {
    log::error("tree doesn't have a ConflictListModel");
    return;
  }

  for (const auto& range : sel->selection()) {
    // ranges are inclusive
    for (int row=range.top(); row<=range.bottom(); ++row) {
      if (auto* item=model->getItem(static_cast<std::size_t>(row))) {
        if (!f(item)) {
          return;
        }
      }
    }
  }
}


ConflictsTab::ConflictsTab(ModInfoDialogTabContext cx) :
  ModInfoDialogTab(cx), // don't move, cx is used again
  m_general(this, cx.ui, cx.core), m_advanced(this, cx.ui, cx.core)
{
  connect(
    &m_general, &GeneralConflictsTab::modOpen,
    [&](const QString& name){ emitModOpen(name); });

  connect(
    &m_advanced, &AdvancedConflictsTab::modOpen,
    [&](const QString& name){ emitModOpen(name); });
}

void ConflictsTab::update()
{
  setHasData(m_general.update());
  m_advanced.update();
}

void ConflictsTab::clear()
{
  m_general.clear();
  m_advanced.clear();
  setHasData(false);
}

void ConflictsTab::saveState(Settings& s)
{
  s.widgets().saveIndex(ui->tabConflictsTabs);

  m_general.saveState(s);
  m_advanced.saveState(s);
}

void ConflictsTab::restoreState(const Settings& s)
{
  s.widgets().restoreIndex(ui->tabConflictsTabs, 0);

  m_general.restoreState(s);
  m_advanced.restoreState(s);
}

bool ConflictsTab::canHandleUnmanaged() const
{
  return true;
}

void ConflictsTab::changeItemsVisibility(QTreeView* tree, bool visible)
{
  bool changed = false;
  bool stop = false;

  const auto n = smallSelectionSize(tree);

  // logging
  {
    const QString action = (visible ? "unhiding" : "hiding");

    QString files;
    if (n > max_small_selection)
      files = "a lot of";
    else
      files = QString("%1").arg(n);

    log::debug("{} {} conflict files", action, files);
  }

  QFlags<FileRenamer::RenameFlags> flags =
    (visible ? FileRenamer::UNHIDE : FileRenamer::HIDE);

  if (n > 1) {
    flags |= FileRenamer::MULTIPLE;
  }

  FileRenamer renamer(parentWidget(), flags);

  auto* model = dynamic_cast<ConflictListModel*>(tree->model());
  if (!model) {
    log::error("list doesn't have a ConflictListModel");
    return;
  }

  for_each_in_selection(tree, [&](const ConflictItem* item) {
    if (stop) {
      return false;
    }

    auto result = FileRenamer::RESULT_CANCEL;

    if (visible) {
      if (!item->canUnhide()) {
        log::debug("cannot unhide {}, skipping", item->relativeName());
        return true;
      }

      result = unhideFile(renamer, item->fileName());

    } else {
      if (!item->canHide()) {
        log::debug("cannot hide {}, skipping", item->relativeName());
        return true;
      }

      result = hideFile(renamer, item->fileName());
    }

    switch (result) {
      case FileRenamer::RESULT_OK: {
        // will trigger a refresh at the end
        changed = true;
        break;
      }

      case FileRenamer::RESULT_SKIP: {
        // nop
        break;
      }

      case FileRenamer::RESULT_CANCEL: {
        // stop right now, but make sure to refresh if needed
        stop = true;
        break;
      }
    }

    return true;
  });

  log::debug("{} conflict files done", (visible ? "unhiding" : "hiding"));

  if (changed) {
    log::debug("triggering refresh");

    if (origin()) {
      emitOriginModified();
    }

    update();
  }
}

void ConflictsTab::activateItems(QTreeView* tree)
{
  const auto tryPreview = core().settings().interface().doubleClicksOpenPreviews();

  // the menu item is only shown for a single selection, but handle all of them
  // in case this changes
  for_each_in_selection(tree, [&](const ConflictItem* item) {
    const auto path = item->fileName();

    if (tryPreview && canPreviewFile(plugin(), item->isArchive(), path)) {
      previewItem(item);
    } else {
      openItem(item);
    }

    return true;
  });
}

void ConflictsTab::openItems(QTreeView* tree)
{
  // the menu item is only shown for a single selection, but handle all of them
  // in case this changes
  for_each_in_selection(tree, [&](const ConflictItem* item) {
    openItem(item);
    return true;
  });
}

void ConflictsTab::openItem(const ConflictItem* item)
{
  core().processRunner()
    .setFromFile(parentWidget(), item->fileName())
    .setWaitForCompletion()
    .run();
}

void ConflictsTab::runItemsHooked(QTreeView* tree)
{
  // the menu item is only shown for a single selection, but handle all of them
  // in case this changes
  for_each_in_selection(tree, [&](const ConflictItem* item) {
    core().processRunner()
      .setFromFile(parentWidget(), item->fileName(), true)
      .setWaitForCompletion()
      .run();

    return true;
  });
}

void ConflictsTab::previewItems(QTreeView* tree)
{
  // the menu item is only shown for a single selection, but handle all of them
  // in case this changes
  for_each_in_selection(tree, [&](const ConflictItem* item) {
    previewItem(item);
    return true;
  });
}

void ConflictsTab::previewItem(const ConflictItem* item)
{
  core().previewFileWithAlternatives(parentWidget(), item->fileName());
}

void ConflictsTab::exploreItems(QTreeView* tree)
{
  // the menu item is only shown for a single selection, but handle all of them
  // in case this changes
  for_each_in_selection(tree, [&](const ConflictItem* item) {
    shell::Explore(item->fileName());
    return true;
  });
}

void ConflictsTab::showContextMenu(const QPoint &pos, QTreeView* tree)
{
  auto actions = createMenuActions(tree);

  QMenu menu;

  // open
  if (actions.open) {
    connect(actions.open, &QAction::triggered, [&]{
      openItems(tree);
    });

    menu.addAction(actions.open);
  }

  // run hooked
  if (actions.runHooked) {
    connect(actions.runHooked, &QAction::triggered, [&]{
      runItemsHooked(tree);
    });

    menu.addAction(actions.runHooked);
  }

  // preview
  if (actions.preview) {
    connect(actions.preview, &QAction::triggered, [&]{
      previewItems(tree);
    });

    menu.addAction(actions.preview);
  }

  // goto
  if (actions.gotoMenu) {
    menu.addMenu(actions.gotoMenu);

    for (auto* a : actions.gotoActions) {
      connect(a, &QAction::triggered, [&, name=a->text()]{
        emitModOpen(name);
        });

      actions.gotoMenu->addAction(a);
    }
  }

  // explore
  if (actions.explore) {
    connect(actions.explore, &QAction::triggered, [&]{
      exploreItems(tree);
    });

    menu.addAction(actions.explore);
  }

  menu.addSeparator();

  // hide
  if (actions.hide) {
    connect(actions.hide, &QAction::triggered, [&]{
      changeItemsVisibility(tree, false);
    });

    menu.addAction(actions.hide);
  }

  // unhide
  if (actions.unhide) {
    connect(actions.unhide, &QAction::triggered, [&]{
      changeItemsVisibility(tree, true);
    });

    menu.addAction(actions.unhide);
  }

  setDefaultActivationActionForFile(actions.open, actions.preview);

  if (!menu.isEmpty()) {
    menu.exec(tree->viewport()->mapToGlobal(pos));
  }
}

ConflictsTab::Actions ConflictsTab::createMenuActions(QTreeView* tree)
{
  if (tree->selectionModel()->selection().isEmpty()) {
    return {};
  }

  bool enableHide = true;
  bool enableUnhide = true;
  bool enableRun = true;
  bool enableOpen = true;
  bool enablePreview = true;
  bool enableExplore = true;
  bool enableGoto = true;

  const auto n = smallSelectionSize(tree);

  const auto* model = dynamic_cast<ConflictListModel*>(tree->model());
  if (!model) {
    log::error("tree doesn't have a ConflictListModel");
    return {};
  }

  if (n == 1) {
    // this is a single selection
    const auto* item = model->getItem(static_cast<std::size_t>(
      tree->selectionModel()->selectedRows()[0].row()));

    if (!item) {
      return {};
    }

    enableHide = item->canHide();
    enableUnhide = item->canUnhide();
    enableRun = item->canRun();
    enableOpen = item->canOpen();
    enablePreview = item->canPreview(plugin());
    enableExplore = item->canExplore();
    enableGoto = item->hasAlts();
  }
  else {
    // this is a multiple selection, don't show open/preview so users don't open
    // a thousand files
    enableRun = false;
    enableOpen = false;
    enablePreview = false;

    // can't explore multiple files
    enableExplore = false;

    // don't bother with this on multiple selection, at least for now
    enableGoto = false;

    if (n <= max_small_selection) {
      // if the number of selected items is low, checking them to accurately
      // show the menu items is worth it
      enableHide = false;
      enableUnhide = false;

      for_each_in_selection(tree, [&](const ConflictItem* item) {
        if (item->canHide()) {
          enableHide = true;
        }

        if (item->canUnhide()) {
          enableUnhide = true;
        }

        if (enableHide && enableUnhide && enableGoto) {
          // found all, no need to check more
          return false;
        }

        return true;
      });
    }
  }

  Actions actions;

  if (enableRun) {
    actions.open = new QAction(tr("&Execute"), parentWidget());
  } else if (enableOpen) {
    actions.open = new QAction(tr("&Open"), parentWidget());
    actions.runHooked = new QAction(tr("Open with &VFS"), parentWidget());
  }

  actions.preview = new QAction(tr("&Preview"), parentWidget());
  actions.preview->setEnabled(enablePreview);

  actions.gotoMenu = new QMenu(tr("&Go to..."), parentWidget());
  actions.gotoMenu->setEnabled(enableGoto);

  actions.explore = new QAction(tr("Open in &Explorer"), parentWidget());
  actions.explore->setEnabled(enableExplore);

  actions.hide = new QAction(tr("&Hide"), parentWidget());
  actions.hide->setEnabled(enableHide);

  // note that it is possible for hidden files to appear if they override other
  // hidden files from another mod
  actions.unhide = new QAction(tr("&Unhide"), parentWidget());
  actions.unhide->setEnabled(enableUnhide);

  if (enableGoto && n == 1) {
    const auto* item = model->getItem(static_cast<std::size_t>(
      tree->selectionModel()->selectedRows()[0].row()));

    actions.gotoActions = createGotoActions(item);
  }

  return actions;
}

std::vector<QAction*> ConflictsTab::createGotoActions(const ConflictItem* item)
{
  if (!origin()) {
    return {};
  }

  auto file = origin()->findFile(item->fileIndex());
  if (!file) {
    return {};
  }


  std::vector<QString> mods;
  const auto& ds = *core().directoryStructure();

  // add all alternatives
  for (const auto& alt : file->getAlternatives()) {
    const auto& o = ds.getOriginByID(alt.first);
    if (o.getID() != origin()->getID()) {
      mods.push_back(ToQString(o.getName()));
    }
  }

  // add the real origin if different from this mod
  const FilesOrigin& realOrigin = ds.getOriginByID(file->getOrigin());
  if (realOrigin.getID() != origin()->getID()) {
    mods.push_back(ToQString(realOrigin.getName()));
  }

  std::sort(mods.begin(), mods.end(), [](const auto& a, const auto& b) {
    return (QString::localeAwareCompare(a, b) < 0);
    });

  std::vector<QAction*> actions;

  for (const auto& name : mods) {
    actions.push_back(new QAction(name, parentWidget()));
  }

  return actions;
}


GeneralConflictsTab::GeneralConflictsTab(
  ConflictsTab* tab, Ui::ModInfoDialog* pui, OrganizerCore& oc) :
    m_tab(tab), ui(pui), m_core(oc),
    m_overwriteModel(new OverwriteConflictListModel(ui->overwriteTree)),
    m_overwrittenModel(new OverwrittenConflictListModel(ui->overwrittenTree)),
    m_noConflictModel(new NoConflictListModel(ui->noConflictTree))
{
  m_expanders.overwrite.set(ui->overwriteExpander, ui->overwriteTree, true);
  m_expanders.overwritten.set(ui->overwrittenExpander, ui->overwrittenTree, true);
  m_expanders.nonconflict.set(ui->noConflictExpander, ui->noConflictTree);

  QObject::connect(
    ui->overwriteTree, &QTreeView::doubleClicked,
    [&](auto&&){ m_tab->activateItems(ui->overwriteTree); });

  QObject::connect(
    ui->overwrittenTree, &QTreeView::doubleClicked,
    [&](auto&& item){ m_tab->activateItems(ui->overwrittenTree); });

  QObject::connect(
    ui->noConflictTree, &QTreeView::doubleClicked,
    [&](auto&& item){ m_tab->activateItems(ui->noConflictTree); });

  QObject::connect(
    ui->overwriteTree, &QTreeView::customContextMenuRequested,
    [&](const QPoint& p){ m_tab->showContextMenu(p, ui->overwriteTree); });

  QObject::connect(
    ui->overwrittenTree, &QTreeView::customContextMenuRequested,
    [&](const QPoint& p){ m_tab->showContextMenu(p, ui->overwrittenTree); });

  QObject::connect(
    ui->noConflictTree, &QTreeView::customContextMenuRequested,
    [&](const QPoint& p){ m_tab->showContextMenu(p, ui->noConflictTree); });
}

void GeneralConflictsTab::clear()
{
  m_overwriteModel->clear();
  m_overwrittenModel->clear();
  m_noConflictModel->clear();

  ui->overwriteCount->display(0);
  ui->overwrittenCount->display(0);
  ui->noConflictCount->display(0);
}

void GeneralConflictsTab::saveState(Settings& s)
{
  s.geometry().saveState(&m_expanders.overwrite);
  s.geometry().saveState(&m_expanders.overwritten);
  s.geometry().saveState(&m_expanders.nonconflict);
  s.geometry().saveState(ui->overwriteTree->header());
  s.geometry().saveState(ui->noConflictTree->header());
  s.geometry().saveState(ui->overwrittenTree->header());
}

void GeneralConflictsTab::restoreState(const Settings& s)
{
  s.geometry().restoreState(&m_expanders.overwrite);
  s.geometry().restoreState(&m_expanders.overwritten);
  s.geometry().restoreState(&m_expanders.nonconflict);
  s.geometry().restoreState(ui->overwriteTree->header());
  s.geometry().restoreState(ui->noConflictTree->header());
  s.geometry().restoreState(ui->overwrittenTree->header());
}

bool GeneralConflictsTab::update()
{
  clear();

  int numNonConflicting = 0;
  int numOverwrite = 0;
  int numOverwritten = 0;

  if (m_tab->origin() != nullptr) {
    const auto rootPath = m_tab->mod().absolutePath();

    for (const auto& file : m_tab->origin()->getFiles()) {
      // careful: these two strings are moved into createXItem() below
      QString relativeName = QDir::fromNativeSeparators(ToQString(file->getRelativePath()));
      QString fileName = rootPath + relativeName;

      bool archive = false;
      const int fileOrigin = file->getOrigin(archive);
      const auto& alternatives = file->getAlternatives();

      if (fileOrigin == m_tab->origin()->getID()) {
        if (!alternatives.empty()) {
          m_overwriteModel->add(createOverwriteItem(
              file->getIndex(), archive,
              std::move(fileName), std::move(relativeName), alternatives));

          ++numOverwrite;
        } else {
          // otherwise, put the file in the noconflict tree
            m_noConflictModel->add(createNoConflictItem(
            file->getIndex(), archive,
              std::move(fileName), std::move(relativeName)));

          ++numNonConflicting;
        }
      } else {
        m_overwrittenModel->add(createOverwrittenItem(
          file->getIndex(), fileOrigin, archive,
          std::move(fileName), std::move(relativeName)));

        ++numOverwritten;
      }
    }

    m_overwriteModel->finished();
    m_overwrittenModel->finished();
    m_noConflictModel->finished();
  }

  ui->overwriteCount->display(numOverwrite);
  ui->overwrittenCount->display(numOverwritten);
  ui->noConflictCount->display(numNonConflicting);

  return (numOverwrite > 0 || numOverwritten > 0);
}

ConflictItem GeneralConflictsTab::createOverwriteItem(
  FileEntry::Index index, bool archive, QString fileName, QString relativeName,
  const FileEntry::AlternativesVector& alternatives)
{
  const auto& ds = *m_core.directoryStructure();
  std::wstring altString;

  for (const auto& alt : alternatives) {
    if (!altString.empty()) {
      altString += L", ";
    }

    altString += ds.getOriginByID(alt.first).getName();
  }

  auto origin = ToQString(ds.getOriginByID(alternatives.back().first).getName());

  return ConflictItem(
    ToQString(altString), std::move(relativeName), QString(), index,
    std::move(fileName), true, std::move(origin), archive);
}

ConflictItem GeneralConflictsTab::createNoConflictItem(
  FileEntry::Index index, bool archive, QString fileName, QString relativeName)
{
  return ConflictItem(
    QString(), std::move(relativeName), QString(), index,
    std::move(fileName), false, QString(), archive);
}

ConflictItem GeneralConflictsTab::createOverwrittenItem(
  FileEntry::Index index, int fileOrigin, bool archive,
  QString fileName, QString relativeName)
{
  const auto& ds = *m_core.directoryStructure();
  const FilesOrigin& realOrigin = ds.getOriginByID(fileOrigin);

  QString after = ToQString(realOrigin.getName());
  QString altOrigin = after;

  return ConflictItem(
    QString(), std::move(relativeName), std::move(after),
    index, std::move(fileName), true, std::move(altOrigin), archive);
}

void GeneralConflictsTab::onOverwriteActivated(const QModelIndex& index)
{
  auto* model = dynamic_cast<ConflictListModel*>(ui->overwriteTree->model());
  if (!model) {
    return;
  }

  auto* item = model->getItem(static_cast<std::size_t>(index.row()));
  if (!item) {
    return;
  }

  const auto origin = item->altOrigin();
  if (!origin.isEmpty()) {
    emit modOpen(origin);
  }
}

void GeneralConflictsTab::onOverwrittenActivated(const QModelIndex& index)
{
  auto* model = dynamic_cast<ConflictListModel*>(ui->overwrittenTree->model());
  if (!model) {
    return;
  }

  auto* item = model->getItem(static_cast<std::size_t>(index.row()));
  if (!item) {
    return;
  }

  const auto origin = item->altOrigin();
  if (!origin.isEmpty()) {
    emit modOpen(origin);
  }
}


AdvancedConflictsTab::AdvancedConflictsTab(
  ConflictsTab* tab, Ui::ModInfoDialog* pui, OrganizerCore& oc) :
    m_tab(tab), ui(pui), m_core(oc),
    m_model(new AdvancedConflictListModel(ui->conflictsAdvancedList))
{
  // left-elide the overwrites column so that the nearest are visible
  ui->conflictsAdvancedList->setItemDelegateForColumn(
    0, new ElideLeftDelegate(ui->conflictsAdvancedList));

  // left-elide the file column to see filenames
  ui->conflictsAdvancedList->setItemDelegateForColumn(
    1, new ElideLeftDelegate(ui->conflictsAdvancedList));

  // don't elide the overwritten by column so that the nearest are visible

  QObject::connect(
    ui->conflictsAdvancedShowNoConflict, &QCheckBox::clicked,
    [&]{ update(); });

  QObject::connect(
    ui->conflictsAdvancedShowAll, &QRadioButton::clicked,
    [&]{ update(); });

  QObject::connect(
    ui->conflictsAdvancedShowNearest, &QRadioButton::clicked,
    [&]{ update(); });

  QObject::connect(
    ui->conflictsAdvancedList, &QTreeView::activated,
    [&]{ m_tab->activateItems(ui->conflictsAdvancedList); });

  QObject::connect(
    ui->conflictsAdvancedList, &QTreeView::customContextMenuRequested,
    [&](const QPoint& p){ m_tab->showContextMenu(p, ui->conflictsAdvancedList); });

  m_filter.setEdit(ui->conflictsAdvancedFilter);
  QObject::connect(&m_filter, &FilterWidget::changed, [&]{ update(); });
}

void AdvancedConflictsTab::clear()
{
  m_model->clear();
}

void AdvancedConflictsTab::saveState(Settings& s)
{
  s.geometry().saveState(ui->conflictsAdvancedList->header());
  s.widgets().saveChecked(ui->conflictsAdvancedShowNoConflict);
  s.widgets().saveChecked(ui->conflictsAdvancedShowAll);
  s.widgets().saveChecked(ui->conflictsAdvancedShowNearest);
}

void AdvancedConflictsTab::restoreState(const Settings& s)
{
  s.geometry().restoreState(ui->conflictsAdvancedList->header());
  s.widgets().restoreChecked(ui->conflictsAdvancedShowNoConflict);
  s.widgets().restoreChecked(ui->conflictsAdvancedShowAll);
  s.widgets().restoreChecked(ui->conflictsAdvancedShowNearest);
}

void AdvancedConflictsTab::update()
{
  clear();

  if (m_tab->origin() != nullptr) {
    const auto rootPath = m_tab->mod().absolutePath();

    const auto& files = m_tab->origin()->getFiles();
    m_model->reserve(files.size());

    for (const auto& file : files) {
      // careful: these two strings are moved into createItem() below
      QString relativeName = QDir::fromNativeSeparators(ToQString(file->getRelativePath()));
      QString fileName = rootPath + relativeName;

      bool archive = false;
      const int fileOrigin = file->getOrigin(archive);
      const auto& alternatives = file->getAlternatives();

      auto item = createItem(
        file->getIndex(), fileOrigin, archive,
        std::move(fileName), std::move(relativeName), alternatives);

      if (item) {
        m_model->add(std::move(*item));
      }
    }

    m_model->finished();
  }
}

std::optional<ConflictItem> AdvancedConflictsTab::createItem(
  FileEntry::Index index, int fileOrigin, bool archive,
  QString fileName, QString relativeName,
  const MOShared::FileEntry::AlternativesVector& alternatives)
{
  const auto& ds = *m_core.directoryStructure();

  std::wstring before, after;

  if (!alternatives.empty()) {
    const bool showAllAlts = ui->conflictsAdvancedShowAll->isChecked();

    int beforePrio = 0;
    int afterPrio = std::numeric_limits<int>::max();

    for (const auto& alt : alternatives)
    {
      const auto& altOrigin = ds.getOriginByID(alt.first);

      if (showAllAlts) {
        // fills 'before' and 'after' with all the alternatives that come
        // before and after this mod in terms of priority

        if (altOrigin.getPriority() < m_tab->origin()->getPriority()) {
          // add all the mods having a lower priority than this one
          if (!before.empty()) {
            before += L", ";
          }

          before += altOrigin.getName();
        } else if (altOrigin.getPriority() > m_tab->origin()->getPriority()) {
          // add all the mods having a higher priority than this one
          if (!after.empty()) {
            after += L", ";
          }

          after += altOrigin.getName();
        }
      } else {
        // keep track of the nearest mods that come before and after this one
        // in terms of priority

        if (altOrigin.getPriority() < m_tab->origin()->getPriority()) {
          // the alternative has a lower priority than this mod

          if (altOrigin.getPriority() > beforePrio) {
            // the alternative has a higher priority and therefore is closer
            // to this mod, use it
            before = altOrigin.getName();
            beforePrio = altOrigin.getPriority();
          }
        }

        if (altOrigin.getPriority() > m_tab->origin()->getPriority()) {
          // the alternative has a higher priority than this mod

          if (altOrigin.getPriority() < afterPrio) {
            // the alternative has a lower priority and there is closer
            // to this mod, use it
            after = altOrigin.getName();
            afterPrio = altOrigin.getPriority();
          }
        }
      }
    }

    // the primary origin is never in the list of alternatives, so it has to
    // be handled separately
    //
    // if 'after' is not empty, it means at least one alternative with a higher
    // priority than this mod was found; if the user only wants to see the
    // nearest mods, it's not worth checking for the primary origin because it
    // will always have a higher priority than the alternatives (or it wouldn't
    // be the primary)
    if (after.empty() || showAllAlts) {
      const FilesOrigin& realOrigin = ds.getOriginByID(fileOrigin);

      // if no mods overwrite this file, the primary origin is the same as this
      // mod, so ignore that
      if (realOrigin.getID() != m_tab->origin()->getID()) {
        if (!after.empty()) {
          after += L", ";
        }

        after += realOrigin.getName();
      }
    }
  }

  const bool hasAlts = !before.empty() || !after.empty();

  if (!hasAlts) {
    // if both before and after are empty, it means this file has no conflicts
    // at all, only display it if the user wants it
    if (!ui->conflictsAdvancedShowNoConflict->isChecked()) {
      return {};
    }
  }

  auto beforeQS = QString::fromStdWString(before);
  auto afterQS = QString::fromStdWString(after);

  const bool matched = m_filter.matches([&](auto&& what) {
    return
      beforeQS.contains(what, Qt::CaseInsensitive) ||
      relativeName.contains(what, Qt::CaseInsensitive) ||
      afterQS.contains(what, Qt::CaseInsensitive);
    });

  if (!matched) {
    return {};
  }

  return ConflictItem(
    std::move(beforeQS), std::move(relativeName), std::move(afterQS),
    index, std::move(fileName), hasAlts, QString(), archive);
}
