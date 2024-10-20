/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "modinfodialog.h"
#include "modinfodialogcategories.h"
#include "modinfodialogconflicts.h"
#include "modinfodialogesps.h"
#include "modinfodialogfiletree.h"
#include "modinfodialogimages.h"
#include "modinfodialognexus.h"
#include "modinfodialogtextfiles.h"
#include "modlistview.h"
#include "organizercore.h"
#include "pluginmanager.h"
#include "shared/directoryentry.h"
#include "shared/filesorigin.h"
#include "ui_modinfodialog.h"
#include <filesystem>

using namespace MOBase;
using namespace MOShared;
namespace fs = std::filesystem;

const int max_scan_for_context_menu = 50;

bool canPreviewFile(const PluginManager& pluginManager, bool isArchive,
                    const QString& filename)
{
  const auto ext = QFileInfo(filename).suffix().toLower();
  return pluginManager.previewGenerator().previewSupported(ext, isArchive);
}

bool isExecutableFilename(const QString& filename)
{
  static const std::set<QString> exeExtensions = {"exe", "cmd", "bat"};

  const auto ext = QFileInfo(filename).suffix().toLower();

  return exeExtensions.contains(ext);
}

bool canRunFile(bool isArchive, const QString& filename)
{
  // can run executables that are not archives
  return !isArchive && isExecutableFilename(filename);
}

bool canOpenFile(bool isArchive, const QString& filename)
{
  // can open non-executables that are not archives
  return !isArchive && !isExecutableFilename(filename);
}

bool canExploreFile(bool isArchive, const QString&)
{
  // can explore anything as long as it's not in an archive
  return !isArchive;
}

bool canHideFile(bool isArchive, const QString& filename)
{
  if (isArchive) {
    // can't hide files from archives
    return false;
  }

  if (filename.endsWith(ModInfo::s_HiddenExt, Qt::CaseInsensitive)) {
    // already hidden
    return false;
  }

  return true;
}

bool canUnhideFile(bool isArchive, const QString& filename)
{
  if (isArchive) {
    // can't unhide files from archives
    return false;
  }

  if (!filename.endsWith(ModInfo::s_HiddenExt, Qt::CaseInsensitive)) {
    // already visible
    return false;
  }

  return true;
}

FileRenamer::RenameResults hideFile(FileRenamer& renamer, const QString& oldName)
{
  const QString newName = oldName + ModInfo::s_HiddenExt;
  return renamer.rename(oldName, newName);
}

FileRenamer::RenameResults unhideFile(FileRenamer& renamer, const QString& oldName)
{
  QString newName = oldName.left(oldName.length() - ModInfo::s_HiddenExt.length());
  return renamer.rename(oldName, newName);
}

FileRenamer::RenameResults restoreHiddenFilesRecursive(FileRenamer& renamer,
                                                       const QString& targetDir)
{
  FileRenamer::RenameResults results = FileRenamer::RESULT_OK;
  QDir currentDir                    = targetDir;
  for (QString hiddenFile :
       currentDir.entryList((QStringList() << "*" + ModInfo::s_HiddenExt),
                            QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot)) {

    QString oldName = currentDir.absoluteFilePath(hiddenFile);
    QString newName = oldName.left(oldName.length() - ModInfo::s_HiddenExt.length());

    auto partialResult = renamer.rename(oldName, newName);

    if (partialResult == FileRenamer::RESULT_CANCEL) {
      return FileRenamer::RESULT_CANCEL;
    }

    if (partialResult == FileRenamer::RESULT_SKIP) {
      results = FileRenamer::RESULT_SKIP;
    }
  }

  for (QString dirName : currentDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot)) {

    const QString dirPath = currentDir.absoluteFilePath(dirName);
    // recurse on childrend directories
    auto partialResult = restoreHiddenFilesRecursive(renamer, dirPath);

    if (partialResult == FileRenamer::RESULT_CANCEL) {
      return FileRenamer::RESULT_CANCEL;
    }

    if (partialResult == FileRenamer::RESULT_SKIP) {
      results = FileRenamer::RESULT_SKIP;
    }
  }
  return results;
}

