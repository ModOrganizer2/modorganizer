#include "datatab.h"
#include "filetree.h"
#include "filetreemodel.h"
#include "messagedialog.h"
#include "modelutils.h"
#include "organizercore.h"
#include "settings.h"
#include "ui_mainwindow.h"
#include <log.h>
#include <report.h>

using namespace MOShared;
using namespace MOBase;

// in mainwindow.cpp
QString UnmanagedModName();

DataTab::DataTab(OrganizerCore& core, PluginContainer& pc, QWidget* parent,
                 Ui::MainWindow* mwui)
    : m_core(core), m_pluginContainer(pc), m_parent(parent),
      ui{mwui->tabWidget,
         mwui->dataTab,
         mwui->dataTabRefresh,
         mwui->dataTree,
         mwui->dataTabShowOnlyConflicts,
         mwui->dataTabShowFromArchives,
         mwui->dataTabShowHiddenFiles},
      m_needUpdate(true)
{
  m_filetree.reset(new FileTree(core, m_pluginContainer, ui.tree));
  m_filter.setUseSourceSort(true);
  m_filter.setFilterColumn(FileTreeModel::FileName);
  m_filter.setEdit(mwui->dataTabFilter);
  m_filter.setList(mwui->dataTree);
  m_filter.setUpdateDelay(true);

  if (auto* m = m_filter.proxyModel()) {
    m->setDynamicSortFilter(false);
  }

  connect(&m_filter, &FilterWidget::aboutToChange, [&] {
    ensureFullyLoaded();
  });

  connect(ui.refresh, &QPushButton::clicked, [&] {
    onRefresh();
  });

  connect(ui.conflicts, &QCheckBox::toggled, [&] {
    onConflicts();
  });

  connect(ui.archives, &QCheckBox::toggled, [&] {
    onArchives();
  });

  connect(ui.hiddenFiles, &QCheckBox::toggled, [&] {
    onHiddenFiles();
  });

  connect(ui.tree->selectionModel(), &QItemSelectionModel::selectionChanged, [=] {
    const auto* fileTreeModel     = m_filetree->model();
    const auto& selectedIndexList = MOShared::indexViewToModel(
        ui.tree->selectionModel()->selectedRows(), fileTreeModel);
    std::set<QString> mods;
    for (auto& idx : selectedIndexList) {
      mods.insert(fileTreeModel->itemFromIndex(idx)->mod());
    }
    mwui->modList->setHighlightedMods(mods);
  });

  connect(m_filetree.get(), &FileTree::executablesChanged, this,
          &DataTab::executablesChanged);

  connect(m_filetree.get(), &FileTree::originModified, this, &DataTab::originModified);

  connect(m_filetree.get(), &FileTree::displayModInformation, this,
          &DataTab::displayModInformation);
}

void DataTab::saveState(Settings& s) const
{
  s.geometry().saveState(ui.tree->header());
  s.widgets().saveChecked(ui.conflicts);
  s.widgets().saveChecked(ui.archives);
  s.widgets().saveChecked(ui.hiddenFiles);
}

void DataTab::restoreState(const Settings& s)
{
  s.geometry().restoreState(ui.tree->header());

  // prior to 2.3, the list was not sortable, and this remembered in the
  // widget state, for whatever reason
  ui.tree->setSortingEnabled(true);

  s.widgets().restoreChecked(ui.conflicts);
  s.widgets().restoreChecked(ui.archives);
  s.widgets().restoreChecked(ui.hiddenFiles);
}

void DataTab::activated()
{
  if (m_needUpdate) {
    updateTree();
  }
  // update highlighted mods
  ui.tree->selectionModel()->selectionChanged({}, {});
}

bool DataTab::isActive() const
{
  return ui.tabs->currentWidget() == ui.tab;
}

void DataTab::onRefresh()
{
  if (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier) {
    m_filetree->model()->setEnabled(false);
    m_filetree->clear();
  }

  m_core.refreshDirectoryStructure();
}

void DataTab::updateTree()
{
  if (isActive()) {
    doUpdateTree();
  } else {
    m_needUpdate = true;
  }
}

void DataTab::doUpdateTree()
{
  m_filetree->model()->setEnabled(true);
  m_filetree->refresh();

  if (!m_filter.empty()) {
    ensureFullyLoaded();

    if (auto* m = m_filter.proxyModel()) {
      m->invalidate();
    }
  }

  m_needUpdate = false;
}

void DataTab::ensureFullyLoaded()
{
  if (!m_filetree->fullyLoaded()) {
    m_filter.setFilteringEnabled(false);
    m_filetree->ensureFullyLoaded();
    m_filter.setFilteringEnabled(true);
  }
}

void DataTab::onConflicts()
{
  updateOptions();
}

void DataTab::onArchives()
{
  updateOptions();
}

void DataTab::onHiddenFiles()
{
  updateOptions();
}

void DataTab::updateOptions()
{
  using M = FileTreeModel;

  M::Flags flags = M::NoFlags;

  if (ui.conflicts->isChecked()) {
    flags |= M::ConflictsOnly | M::PruneDirectories;
  }

  if (ui.archives->isChecked()) {
    flags |= M::Archives;
  }

  if (ui.hiddenFiles->isChecked()) {
    flags |= M::HiddenFiles;
  }

  m_filetree->model()->setFlags(flags);
  updateTree();
}
