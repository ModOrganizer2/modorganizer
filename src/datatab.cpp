#include "datatab.h"
#include "ui_mainwindow.h"
#include "settings.h"
#include "organizercore.h"
#include "directoryentry.h"
#include "messagedialog.h"
#include "filetree.h"
#include "filetreemodel.h"
#include <log.h>
#include <report.h>

using namespace MOShared;
using namespace MOBase;

// in mainwindow.cpp
QString UnmanagedModName();


DataTab::DataTab(
  OrganizerCore& core, PluginContainer& pc,
  QWidget* parent, Ui::MainWindow* mwui) :
    m_core(core), m_pluginContainer(pc), m_parent(parent),
    ui{
      mwui->btnRefreshData, mwui->dataTree,
      mwui->conflictsCheckBox, mwui->showArchiveDataCheckBox}
{
  m_filetree.reset(new FileTree(core, m_pluginContainer, ui.tree));

  connect(
    ui.refresh, &QPushButton::clicked,
    [&]{ onRefresh(); });

  connect(
    ui.conflicts, &QCheckBox::toggled,
    [&]{ onConflicts(); });

  connect(
    ui.archives, &QCheckBox::toggled,
    [&]{ onArchives(); });

  connect(
    m_filetree.get(), &FileTree::executablesChanged,
    this, &DataTab::executablesChanged);

  connect(
    m_filetree.get(), &FileTree::originModified,
    this, &DataTab::originModified);

  connect(
    m_filetree.get(), &FileTree::displayModInformation,
    this, &DataTab::displayModInformation);
}

void DataTab::saveState(Settings& s) const
{
  s.geometry().saveState(ui.tree->header());
}

void DataTab::restoreState(const Settings& s)
{
  s.geometry().restoreState(ui.tree->header());

  // prior to 2.3, the list was not sortable, and this remembered in the
  // widget state, for whatever reason
  ui.tree->setSortingEnabled(true);
}

void DataTab::activated()
{
  updateTree();
}

void DataTab::onRefresh()
{
  if (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier) {
    m_filetree->clear();
  }

  m_core.refreshDirectoryStructure();
}

void DataTab::updateTree()
{
  m_filetree->refresh();
}

void DataTab::onConflicts()
{
  updateOptions();
}

void DataTab::onArchives()
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

  m_filetree->model()->setFlags(flags);
  updateTree();
}