ModInfoDialog::TabInfo::TabInfo(std::unique_ptr<ModInfoDialogTab> tab)
    : tab(std::move(tab)), realPos(-1), widget(nullptr)
{}

bool ModInfoDialog::TabInfo::isVisible() const
{
  return (realPos != -1);
}

ModInfoDialog::ModInfoDialog(OrganizerCore& core, PluginManager& plugins,
                             ModInfo::Ptr mod, ModListView* modListView,
                             QWidget* parent)
    : TutorableDialog("ModInfoDialog", parent), ui(new Ui::ModInfoDialog), m_core(core),
      m_plugins(plugins), m_modListView(modListView), m_initialTab(ModInfoTabIDs::None),
      m_arrangingTabs(false)
{
  ui->setupUi(this);

  {
    auto* sc = new QShortcut(QKeySequence::Delete, this);
    connect(sc, &QShortcut::activated, [&] {
      onDeleteShortcut();
    });
  }
  {
    auto* sc = new QShortcut(QKeySequence::MoveToNextPage, this);
    connect(sc, &QShortcut::activated, [&] {
      onNextMod();
    });
  }
  {
    auto* sc = new QShortcut(QKeySequence::MoveToPreviousPage, this);
    connect(sc, &QShortcut::activated, [&] {
      onPreviousMod();
    });
  }

  setMod(mod);
  createTabs();

  connect(ui->tabWidget, &QTabWidget::currentChanged, [&] {
    onTabSelectionChanged();
  });
  connect(ui->tabWidget->tabBar(), &QTabBar::tabMoved, [&] {
    onTabMoved();
  });
  connect(ui->close, &QPushButton::clicked, [&] {
    onCloseButton();
  });
  connect(ui->previousMod, &QPushButton::clicked, [&] {
    onPreviousMod();
  });
  connect(ui->nextMod, &QPushButton::clicked, [&] {
    onNextMod();
  });
}

ModInfoDialog::~ModInfoDialog() = default;

template <class T>
std::unique_ptr<ModInfoDialogTab> createTab(ModInfoDialog& d, ModInfoTabIDs id)
{
  return std::make_unique<T>(ModInfoDialogTabContext(
      d.m_core, d.m_plugins, &d, d.ui.get(), id, d.m_mod, d.getOrigin()));
}

void ModInfoDialog::createTabs()
{
  m_tabs.clear();

  m_tabs.push_back(createTab<TextFilesTab>(*this, ModInfoTabIDs::TextFiles));
  m_tabs.push_back(createTab<IniFilesTab>(*this, ModInfoTabIDs::IniFiles));
  m_tabs.push_back(createTab<ImagesTab>(*this, ModInfoTabIDs::Images));
  m_tabs.push_back(createTab<ESPsTab>(*this, ModInfoTabIDs::Esps));
  m_tabs.push_back(createTab<ConflictsTab>(*this, ModInfoTabIDs::Conflicts));
  m_tabs.push_back(createTab<CategoriesTab>(*this, ModInfoTabIDs::Categories));
  m_tabs.push_back(createTab<NexusTab>(*this, ModInfoTabIDs::Nexus));
  m_tabs.push_back(createTab<NotesTab>(*this, ModInfoTabIDs::Notes));
  m_tabs.push_back(createTab<FileTreeTab>(*this, ModInfoTabIDs::Filetree));

  // check for tabs in the ui not having a corresponding tab in the list
  int count = ui->tabWidget->count();
  if (count < 0 || count > static_cast<int>(m_tabs.size())) {
    log::error("mod info dialog has more tabs than expected");
    count = static_cast<int>(m_tabs.size());
  }

  // for each tab in the widget; connects the widgets with the tab objects
  //
  for (int i = 0; i < count; ++i) {
    auto& tabInfo = m_tabs[static_cast<std::size_t>(i)];

    // remembering tab information so tabs can be removed and re-added
    tabInfo.widget  = ui->tabWidget->widget(i);
    tabInfo.caption = ui->tabWidget->tabText(i);
    tabInfo.icon    = ui->tabWidget->tabIcon(i);

    connect(tabInfo.tab.get(), &ModInfoDialogTab::originModified, [this](int originID) {
      onOriginModified(originID);
    });

    connect(tabInfo.tab.get(), &ModInfoDialogTab::modOpen, [&](const QString& name) {
      setMod(name);
      update();
    });

    connect(tabInfo.tab.get(), &ModInfoDialogTab::hasDataChanged, [&] {
      setTabsColors();
    });

    connect(tabInfo.tab.get(), &ModInfoDialogTab::wantsFocus,
            [&, id = tabInfo.tab->tabID()] {
              switchToTab(id);
            });
  }
}

