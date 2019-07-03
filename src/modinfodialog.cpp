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
#include "ui_modinfodialog.h"
#include "plugincontainer.h"
#include "organizercore.h"
#include "mainwindow.h"
#include "modinfodialogtextfiles.h"
#include "modinfodialogimages.h"
#include "modinfodialogesps.h"
#include "modinfodialogconflicts.h"
#include "modinfodialogcategories.h"
#include "modinfodialognexus.h"
#include "modinfodialogfiletree.h"
#include <filesystem>

using namespace MOBase;
using namespace MOShared;
namespace fs = std::filesystem;

const int max_scan_for_context_menu = 50;


int naturalCompare(const QString& a, const QString& b)
{
  static QCollator c = []{
    QCollator c;
    c.setNumericMode(true);
    c.setCaseSensitivity(Qt::CaseInsensitive);
    return c;
  }();

  return c.compare(a, b);
}

bool canPreviewFile(
  PluginContainer& pluginContainer, bool isArchive, const QString& filename)
{
  if (isArchive) {
    return false;
  }

  const auto ext = QFileInfo(filename).suffix();
  return pluginContainer.previewGenerator().previewSupported(ext);
}

bool canOpenFile(bool isArchive, const QString&)
{
  // can open anything as long as it's not in an archive
  return !isArchive;
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

  if (filename.endsWith(ModInfo::s_HiddenExt)) {
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

  if (!filename.endsWith(ModInfo::s_HiddenExt)) {
    // already visible
    return false;
  }

  return true;
}

FileRenamer::RenameResults hideFile(FileRenamer& renamer, const QString &oldName)
{
  const QString newName = oldName + ModInfo::s_HiddenExt;
  return renamer.rename(oldName, newName);
}

FileRenamer::RenameResults unhideFile(FileRenamer& renamer, const QString &oldName)
{
  QString newName = oldName.left(oldName.length() - ModInfo::s_HiddenExt.length());
  return renamer.rename(oldName, newName);
}


ModInfoDialog::TabInfo::TabInfo(std::unique_ptr<ModInfoDialogTab> tab)
  : tab(std::move(tab)), realPos(-1), widget(nullptr)
{
}

bool ModInfoDialog::TabInfo::isVisible() const
{
  return (realPos != -1);
}


ModInfoDialog::ModInfoDialog(
  MainWindow* mw, OrganizerCore* core, PluginContainer* plugin,
  ModInfo::Ptr mod) :
    TutorableDialog("ModInfoDialog", mw),
    ui(new Ui::ModInfoDialog), m_mainWindow(mw),
    m_core(core), m_plugin(plugin), m_initialTab(ModInfoTabIDs::None),
    m_arrangingTabs(false)
{
  ui->setupUi(this);

  auto* sc = new QShortcut(QKeySequence::Delete, this);
  connect(sc, &QShortcut::activated, [&]{ onDeleteShortcut(); });

  setMod(mod);
  createTabs();

  connect(ui->tabWidget, &QTabWidget::currentChanged, [&]{ onTabSelectionChanged(); });
  connect(ui->tabWidget->tabBar(), &QTabBar::tabMoved, [&]{ onTabMoved(); });
  connect(ui->close, &QPushButton::clicked, [&]{ onCloseButton(); });
  connect(ui->previousMod, &QPushButton::clicked, [&]{ onPreviousMod(); });
  connect(ui->nextMod, &QPushButton::clicked, [&]{ onNextMod(); });
}

ModInfoDialog::~ModInfoDialog() = default;

template <class T>
std::unique_ptr<ModInfoDialogTab> createTab(ModInfoDialog& d, ModInfoTabIDs id)
{
  return std::make_unique<T>(ModInfoDialogTabContext(
    *d.m_core, *d.m_plugin, &d, d.ui.get(), id, d.m_mod, d.getOrigin()));
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
    qCritical() << "mod info dialog has more tabs than expected";
    count = static_cast<int>(m_tabs.size());
  }

  // for each tab in the widget; connects the widgets with the tab objects
  //
  for (int i=0; i<count; ++i) {
    auto& tabInfo = m_tabs[static_cast<std::size_t>(i)];

    // remembering tab information so tabs can be removed and re-added
    tabInfo.widget = ui->tabWidget->widget(i);
    tabInfo.caption = ui->tabWidget->tabText(i);
    tabInfo.icon = ui->tabWidget->tabIcon(i);

    connect(
      tabInfo.tab.get(), &ModInfoDialogTab::originModified,
      [this](int originID){ onOriginModified(originID); });

    connect(
      tabInfo.tab.get(), &ModInfoDialogTab::modOpen,
      [&](const QString& name){ setMod(name); update(); });

    connect(
      tabInfo.tab.get(), &ModInfoDialogTab::hasDataChanged,
      [&]{ setTabsColors(); });

    connect(
      tabInfo.tab.get(), &ModInfoDialogTab::wantsFocus,
      [&, id=tabInfo.tab->tabID()]{ switchToTab(id); });
  }
}

