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
  , m_index(index)
  , m_view(view)
  , m_actions(view->actions())
{
  if (view->selectionModel()->hasSelection()) {
    m_selected = view->indexViewToModel(view->selectionModel()->selectedRows());
  }
  else {
    m_selected = { index };
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
    addOverwriteActions(info);
  }
  else if (info->isBackup()) {
    addBackupActions(info);
  }
  else if (info->isSeparator()) {
    addSeparatorActions(info);
  }
  else if (info->isForeign()) {
    addForeignActions(info);
  }
  else {
    addRegularActions(info);
  }

  // add information for all except foreign
  if (!info->isForeign()) {
    QAction* infoAction = addAction(tr("Information..."), [=]() { view->actions().displayModInformation(m_index.data(ModList::IndexRole).toInt()); });
    setDefaultAction(infoAction);
  }
}

QMenu* ModListContextMenu::createSendToContextMenu()
{
  QMenu* menu = new QMenu(m_view);
  menu->setTitle(tr("Send to... "));
  menu->addAction(tr("Top"), [=]() { m_actions.sendModsToTop(m_selected); });
  menu->addAction(tr("Bottom"), [=]() { m_actions.sendModsToBottom(m_selected); });
  menu->addAction(tr("Priority..."), [=]() { m_actions.sendModsToPriority(m_selected); });
  menu->addAction(tr("Separator..."), [=]() { m_actions.sendModsToSeparator(m_selected); });
  return menu;
}

void ModListContextMenu::addOverwriteActions(ModInfo::Ptr mod)
{
  if (QDir(mod->absolutePath()).count() > 2) {
    addAction(tr("Sync to Mods..."), [=]() { m_core.syncOverwrite(); });
    addAction(tr("Create Mod..."), [=]() { m_actions.createModFromOverwrite(); });
    addAction(tr("Move content to Mod..."), [=]() { m_actions.moveOverwriteContentToExistingMod(); });
    addAction(tr("Clear Overwrite..."), [=]() { m_actions.clearOverwrite(); });
  }
  addAction(tr("Open in Explorer"), [=]() { m_actions.openExplorer(m_selected); });
}

void ModListContextMenu::addSeparatorActions(ModInfo::Ptr mod)
{

}

void ModListContextMenu::addForeignActions(ModInfo::Ptr mod)
{
  if (m_view->sortColumn() == ModList::COL_PRIORITY) {
    addMenu(createSendToContextMenu());
  }
}

void ModListContextMenu::addBackupActions(ModInfo::Ptr mod)
{
  auto flags = mod->getFlags();
  addAction(tr("Restore Backup"), [=]() { m_actions.restoreBackup(m_index); });
  addAction(tr("Remove Backup..."), [=]() { m_actions.removeMods(m_selected); });
  addSeparator();
  if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_INVALID) != flags.end()) {
    addAction(tr("Ignore missing data"), [=]() { m_actions.ignoreMissingData(m_selected); });
  }
  if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_ALTERNATE_GAME) != flags.end()) {
    addAction(tr("Mark as converted/working"), [=]() { m_actions.markConverted(m_selected); });
  }
  addSeparator();
  if (mod->nexusId() > 0) {
    addAction(tr("Visit on Nexus"), [=]() { m_actions.visitOnNexus(m_selected); });
  }

  const auto url = mod->parseCustomURL();
  if (url.isValid()) {
    addAction(tr("Visit on %1").arg(url.host()), [=]() { m_actions.visitWebPage(m_selected); });
  }

  addAction(tr("Open in Explorer"), [=]() { m_actions.openExplorer(m_selected); });
}

void ModListContextMenu::addRegularActions(ModInfo::Ptr mod)
{

}