int ModInfoDialog::exec()
{
  GeometrySaver gs(Settings::instance(), this);
  restoreState();

  // whether to select the first tab; if the main window requested a specific
  // tab, it is selected when encountered in update()
  const auto noCustomTabRequested = (m_initialTab == ModInfoTabIDs::None);
  const auto requestedTab         = m_initialTab;

  update(true);

  if (noCustomTabRequested) {
    m_core.settings().widgets().restoreIndex(ui->tabWidget);
  }

  const int r = TutorableDialog::exec();
  saveState();

  return r;
}

void ModInfoDialog::setMod(ModInfo::Ptr mod)
{
  Q_ASSERT(mod);
  m_mod = mod;

  // resetting the first activation flag so selecting tabs will trigger it
  // again
  for (auto& tabInfo : m_tabs) {
    tabInfo.tab->resetFirstActivation();
  }
}

void ModInfoDialog::setMod(const QString& name)
{
  unsigned int index = ModInfo::getIndex(name);
  if (index == UINT_MAX) {
    log::error("failed to resolve mod name {}", name);
    return;
  }

  auto mod = ModInfo::getByIndex(index);
  if (!mod) {
    log::error("mod by index {} is null", index);
    return;
  }

  setMod(mod);
}

void ModInfoDialog::selectTab(ModInfoTabIDs id)
{
  if (!isVisible()) {
    // can't select a tab if the dialog hasn't been properly updated yet
    m_initialTab = id;
    return;
  }

  switchToTab(id);
}

ModInfoDialog::TabInfo* ModInfoDialog::currentTab()
{
  const auto index = ui->tabWidget->currentIndex();
  if (index < 0) {
    return nullptr;
  }

  // looking for the actual tab at that position
  for (auto& tabInfo : m_tabs) {
    if (tabInfo.realPos == index) {
      return &tabInfo;
    }
  }

  return nullptr;
}

void ModInfoDialog::update(bool firstTime)
{
  // remembering the current selection, will be restored if the tab still
  // exists
  const int oldTab = ui->tabWidget->currentIndex();

  setWindowTitle(m_mod->name());

  // rebuilding the tab widget if needed depending on what tabs are valid for
  // the current mod
  setTabsVisibility(firstTime);

  // updating the data in all tabs
  updateTabs();

  // switching to the initial tab, if any
  if (m_initialTab != ModInfoTabIDs::None) {
    switchToTab(m_initialTab);
    m_initialTab = ModInfoTabIDs::None;
  }

  if (ui->tabWidget->currentIndex() == oldTab) {
    if (auto* tabInfo = currentTab()) {
      // activated() has to be fired manually because the tab index hasn't been
      // changed
      tabInfo->tab->activated();
    } else {
      log::error("tab index {} not found", oldTab);
    }
  }
}