int ModInfoDialog::exec()
{
  // whether to select the first tab; if the main window requested a specific
  // tab, it is selected when encountered in update()
  const auto selectFirst = (m_initialTab == ModInfoTabIDs::None);

  update(true);

  if (selectFirst && ui->tabWidget->count() > 0) {
    ui->tabWidget->setCurrentIndex(0);
  }

  return TutorableDialog::exec();
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
    qCritical() << "failed to resolve mod name " << name;
    return;
  }

  auto mod = ModInfo::getByIndex(index);
  if (!mod) {
    qCritical() << "mod by index " << index << " is null";
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
    if (auto* tabInfo=currentTab()) {
      // activated() has to be fired manually because the tab index hasn't been
      // changed
      tabInfo->tab->activated();
    } else {
      qCritical() << "tab index " << oldTab << " not found";
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

  for (std::size_t i=0; i<m_tabs.size(); ++i) {
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
    saveTabOrder(Settings::instance());
  }

  // remember selection, if any
  auto sel = ModInfoTabIDs::None;
  if (const auto* tabInfo=currentTab()) {
    sel = tabInfo->tab->tabID();
  }

  // removes all tabs and re-adds the visible ones
  reAddTabs(visibility, sel);
}

void ModInfoDialog::reAddTabs(
  const std::vector<bool>& visibility, ModInfoTabIDs sel)
{
  Q_ASSERT(visibility.size() == m_tabs.size());

  // ordered tab names from settings
  const auto orderedNames = getOrderedTabNames();

  // whether the tabs can be sorted; if the object name of a tab widget is not
  // found in orderedNames, the list cannot be sorted safely
  bool canSort = true;

  // gathering visible tabs
  std::vector<TabInfo*> visibleTabs;
  for (std::size_t i=0; i<m_tabs.size(); ++i) {
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
        qCritical() << "can't sort tabs, '" << objectName << "' not found";
        canSort = false;
      }
    }
  }

  // sorting tabs
  if (canSort) {
    std::sort(visibleTabs.begin(), visibleTabs.end(), [&](auto&& a, auto&& b){
      // looking the names in the ordered list
      auto aItor = std::find(
        orderedNames.begin(), orderedNames.end(),
        a->widget->objectName());

      auto bItor = std::find(
        orderedNames.begin(), orderedNames.end(),
        b->widget->objectName());

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
  for (std::size_t i=0; i<visibleTabs.size(); ++i) {
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
  const auto p = m_mainWindow->palette();

  for (const auto& tabInfo : m_tabs) {
    if (!tabInfo.isVisible()) {
      // don't bother with invisible tabs
      continue;
    }

    const QColor color = tabInfo.tab->hasData() ?
      QColor::Invalid :
      p.color(QPalette::Disabled, QPalette::WindowText);

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
  qDebug()
    << "can't switch to tab ID " << static_cast<int>(id)
    << ", not available";
}

MOShared::FilesOrigin* ModInfoDialog::getOrigin()
{
  auto* ds = m_core->directoryStructure();

  if (!ds->originExists(m_mod->name().toStdWString())) {
    return nullptr;
  }

  auto* origin = &ds->getOriginByName(m_mod->name().toStdWString());
  if (origin->isDisabled()) {
    return nullptr;
  }

  return origin;
}

void ModInfoDialog::saveState(Settings& s) const
{
  saveTabOrder(s);

  // remove 2.2.0 settings
  s.directInterface().remove("mod_info_tabs");
  s.directInterface().remove("mod_info_conflict_expanders");
  s.directInterface().remove("mod_info_conflicts");
  s.directInterface().remove("mod_info_advanced_conflicts");
  s.directInterface().remove("mod_info_conflicts_overwrite");
  s.directInterface().remove("mod_info_conflicts_noconflict");
  s.directInterface().remove("mod_info_conflicts_overwritten");

  // save state for each tab
  for (const auto& tabInfo : m_tabs) {
    tabInfo.tab->saveState(s);
  }
}

void ModInfoDialog::restoreState(const Settings& s)
{
  // tab order is not restored here, it will be picked up if tabs have to be
  // removed and re-added

  // restore state for each tab
  for (const auto& tabInfo : m_tabs) {
    tabInfo.tab->restoreState(s);
  }
}

void ModInfoDialog::saveTabOrder(Settings& s) const
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

  for (int i=0; i<ui->tabWidget->count(); ++i) {
    if (!names.isEmpty()) {
      names += " ";
    }

    names += ui->tabWidget->widget(i)->objectName();
  }

  s.directInterface().setValue("mod_info_tab_order", names);
}

std::vector<QString> ModInfoDialog::getOrderedTabNames() const
{
  const auto& settings = Settings::instance().directInterface();

  std::vector<QString> v;

  if (settings.contains("mod_info_tabs")) {
    // old byte array from 2.2.0
    QDataStream stream(settings.value("mod_info_tabs").toByteArray());

    int count = 0;
    stream >> count;

    for (int i=0; i<count; ++i) {
      QString s;
      stream >> s;
      v.emplace_back(std::move(s));
    }
  } else {
    // string list
    QString string = settings.value("mod_info_tab_order").toString();
    QTextStream stream(&string);

    while (!stream.atEnd()) {
      QString s;
      stream >> s;
      v.emplace_back(std::move(s));
    }
  }

  return v;
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
  if (auto* tabInfo=currentTab()) {
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
  if (auto* tabInfo=currentTab()) {
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
  for (int i=0; i<ui->tabWidget->count(); ++i) {
    const auto* w = ui->tabWidget->widget(i);

    bool found = false;

    // find the corresponding tab info
    for (auto& tabInfo : m_tabs) {
      if (tabInfo.widget == w) {
        tabInfo.realPos = i;
        found = true;
        break;
      }
    }

    if (!found) {
      qCritical() << "unknown tab at index " << i;
    }
  }
}

void ModInfoDialog::onNextMod()
{
  auto mod = m_mainWindow->nextModInList();
  if (!mod || mod == m_mod) {
    return;
  }

  setMod(mod);
  update();
}

void ModInfoDialog::onPreviousMod()
{
  auto mod = m_mainWindow->previousModInList();
  if (!mod || mod == m_mod) {
    return;
  }

  setMod(mod);
  update();
}
