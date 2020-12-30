#include "modlistcontextmenu.h"

#include <report.h>

#include "modlist.h"
#include "modlistview.h"
#include "modlistviewactions.h"
#include "organizercore.h"

using namespace MOBase;

ModListGlobalContextMenu::ModListGlobalContextMenu(OrganizerCore& core, ModListView* view, QWidget* parent)
  : QMenu(parent)
{
  addAction(tr("Install Mod..."), [=]() { view->actions().installMod(); });
  addAction(tr("Create empty mod"), [=]() { view->actions().createEmptyMod(-1); });
  addAction(tr("Create Separator"), [=]() { view->actions().createSeparator(-1); });

  if (view->hasCollapsibleSeparators()) {
    addSeparator();
    addAction(tr("Collapse all"), view, &QTreeView::collapseAll);
    addAction(tr("Expand all"), view, &QTreeView::expandAll);
  }

  addSeparator();

  addAction(tr("Enable all visible"), [=]() {
    if (QMessageBox::question(view, tr("Confirm"), tr("Really enable all visible mods?"),
      QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      view->enableAllVisible();
    }
  });
  addAction(tr("Disable all visible"), [=]() {
    if (QMessageBox::question(view, tr("Confirm"), tr("Really disable all visible mods?"),
      QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      view->disableAllVisible();
    }
  });
  addAction(tr("Check for updates"), [=]() { view->actions().checkModsForUpdates(); });
  addAction(tr("Refresh"), &core, &OrganizerCore::profileRefresh);
  addAction(tr("Export to csv..."), [=]() { view->actions().exportModListCSV(); });
}

ModListContextMenu::ModListContextMenu(OrganizerCore& core, const QModelIndex& index, ModListView* view) :
  QMenu(view)
  , m_core(core)
  , m_index()
{
  if (view->selectionModel()->hasSelection()) {
    m_index = view->indexViewToModel(view->selectionModel()->selectedRows());
  }
  else {
    m_index = { index };
  }


  QMenu* allMods = new ModListGlobalContextMenu(core, view, view);
  allMods->setTitle(tr("All Mods"));
  addMenu(allMods);

  if (view->hasCollapsibleSeparators()) {
    addAction(tr("Collapse all"), view, &QTreeView::collapseAll);
    addAction(tr("Expand all"), view, &QTreeView::expandAll);
  }

  addSeparator();

  // Add type-specific items
  ModInfo::Ptr info = ModInfo::getByIndex(index.data(ModList::IndexRole).toInt());

  if (info->isOverwrite()) {
    addOverwriteActions(core, view);
  }
  else if (info->isBackup()) {
    addBackupActions(core, view);
  }
  else if (info->isSeparator()) {
    addSeparatorActions(core, view);
  }
  else if (info->isForeign()) {
    addForeignActions(core, view);
  }
  else {
    addRegularActions(core, view);
  }

  // add information for all except foreign
  if (!info->isForeign()) {
    QAction* infoAction = addAction(tr("Information..."), [=]() { view->actions().displayModInformation(m_index[0].row()); });
    setDefaultAction(infoAction);
  }
}

void ModListContextMenu::addOverwriteActions(OrganizerCore& core, ModListView* modListView)
{

}

void ModListContextMenu::addSeparatorActions(OrganizerCore& core, ModListView* modListView)
{

}

void ModListContextMenu::addForeignActions(OrganizerCore& core, ModListView* modListView)
{

}

void ModListContextMenu::addBackupActions(OrganizerCore& core, ModListView* modListView)
{

}

void ModListContextMenu::addRegularActions(OrganizerCore& core, ModListView* modListView)
{

}