void ModInfoDialog::setTabsVisibility(bool firstTime)
{
  // this flag is picked up by onTabSelectionChanged() to avoid triggering
  // activation events while moving tabs around
  QScopedValueRollback arrangingTabs(m_arrangingTabs, true);

  // one bool per tab to indicate whether the tab should be visible
  std::vector<bool> visibility(m_tabs.size());

  bool changed = false;

  for (std::size_t i = 0; i < m_tabs.size(); ++i) {
    const auto& tabInfo = m_tabs[i];

    bool visible = true;

    // a tab is visible if it can handle the current mod
    if (m_mod->hasFlag(ModInfo::FLAG_FOREIGN)) {
      visible = tabInfo.tab->canHandleUnmanaged();
    } else if (m_mod->hasFlag(ModInfo::FLAG_SEPARATOR)) {
      visible = tabInfo.tab->canHandleSeparators();
    }

    // if the visibility of this tab is changing, set changed to true because
    // the tabs have to be rebuilt
    const auto currentlyVisible = (ui->tabWidget->indexOf(tabInfo.widget) != -1);
    if (visible != currentlyVisible) {
      changed = true;
    }

    visibility[i] = visible;
  }

  // the tabs have to be rebuilt the first time the dialog is shown, or when
  // the visibility of any tab has changed
  if (!firstTime && !changed) {
    return;
  }

  // save the current order (if necessary) because some tabs will be removed and
  // others added
  if (!firstTime) {
    // but don't do it the first time visibility is set because the tabs are
    // in the default order, which will clobber the current settings
    saveTabOrder();
  }

  // remember selection, if any
  auto sel = ModInfoTabIDs::None;
  if (const auto* tabInfo = currentTab()) {
    sel = tabInfo->tab->tabID();
  }

  // removes all tabs and re-adds the visible ones
  reAddTabs(visibility, sel);
}

void ModInfoDialog::reAddTabs(const std::vector<bool>& visibility, ModInfoTabIDs sel)
{
  Q_ASSERT(visibility.size() == m_tabs.size());

  // ordered tab names from settings
  const auto orderedNames = m_core.settings().geometry().modInfoTabOrder();

  // whether the tabs can be sorted
  //
  // if the object name of a tab widget is not found in orderedNames, the list
  // cannot be sorted safely; if the list is empty, it's probably a first run
  // and there's nothing to sort
  bool canSort = !orderedNames.empty();

  // gathering visible tabs
  std::vector<TabInfo*> visibleTabs;
  for (std::size_t i = 0; i < m_tabs.size(); ++i) {
    if (!visibility[i]) {
      // this tab is not visible, skip it
      continue;
    }

    // this tab is visible
    visibleTabs.push_back(&m_tabs[i]);

    if (canSort) {
      // make sure the widget object name is found in the list
      const auto objectName = m_tabs[i].widget->objectName();
      auto itor = std::find(orderedNames.begin(), orderedNames.end(), objectName);

      if (itor == orderedNames.end()) {
        // this shouldn't happen, it means there's a tab in the UI that's no
        // in the list
        log::error("can't sort tabs, '{}' not found", objectName);
        canSort = false;
      }
    }
  }

  // sorting tabs
  if (canSort) {
    std::sort(visibleTabs.begin(), visibleTabs.end(), [&](auto&& a, auto&& b) {
      // looking the names in the ordered list
      auto aItor =
          std::find(orderedNames.begin(), orderedNames.end(), a->widget->objectName());

      auto bItor =
          std::find(orderedNames.begin(), orderedNames.end(), b->widget->objectName());

      // this shouldn't happen, it was checked above
      Q_ASSERT(aItor != orderedNames.end() && bItor != orderedNames.end());

      return (aItor < bItor);
    });
  }

  // removing all tabs
  ui->tabWidget->clear();

  // reset real positions
  for (auto& tabInfo : m_tabs) {
    tabInfo.realPos = -1;
  }

  // add visible tabs
  for (std::size_t i = 0; i < visibleTabs.size(); ++i) {
    auto& tabInfo = *visibleTabs[i];

    // remembering real position
    tabInfo.realPos = static_cast<int>(i);

    // adding tab
    ui->tabWidget->addTab(tabInfo.widget, tabInfo.icon, tabInfo.caption);

    // selecting
    if (tabInfo.tab->tabID() == sel) {
      ui->tabWidget->setCurrentIndex(static_cast<int>(i));
    }
  }
}

