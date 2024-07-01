#include "modinfodialogconflicts.h"
#include "modinfodialog.h"
#include "modinfodialogconflictsmodels.h"
#include "organizercore.h"
#include "settings.h"
#include "shared/directoryentry.h"
#include "shared/fileentry.h"
#include "shared/filesorigin.h"
#include "ui_modinfodialog.h"
#include "utility.h"

using namespace MOShared;
using namespace MOBase;

// if there are more than 50 selected items in the conflict tree, don't bother
// checking whether menu items apply to them, just show all of them
const std::size_t max_small_selection = 50;

std::size_t smallSelectionSize(const QTreeView* tree)
{
  const std::size_t too_many = std::numeric_limits<std::size_t>::max();

  std::size_t n   = 0;
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
void forEachInSelection(QTreeView* tree, F&& f)
{
  const auto* sel = tree->selectionModel();

  const auto* proxy = dynamic_cast<QAbstractProxyModel*>(tree->model());

  if (!proxy) {
    log::error("tree doesn't have a SortProxyModel");
    return;
  }

  const auto* model = dynamic_cast<ConflictListModel*>(proxy->sourceModel());

  if (!model) {
    log::error("tree doesn't have a ConflictListModel");
    return;
  }

  for (const auto& rowIndex : sel->selectedRows()) {
    auto modelRow = proxy->mapToSource(rowIndex).row();
    if (auto* item = model->getItem(static_cast<std::size_t>(modelRow))) {
      if (!f(item)) {
        return;
      }
    }
  }
}

ConflictsTab::ConflictsTab(ModInfoDialogTabContext cx)
    : ModInfoDialogTab(cx),  // don't move, cx is used again
      m_general(this, cx.ui, cx.core), m_advanced(this, cx.ui, cx.core)
{
  connect(&m_general, &GeneralConflictsTab::modOpen, [&](const QString& name) {
    emitModOpen(name);
  });

  connect(&m_advanced, &AdvancedConflictsTab::modOpen, [&](const QString& name) {
    emitModOpen(name);
  });
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
  bool stop    = false;

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

  const auto* proxy = dynamic_cast<QAbstractProxyModel*>(tree->model());

  if (!proxy) {
    log::error("tree doesn't have a SortProxyModel");
    return;
  }

  const auto* model = dynamic_cast<ConflictListModel*>(proxy->sourceModel());

  if (!model) {
    log::error("tree doesn't have a ConflictListModel");
    return;
  }

  forEachInSelection(tree, [&](const ConflictItem* item) {
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
  forEachInSelection(tree, [&](const ConflictItem* item) {
    const auto path = item->fileName();

    if (tryPreview && canPreviewFile(plugins(), item->isArchive(), path)) {
      previewItem(item);
    } else {
      openItem(item, false);
    }

    return true;
  });
}

void ConflictsTab::openItems(QTreeView* tree, bool hooked)
{
  // the menu item is only shown for a single selection, but handle all of them
  // in case this changes
  forEachInSelection(tree, [&](const ConflictItem* item) {
    openItem(item, hooked);
    return true;
  });
}

void ConflictsTab::openItem(const ConflictItem* item, bool hooked)
{
  core()
      .processRunner()
      .setFromFile(parentWidget(), QFileInfo(item->fileName()))
      .setHooked(hooked)
      .setWaitForCompletion()
      .run();
}

void ConflictsTab::previewItems(QTreeView* tree)
{
  // the menu item is only shown for a single selection, but handle all of them
  // in case this changes
  forEachInSelection(tree, [&](const ConflictItem* item) {
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
  forEachInSelection(tree, [&](const ConflictItem* item) {
    shell::Explore(item->fileName());
    return true;
  });
}

void ConflictsTab::showContextMenu(const QPoint& pos, QTreeView* tree)
{
  auto actions = createMenuActions(tree);

  QMenu menu;

  // open
  if (actions.open) {
    connect(actions.open, &QAction::triggered, [&] {
      openItems(tree, false);
    });
  }

  // preview
  if (actions.preview) {
    connect(actions.preview, &QAction::triggered, [&] {
      previewItems(tree);
    });
  }

  if ((actions.open && actions.open->isEnabled()) &&
      (actions.preview && actions.preview->isEnabled())) {
    if (Settings::instance().interface().doubleClicksOpenPreviews()) {
      menu.addAction(actions.preview);
      menu.addAction(actions.open);
    } else {
      menu.addAction(actions.open);
      menu.addAction(actions.preview);
    }
  } else {
    if (actions.open) {
      menu.addAction(actions.open);
    }

    if (actions.preview) {
      menu.addAction(actions.preview);
    }
  }

  // run hooked
  if (actions.runHooked) {
    connect(actions.runHooked, &QAction::triggered, [&] {
      openItems(tree, true);
    });

    menu.addAction(actions.runHooked);
  }

  // goto
  if (actions.gotoMenu) {
    menu.addMenu(actions.gotoMenu);

    for (auto* a : actions.gotoActions) {
      connect(a, &QAction::triggered, [&, name = a->text()] {
        emitModOpen(name);
      });

      actions.gotoMenu->addAction(a);
    }
  }

  // explore
  if (actions.explore) {
    connect(actions.explore, &QAction::triggered, [&] {
      exploreItems(tree);
    });

    menu.addAction(actions.explore);
  }

  menu.addSeparator();

  // hide
  if (actions.hide) {
    connect(actions.hide, &QAction::triggered, [&] {
      changeItemsVisibility(tree, false);
    });

    menu.addAction(actions.hide);
  }

  // unhide
  if (actions.unhide) {
    connect(actions.unhide, &QAction::triggered, [&] {
      changeItemsVisibility(tree, true);
    });

    menu.addAction(actions.unhide);
  }

  if (!menu.isEmpty()) {
    if (actions.open || actions.preview || actions.runHooked) {
      // bold the first option
      auto* top = menu.actions()[0];
      auto f    = top->font();
      f.setBold(true);
      top->setFont(f);
    }

    menu.exec(tree->viewport()->mapToGlobal(pos));
  }
}

ConflictsTab::Actions ConflictsTab::createMenuActions(QTreeView* tree)
{
  if (tree->selectionModel()->selection().isEmpty()) {
    return {};
  }

  bool enableHide    = true;
  bool enableUnhide  = true;
  bool enableRun     = true;
  bool enableOpen    = true;
  bool enablePreview = true;
  bool enableExplore = true;
  bool enableGoto    = true;

  const auto n = smallSelectionSize(tree);

  const auto* proxy = dynamic_cast<QAbstractProxyModel*>(tree->model());

  if (!proxy) {
    log::error("tree doesn't have a SortProxyModel");
    return {};
  }

  const auto* model = dynamic_cast<ConflictListModel*>(proxy->sourceModel());

  if (!model) {
    log::error("tree doesn't have a ConflictListModel");
    return {};
  }

  auto modelSel = proxy->mapSelectionToSource(tree->selectionModel()->selection());

  if (n == 1) {
    // this is a single selection
    const auto* item =
        model->getItem(static_cast<std::size_t>(modelSel.indexes()[0].row()));

    if (!item) {
      return {};
    }

    enableHide    = item->canHide();
    enableUnhide  = item->canUnhide();
    enableRun     = item->canRun();
    enableOpen    = item->canOpen();
    enablePreview = item->canPreview(plugins());
    enableExplore = item->canExplore();
    enableGoto    = item->hasAlts();
  } else {
    // this is a multiple selection, don't show open/preview so users don't open
    // a thousand files
    enableRun     = false;
    enableOpen    = false;
    enablePreview = false;

    // can't explore multiple files
    enableExplore = false;

    // don't bother with this on multiple selection, at least for now
    enableGoto = false;

    if (n <= max_small_selection) {
      // if the number of selected items is low, checking them to accurately
      // show the menu items is worth it
      enableHide   = false;
      enableUnhide = false;

      forEachInSelection(tree, [&](const ConflictItem* item) {
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
    actions.open      = new QAction(tr("&Execute"), parentWidget());
    actions.runHooked = new QAction(tr("Execute with &VFS"), parentWidget());
  } else if (enableOpen) {
    actions.open      = new QAction(tr("&Open"), parentWidget());
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
    const auto* item =
        model->getItem(static_cast<std::size_t>(modelSel.indexes()[0].row()));

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
    const auto& o = ds.getOriginByID(alt.originID());
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

GeneralConflictsTab::GeneralConflictsTab(ConflictsTab* tab, Ui::ModInfoDialog* pui,
                                         OrganizerCore& oc)
    : m_tab(tab), ui(pui), m_core(oc),
      m_overwriteModel(new OverwriteConflictListModel(ui->overwriteTree)),
      m_overwrittenModel(new OverwrittenConflictListModel(ui->overwrittenTree)),
      m_noConflictModel(new NoConflictListModel(ui->noConflictTree))
{
  m_expanders.overwrite.set(ui->overwriteExpander, ui->overwriteTree, true);
  m_expanders.overwritten.set(ui->overwrittenExpander, ui->overwrittenTree, true);
  m_expanders.nonconflict.set(ui->noConflictExpander, ui->noConflictTree);

  m_filterOverwrite.setEdit(ui->overwriteLineEdit);
  m_filterOverwrite.setList(ui->overwriteTree);
  m_filterOverwrite.setUseSourceSort(true);

  m_filterOverwritten.setEdit(ui->overwrittenLineEdit);
  m_filterOverwritten.setList(ui->overwrittenTree);
  m_filterOverwritten.setUseSourceSort(true);

  m_filterNoConflicts.setEdit(ui->noConflictLineEdit);
  m_filterNoConflicts.setList(ui->noConflictTree);
  m_filterNoConflicts.setUseSourceSort(true);

  QObject::connect(ui->overwriteTree, &QTreeView::doubleClicked, [&](auto&&) {
    m_tab->activateItems(ui->overwriteTree);
  });

  QObject::connect(ui->overwrittenTree, &QTreeView::doubleClicked, [&](auto&& item) {
    m_tab->activateItems(ui->overwrittenTree);
  });

  QObject::connect(ui->noConflictTree, &QTreeView::doubleClicked, [&](auto&& item) {
    m_tab->activateItems(ui->noConflictTree);
  });

  QObject::connect(ui->overwriteTree, &QTreeView::customContextMenuRequested,
                   [&](const QPoint& p) {
                     m_tab->showContextMenu(p, ui->overwriteTree);
                   });

  QObject::connect(ui->overwrittenTree, &QTreeView::customContextMenuRequested,
                   [&](const QPoint& p) {
                     m_tab->showContextMenu(p, ui->overwrittenTree);
                   });

  QObject::connect(ui->noConflictTree, &QTreeView::customContextMenuRequested,
                   [&](const QPoint& p) {
                     m_tab->showContextMenu(p, ui->noConflictTree);
                   });
}

void GeneralConflictsTab::clear()
{
  m_counts.clear();

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

  if (m_tab->origin() != nullptr) {
    const auto rootPath = m_tab->mod().absolutePath();

    for (const auto& file : m_tab->origin()->getFiles()) {
      // careful: these two strings are moved into createXItem() below
      QString relativeName =
          QDir::fromNativeSeparators(ToQString(file->getRelativePath()));
      QString fileName = rootPath + relativeName;

      bool archive         = false;
      const int fileOrigin = file->getOrigin(archive);

      ++m_counts.numTotalFiles;

      const auto& alternatives = file->getAlternatives();

      if (fileOrigin == m_tab->origin()->getID()) {
        // current mod is primary origin, the winner
        (archive) ? ++m_counts.numTotalArchive : ++m_counts.numTotalLoose;

        if (!alternatives.empty()) {
          m_overwriteModel->add(
              createOverwriteItem(file->getIndex(), archive, std::move(fileName),
                                  std::move(relativeName), alternatives));

          ++m_counts.numOverwrite;
          if (archive) {
            ++m_counts.numOverwriteArchive;
          } else {
            ++m_counts.numOverwriteLoose;
          }
        } else {
          // otherwise, put the file in the noconflict tree
          m_noConflictModel->add(createNoConflictItem(
              file->getIndex(), archive, std::move(fileName), std::move(relativeName)));

          ++m_counts.numNonConflicting;
          if (archive) {
            ++m_counts.numNonConflictingArchive;
          } else {
            ++m_counts.numNonConflictingLoose;
          }
        }
      } else {
        auto currId     = m_tab->origin()->getID();
        auto currModAlt = std::find_if(alternatives.begin(), alternatives.end(),
                                       [&currId](auto const& alt) {
                                         return currId == alt.originID();
                                       });

        if (currModAlt == alternatives.end()) {
          log::error("Mod {} not found in the list of origins for file {}",
                     m_tab->origin()->getName(), fileName);
          continue;
        }

        bool currModFileArchive = currModAlt->isFromArchive();

        m_overwrittenModel->add(createOverwrittenItem(file->getIndex(), fileOrigin,
                                                      archive, std::move(fileName),
                                                      std::move(relativeName)));

        ++m_counts.numOverwritten;
        if (currModFileArchive) {
          ++m_counts.numOverwrittenArchive;
          ++m_counts.numTotalArchive;
        } else {
          ++m_counts.numOverwrittenLoose;
          ++m_counts.numTotalLoose;
        }
      }
    }

    m_overwriteModel->finished();
    m_overwrittenModel->finished();
    m_noConflictModel->finished();
  }

  updateUICounters();

  return (m_counts.numOverwrite > 0 || m_counts.numOverwritten > 0);
}

ConflictItem GeneralConflictsTab::createOverwriteItem(
    FileIndex index, bool archive, QString fileName, QString relativeName,
    const MOShared::AlternativesVector& alternatives)
{
  const auto& ds = *m_core.directoryStructure();
  std::wstring altString;

  for (const auto& alt : alternatives) {
    if (!altString.empty()) {
      altString += L", ";
    }

    altString += ds.getOriginByID(alt.originID()).getName();
  }

  auto origin = ToQString(ds.getOriginByID(alternatives.back().originID()).getName());

  return ConflictItem(ToQString(altString), std::move(relativeName), QString(), index,
                      std::move(fileName), true, std::move(origin), archive);
}

ConflictItem GeneralConflictsTab::createNoConflictItem(FileIndex index, bool archive,
                                                       QString fileName,
                                                       QString relativeName)
{
  return ConflictItem(QString(), std::move(relativeName), QString(), index,
                      std::move(fileName), false, QString(), archive);
}

ConflictItem GeneralConflictsTab::createOverwrittenItem(FileIndex index, int fileOrigin,
                                                        bool archive, QString fileName,
                                                        QString relativeName)
{
  const auto& ds                = *m_core.directoryStructure();
  const FilesOrigin& realOrigin = ds.getOriginByID(fileOrigin);

  QString after     = ToQString(realOrigin.getName());
  QString altOrigin = after;

  return ConflictItem(QString(), std::move(relativeName), std::move(after), index,
                      std::move(fileName), true, std::move(altOrigin), archive);
}

QString percent(int a, int b)
{
  if (b == 0) {
    return QString::number(0, 'f', 2);
  }
  return QString::number((((float)a / (float)b) * 100), 'f', 2);
}

void GeneralConflictsTab::updateUICounters()
{
  ui->overwriteCount->display(m_counts.numOverwrite);
  ui->overwrittenCount->display(m_counts.numOverwritten);
  ui->noConflictCount->display(m_counts.numNonConflicting);

  QString tooltipBase =
      tr("<table cellspacing=\"5\">"
         "<tr><th>Type</th><th>%1</th><th>Total</th><th>Percent</th></tr>"
         "<tr><td>Loose files:&emsp;</td>"
         "<td align=right>%2</td><td align=right>%3</td><td align=right>%4%</td></tr>"
         "<tr><td>Archive files:&emsp;</td>"
         "<td align=right>%5</td><td align=right>%6</td><td align=right>%7%</td></tr>"
         "<tr><td>Combined:&emsp;</td>"
         "<td align=right>%8</td><td align=right>%9</td><td align=right>%10%</td></tr>"
         "</table>");

  QString tooltipOverwrite =
      tooltipBase.arg(tr("Winning"))
          .arg(m_counts.numOverwriteLoose)
          .arg(m_counts.numTotalLoose)
          .arg(percent(m_counts.numOverwriteLoose, m_counts.numTotalLoose))
          .arg(m_counts.numOverwriteArchive)
          .arg(m_counts.numTotalArchive)
          .arg(percent(m_counts.numOverwriteArchive, m_counts.numTotalArchive))
          .arg(m_counts.numOverwrite)
          .arg(m_counts.numTotalFiles)
          .arg(percent(m_counts.numOverwrite, m_counts.numTotalFiles));

  QString tooltipOverwritten =
      tooltipBase.arg(tr("Losing"))
          .arg(m_counts.numOverwrittenLoose)
          .arg(m_counts.numTotalLoose)
          .arg(percent(m_counts.numOverwrittenLoose, m_counts.numTotalLoose))
          .arg(m_counts.numOverwrittenArchive)
          .arg(m_counts.numTotalArchive)
          .arg(percent(m_counts.numOverwrittenArchive, m_counts.numTotalArchive))
          .arg(m_counts.numOverwritten)
          .arg(m_counts.numTotalFiles)
          .arg(percent(m_counts.numOverwritten, m_counts.numTotalFiles));

  QString tooltipNonConflict =
      tooltipBase.arg(tr("Non conflicting"))
          .arg(m_counts.numNonConflictingLoose)
          .arg(m_counts.numTotalLoose)
          .arg(percent(m_counts.numNonConflictingLoose, m_counts.numTotalLoose))
          .arg(m_counts.numNonConflictingArchive)
          .arg(m_counts.numTotalArchive)
          .arg(percent(m_counts.numNonConflictingArchive, m_counts.numTotalArchive))
          .arg(m_counts.numNonConflicting)
          .arg(m_counts.numTotalFiles)
          .arg(percent(m_counts.numNonConflicting, m_counts.numTotalFiles));

  ui->overwriteCount->setToolTip(tooltipOverwrite);
  ui->overwrittenCount->setToolTip(tooltipOverwritten);
  ui->noConflictCount->setToolTip(tooltipNonConflict);
}

void GeneralConflictsTab::onOverwriteActivated(const QModelIndex& index)
{
  const auto* proxy = dynamic_cast<QAbstractProxyModel*>(ui->overwriteTree->model());

  if (!proxy) {
    log::error("tree doesn't have a SortProxyModel");
    return;
  }

  const auto* model = dynamic_cast<ConflictListModel*>(proxy->sourceModel());

  if (!model) {
    log::error("tree doesn't have a ConflictListModel");
    return;
  }

  auto modelIndex = proxy->mapToSource(index);

  auto* item = model->getItem(static_cast<std::size_t>(modelIndex.row()));
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
  const auto* proxy = dynamic_cast<QAbstractProxyModel*>(ui->overwrittenTree->model());

  if (!proxy) {
    log::error("tree doesn't have a SortProxyModel");
    return;
  }

  const auto* model = dynamic_cast<ConflictListModel*>(proxy->sourceModel());

  if (!model) {
    log::error("tree doesn't have a ConflictListModel");
    return;
  }

  proxy->mapSelectionToSource(ui->overwrittenTree->selectionModel()->selection());

  auto modelIndex = proxy->mapToSource(index);

  auto* item = model->getItem(static_cast<std::size_t>(modelIndex.row()));
  if (!item) {
    return;
  }

  const auto origin = item->altOrigin();
  if (!origin.isEmpty()) {
    emit modOpen(origin);
  }
}

AdvancedConflictsTab::AdvancedConflictsTab(ConflictsTab* tab, Ui::ModInfoDialog* pui,
                                           OrganizerCore& oc)
    : m_tab(tab), ui(pui), m_core(oc),
      m_model(new AdvancedConflictListModel(ui->conflictsAdvancedList))
{
  m_filter.setEdit(ui->conflictsAdvancedFilter);
  m_filter.setList(ui->conflictsAdvancedList);
  m_filter.setUseSourceSort(true);

  // left-elide the overwrites column so that the nearest are visible
  ui->conflictsAdvancedList->setItemDelegateForColumn(
      0, new ElideLeftDelegate(ui->conflictsAdvancedList));

  // left-elide the file column to see filenames
  ui->conflictsAdvancedList->setItemDelegateForColumn(
      1, new ElideLeftDelegate(ui->conflictsAdvancedList));

  // don't elide the overwritten by column so that the nearest are visible

  QObject::connect(ui->conflictsAdvancedShowNoConflict, &QCheckBox::clicked, [&] {
    update();
  });

  QObject::connect(ui->conflictsAdvancedShowAll, &QRadioButton::clicked, [&] {
    update();
  });

  QObject::connect(ui->conflictsAdvancedShowNearest, &QRadioButton::clicked, [&] {
    update();
  });

  QObject::connect(ui->conflictsAdvancedList, &QTreeView::activated, [&] {
    m_tab->activateItems(ui->conflictsAdvancedList);
  });

  QObject::connect(ui->conflictsAdvancedList, &QTreeView::customContextMenuRequested,
                   [&](const QPoint& p) {
                     m_tab->showContextMenu(p, ui->conflictsAdvancedList);
                   });
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
      QString relativeName =
          QDir::fromNativeSeparators(ToQString(file->getRelativePath()));
      QString fileName = rootPath + relativeName;

      bool archive             = false;
      const int fileOrigin     = file->getOrigin(archive);
      const auto& alternatives = file->getAlternatives();

      auto item = createItem(file->getIndex(), fileOrigin, archive, std::move(fileName),
                             std::move(relativeName), alternatives);

      if (item) {
        m_model->add(std::move(*item));
      }
    }

    m_model->finished();
  }
}

std::optional<ConflictItem>
AdvancedConflictsTab::createItem(FileIndex index, int fileOrigin, bool archive,
                                 QString fileName, QString relativeName,
                                 const MOShared::AlternativesVector& alternatives)
{
  const auto& ds = *m_core.directoryStructure();

  std::wstring before, after;

  auto currOrigin        = m_tab->origin();
  bool isCurrOrigArchive = archive;

  if (!alternatives.empty()) {
    const bool showAllAlts = ui->conflictsAdvancedShowAll->isChecked();

    if (currOrigin->getID() == fileOrigin) {
      // current origin is the active winner, all alternatives go in 'before'

      if (showAllAlts) {
        for (const auto& alt : alternatives) {
          const auto& altOrigin = ds.getOriginByID(alt.originID());
          if (!before.empty()) {
            before += L", ";
          }

          before += altOrigin.getName();
        }
      } else {
        // only add nearest, which is the last element of alternatives
        const auto& altOrigin = ds.getOriginByID(alternatives.back().originID());

        before += altOrigin.getName();
      }

    } else {
      // current mod is one of the alternatives, find its position

      auto currOrgId = currOrigin->getID();

      auto currModIter = std::find_if(alternatives.begin(), alternatives.end(),
                                      [&currOrgId](auto const& alt) {
                                        return currOrgId == alt.originID();
                                      });

      if (currModIter == alternatives.end()) {
        log::error("Mod {} not found in the list of origins for file {}",
                   currOrigin->getName(), fileName);
        return {};
      }

      isCurrOrigArchive = currModIter->isFromArchive();

      if (showAllAlts) {
        // fills 'before' and 'after' with all the alternatives that come
        // before and after the current mod, trusting the alternatives vector to be
        // already sorted correctly

        for (auto iter = alternatives.begin(); iter != alternatives.end(); iter++) {

          const auto& altOrigin = ds.getOriginByID(iter->originID());

          if (iter < currModIter) {
            // mod comes before current

            if (!before.empty()) {
              before += L", ";
            }

            before += altOrigin.getName();
          } else if (iter > currModIter) {
            // mod comes after current

            if (!after.empty()) {
              after += L", ";
            }

            after += altOrigin.getName();
          }
        }

        // also add the active winner origin (the one outside alternatives) to 'after'
        if (!after.empty()) {
          after += L", ";
        }
        after += ds.getOriginByID(fileOrigin).getName();

      } else {
        // only show nearest origins

        // before
        if (currModIter > alternatives.begin()) {
          auto previousOrigId = (currModIter - 1)->originID();
          before += ds.getOriginByID(previousOrigId).getName();
        }

        // after
        if (currModIter < (alternatives.end() - 1)) {
          auto followingOrigId = (currModIter + 1)->originID();
          after += ds.getOriginByID(followingOrigId).getName();
        } else {
          // current mod is last of alternatives, so closest to the active winner

          after += ds.getOriginByID(fileOrigin).getName();
        }
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
  auto afterQS  = QString::fromStdWString(after);

  return ConflictItem(std::move(beforeQS), std::move(relativeName), std::move(afterQS),
                      index, std::move(fileName), hasAlts, QString(),
                      isCurrOrigArchive);
}
