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
  , m_view(view)
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

  // TODO:
  // - Don't forget to check for the sort priority for "Send To... "

  if (info->isOverwrite()) {
    addOverwriteActions();
  }
  else if (info->isBackup()) {
    addBackupActions();
  }
  else if (info->isSeparator()) {
    addSeparatorActions();
  }
  else if (info->isForeign()) {
    addForeignActions();
  }
  else {
    addRegularActions();
  }

  // add information for all except foreign
  if (!info->isForeign()) {
    QAction* infoAction = addAction(tr("Information..."), [=]() { view->actions().displayModInformation(m_index[0].row()); });
    setDefaultAction(infoAction);
  }
}

QMenu* ModListContextMenu::createSendToContextMenu()
{
  QMenu* menu = new QMenu(m_view);
  menu->setTitle(tr("Send to... "));
  menu->addAction(tr("Top"), [=]() { m_view->actions().sendModsToTop(m_index); });
  menu->addAction(tr("Bottom"), [=]() { m_view->actions().sendModsToBottom(m_index); });
  menu->addAction(tr("Priority..."), [=]() { m_view->actions().sendModsToPriority(m_index); });
  menu->addAction(tr("Separator..."), [=]() { m_view->actions().sendModsToSeparator(m_index); });
  return menu;
}

void ModListContextMenu::addOverwriteActions()
{

}

void ModListContextMenu::addSeparatorActions()
{

}

void ModListContextMenu::addForeignActions()
{
  if (m_view->sortColumn() == ModList::COL_PRIORITY) {
    addMenu(createSendToContextMenu());
  }
}

void ModListContextMenu::addBackupActions()
{

}

void ModListContextMenu::addRegularActions()
{

}