void ModInfoDialog::updateTabs(bool becauseOriginChanged)
{
  auto* origin = getOrigin();

  // list of tabs that should be updated
  std::vector<TabInfo*> interestedTabs;

  for (auto& tabInfo : m_tabs) {
    // don't touch invisible tabs
    if (!tabInfo.isVisible()) {
      continue;
    }

    // updateTabs() is also called from onOriginModified() to update all the
    // tabs that depend on the origin; if updateTabs() is called because the
    // origin changed, but the tab doesn't use origin files, it can be safely
    // skipped
    //
    // this happens for tabs like notes and categories, which don't need to
    // be updated when files change
    if (becauseOriginChanged && !tabInfo.tab->usesOriginFiles()) {
      continue;
    }

    // this tab should be updated
    interestedTabs.push_back(&tabInfo);
  }

  for (auto* tabInfo : interestedTabs) {
    // set the current mod
    tabInfo->tab->setMod(m_mod, origin);

    // clear
    tabInfo->tab->clear();
  }

  // feed all the files from the filesystem
  feedFiles(interestedTabs);

  // call update() on all tabs
  for (auto* tabInfo : interestedTabs) {
    tabInfo->tab->update();
  }

  // update the text colours
  setTabsColors();
}

void ModInfoDialog::feedFiles(std::vector<TabInfo*>& interestedTabs)
{
  const auto rootPath = m_mod->absolutePath();
  if (rootPath.isEmpty()) {
    return;
  }

  const fs::path fsPath(rootPath.toStdWString());

  for (const auto& entry : fs::recursive_directory_iterator(fsPath)) {
    if (!entry.is_regular_file()) {
      // skip directories
      continue;
    }

    const auto filePath = QString::fromStdWString(entry.path().native());

    // for each tab
    for (auto* tabInfo : interestedTabs) {
      if (tabInfo->tab->feedFile(rootPath, filePath)) {
        break;
      }
    }
  }
}

void ModInfoDialog::setTabsColors()
{
  const auto p = m_modListView->parentWidget()->palette();

  for (const auto& tabInfo : m_tabs) {
    if (!tabInfo.isVisible()) {
      // don't bother with invisible tabs
      continue;
    }

    const QColor color = tabInfo.tab->hasData()
                             ? QColor::Invalid
                             : p.color(QPalette::Disabled, QPalette::WindowText);

    ui->tabWidget->tabBar()->setTabTextColor(tabInfo.realPos, color);
  }
}

void ModInfoDialog::switchToTab(ModInfoTabIDs id)
{
  // look a tab with the given id
  for (const auto& tabInfo : m_tabs) {
    if (tabInfo.tab->tabID() == id) {
      // use realPos to select the proper tab in the widget
      ui->tabWidget->setCurrentIndex(tabInfo.realPos);
      return;
    }
  }

  // this could happen if the tab is not visible right now
  log::debug("can't switch to tab ID {}, not available", static_cast<int>(id));
}

MOShared::FilesOrigin* ModInfoDialog::getOrigin()
{
  auto* ds = m_core.directoryStructure();

  if (!ds->originExists(m_mod->name().toStdWString())) {
    return nullptr;
  }

  auto* origin = &ds->getOriginByName(m_mod->name().toStdWString());
  if (origin->isDisabled()) {
    return nullptr;
  }

  return origin;
}

void ModInfoDialog::saveState() const
{
  saveTabOrder();

  // save state for each tab
  for (const auto& tabInfo : m_tabs) {
    tabInfo.tab->saveState(m_core.settings());
  }
}

void ModInfoDialog::restoreState()
{
  // tab order is not restored here, it will be picked up if tabs have to be
  // removed and re-added

  // restore state for each tab
  for (const auto& tabInfo : m_tabs) {
    tabInfo.tab->restoreState(m_core.settings());
  }
}

void ModInfoDialog::saveTabOrder() const
{
  if (static_cast<int>(m_tabs.size()) != ui->tabWidget->count()) {
    // only save tab state when all tabs are visible
    //
    // if not all tabs are visible, it becomes very difficult to figure out in
    // what order the user wants these tabs to be, so just avoid saving it
    // completely
    //
    // this means that reordering tabs when not all tabs are visible is not
    // saved, but it's better than breaking everything
    return;
  }

  QString names;

  for (int i = 0; i < ui->tabWidget->count(); ++i) {
    if (!names.isEmpty()) {
      names += " ";
    }

    names += ui->tabWidget->widget(i)->objectName();
  }

  m_core.settings().geometry().setModInfoTabOrder(names);
  // save last opened index
  m_core.settings().widgets().saveIndex(ui->tabWidget);
}

void ModInfoDialog::onOriginModified(int originID)
{
  // tell the main window the origin changed
  emit originModified(originID);

  // update tabs that depend on the origin
  updateTabs(true);
}

void ModInfoDialog::onDeleteShortcut()
{
  // forward the request to the current tab
  if (auto* tabInfo = currentTab()) {
    tabInfo->tab->deleteRequested();
  }
}

void ModInfoDialog::closeEvent(QCloseEvent* e)
{
  if (tryClose()) {
    e->accept();
  } else {
    e->ignore();
  }
}

void ModInfoDialog::onCloseButton()
{
  if (tryClose()) {
    close();
  }
}

bool ModInfoDialog::tryClose()
{
  // cancel the close if any tab returns false; for example. this can happen if
  // a tab has unsaved content, pops a confirmation dialog, and the user clicks
  // cancel

  for (auto& tabInfo : m_tabs) {
    if (!tabInfo.tab->canClose()) {
      return false;
    }
  }

  return true;
}

void ModInfoDialog::onTabSelectionChanged()
{
  if (m_arrangingTabs) {
    // this can be fired while re-arranging tabs, which happens before mods
    // are given to tabs, and might trigger first activation, which breaks all
    // sorts of things
    return;
  }

  // this will call firstActivation() on the tab if needed
  if (auto* tabInfo = currentTab()) {
    tabInfo->tab->activated();
  }
}

void ModInfoDialog::onTabMoved()
{
  // reset
  for (auto& tabInfo : m_tabs) {
    tabInfo.realPos = -1;
  }

  // for each tab in the widget
  for (int i = 0; i < ui->tabWidget->count(); ++i) {
    const auto* w = ui->tabWidget->widget(i);

    bool found = false;

    // find the corresponding tab info
    for (auto& tabInfo : m_tabs) {
      if (tabInfo.widget == w) {
        tabInfo.realPos = i;
        found           = true;
        break;
      }
    }

    if (!found) {
      log::error("unknown tab at index {}", i);
    }
  }
}

void ModInfoDialog::onNextMod()
{
  auto index = m_modListView->nextMod(ModInfo::getIndex(m_mod->name()));
  if (!index) {
    return;
  }
  auto mod = ModInfo::getByIndex(*index);
  if (!mod || mod == m_mod) {
    return;
  }

  setMod(mod);
  update();

  emit modChanged(*index);
}

void ModInfoDialog::onPreviousMod()
{
  auto index = m_modListView->prevMod(ModInfo::getIndex(m_mod->name()));
  if (!index) {
    return;
  }
  auto mod = ModInfo::getByIndex(*index);

  setMod(mod);
  update();

  emit modChanged(*index);
}
