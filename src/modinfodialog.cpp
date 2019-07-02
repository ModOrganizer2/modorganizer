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
    m_core(core), m_plugin(plugin), m_initialTab(ETabs(-1)),
    m_arrangingTabs(false)
{
  ui->setupUi(this);

  auto* sc = new QShortcut(QKeySequence::Delete, this);
  connect(sc, &QShortcut::activated, [&]{ onDeleteShortcut(); });

  setMod(mod);
  m_tabs = createTabs();

  for (int i=0; i<ui->tabWidget->count(); ++i) {
    if (static_cast<std::size_t>(i) >= m_tabs.size()) {
      qCritical() << "mod info dialog has more tabs than expected";
      break;
    }

    auto& tabInfo = m_tabs[static_cast<std::size_t>(i)];
    tabInfo.widget = ui->tabWidget->widget(i);
    tabInfo.caption = ui->tabWidget->tabText(i);
    tabInfo.icon = ui->tabWidget->tabIcon(i);

    connect(
      tabInfo.tab.get(), &ModInfoDialogTab::originModified,
      [this, i](int originID) {
        onOriginModified(originID);
      });

    connect(
      tabInfo.tab.get(), &ModInfoDialogTab::modOpen,
      [&](const QString& name){
        setMod(name);
        update();
      });

    connect(
      tabInfo.tab.get(), &ModInfoDialogTab::hasDataChanged,
      [&]{ setTabsColors(); });

    connect(
      tabInfo.tab.get(), &ModInfoDialogTab::wantsFocus,
      [&, i=static_cast<std::size_t>(i)]
      {
        if (i < m_tabs.size()) {
          switchToTab(ETabs(m_tabs[i].tab->tabID()));
        }
      });
  }

  connect(ui->tabWidget, &QTabWidget::currentChanged, [&]{ onTabChanged(); });
  connect(ui->tabWidget->tabBar(), &QTabBar::tabMoved, [&]{ onTabMoved(); });
}

ModInfoDialog::~ModInfoDialog() = default;

template <class T>
std::unique_ptr<ModInfoDialogTab> createTab(ModInfoDialog& d, int index)
{
  return std::make_unique<T>(ModInfoDialogTabContext(
    *d.m_core, *d.m_plugin, &d, d.ui.get(), index, d.m_mod, d.getOrigin()));
}

std::vector<ModInfoDialog::TabInfo> ModInfoDialog::createTabs()
{
  std::vector<TabInfo> v;

  v.push_back(createTab<TextFilesTab>(*this, TAB_TEXTFILES));
  v.push_back(createTab<IniFilesTab>(*this, TAB_INIFILES));
  v.push_back(createTab<ImagesTab>(*this, TAB_IMAGES));
  v.push_back(createTab<ESPsTab>(*this, TAB_ESPS));
  v.push_back(createTab<ConflictsTab>(*this, TAB_CONFLICTS));
  v.push_back(createTab<CategoriesTab>(*this, TAB_CATEGORIES));
  v.push_back(createTab<NexusTab>(*this, TAB_NEXUS));
  v.push_back(createTab<NotesTab>(*this, TAB_NOTES));
  v.push_back(createTab<FileTreeTab>(*this, TAB_FILETREE));

  return v;
}

int ModInfoDialog::exec()
{
  const auto selectFirst = (m_initialTab == -1);

  update(true);

  if (selectFirst) {
    if (ui->tabWidget->count() > 0) {
      ui->tabWidget->setCurrentIndex(0);
    }
  }

  return TutorableDialog::exec();
}

void ModInfoDialog::setMod(ModInfo::Ptr mod)
{
  Q_ASSERT(mod);
  m_mod = mod;

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

void ModInfoDialog::setTab(ETabs id)
{
  if (!isVisible()) {
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

  for (auto& tabInfo : m_tabs) {
    if (tabInfo.realPos == index) {
      return &tabInfo;
    }
  }

  qCritical() << "tab index " << index << " not found";
  return nullptr;
}

void ModInfoDialog::update(bool firstTime)
{
  const int oldTab = ui->tabWidget->currentIndex();

  setWindowTitle(m_mod->name());
  setTabsVisibility(firstTime);

  updateTabs();

  if (m_initialTab >= 0) {
    switchToTab(m_initialTab);
    m_initialTab = ETabs(-1);
  }

  if (ui->tabWidget->currentIndex() == oldTab) {
    // manually fire activated() because the tab index hasn't been changed
    if (auto* tabInfo=currentTab()) {
      tabInfo->tab->activated();
    }
  }
}

void ModInfoDialog::setTabsVisibility(bool firstTime)
{
  QScopedValueRollback arrangingTabs(m_arrangingTabs, true);

  std::vector<bool> visibility(m_tabs.size());

  bool changed = false;

  for (std::size_t i=0; i<m_tabs.size(); ++i) {
    const auto& tabInfo = m_tabs[i];

    bool visible = true;

    if (m_mod->hasFlag(ModInfo::FLAG_FOREIGN)) {
      visible = tabInfo.tab->canHandleUnmanaged();
    } else if (m_mod->hasFlag(ModInfo::FLAG_SEPARATOR)) {
      visible = tabInfo.tab->canHandleSeparators();
    }

    const auto currentlyVisible = (ui->tabWidget->indexOf(tabInfo.widget) != -1);

    if (visible != currentlyVisible) {
      changed = true;
    }

    visibility[i] = visible;
  }

  if (!firstTime && !changed) {
    return;
  }

  // remember selection
  const int selIndex = ui->tabWidget->currentIndex();
  ETabs sel = ETabs(-1);

  for (const auto& tabInfo : m_tabs) {
    if (tabInfo.realPos == selIndex) {
      sel = ETabs(tabInfo.tab->tabID());
      break;
    }
  }

  reAddTabs(visibility, sel);
}

void ModInfoDialog::updateTabs(bool becauseOriginChanged)
{
  auto* origin = getOrigin();

  for (auto& tabInfo : m_tabs) {
    if (!tabInfo.isVisible()) {
      continue;
    }

    if (becauseOriginChanged && !tabInfo.tab->usesOriginFiles()) {
      continue;
    }

    tabInfo.tab->setMod(m_mod, origin);
    tabInfo.tab->clear();
  }


  feedFiles(becauseOriginChanged);

  for (auto& tabInfo : m_tabs) {
    if (tabInfo.isVisible()) {
      tabInfo.tab->update();
    }
  }

  setTabsColors();
}

void ModInfoDialog::feedFiles(bool becauseOriginChanged)
{
  namespace fs = std::filesystem;

  const auto rootPath = m_mod->absolutePath();

  if (rootPath.length() > 0) {
    fs::path path(rootPath.toStdWString());
    for (const auto& entry : fs::recursive_directory_iterator(path)) {
      if (!entry.is_regular_file()) {
        continue;
      }

      const auto fileName = QString::fromWCharArray(
        entry.path().native().c_str());

      for (auto& tabInfo : m_tabs) {
        if (!tabInfo.isVisible()) {
          continue;
        }

        if (becauseOriginChanged && !tabInfo.tab->usesOriginFiles()) {
          continue;
        }

        if (tabInfo.tab->feedFile(rootPath, fileName)) {
          break;
        }
      }
    }
  }
}

void ModInfoDialog::setTabsColors()
{
  for (const auto& tabInfo : m_tabs) {
    const auto c = tabInfo.tab->hasData() ?
      QColor::Invalid :
      m_mainWindow->palette().color(QPalette::Disabled, QPalette::WindowText);

    ui->tabWidget->tabBar()->setTabTextColor(tabInfo.realPos, c);
  }
}

void ModInfoDialog::switchToTab(ETabs id)
{
  for (const auto& tabInfo : m_tabs) {
    if (tabInfo.tab->tabID() == id) {
      ui->tabWidget->setCurrentIndex(tabInfo.realPos);
      return;
    }
  }

  qDebug() << "can't switch to tab " << id << ", not available";
}

MOShared::FilesOrigin* ModInfoDialog::getOrigin()
{
  MOShared::FilesOrigin* origin = nullptr;

  auto* ds = m_core->directoryStructure();
  if (ds->originExists(ToWString(m_mod->name()))) {
    auto* origin = &ds->getOriginByName(ToWString(m_mod->name()));
    if (!origin->isDisabled()) {
      return origin;
    }
  }

  return nullptr;
}

void ModInfoDialog::saveState(Settings& s) const
{
  const auto tabState = saveTabState();
  if (!tabState.isEmpty()) {
    s.directInterface().setValue("mod_info_tab_order", tabState);
  }

  // remove 2.2.0 settings
  s.directInterface().remove("mod_info_tabs");
  s.directInterface().remove("mod_info_conflict_expanders");
  s.directInterface().remove("mod_info_conflicts");
  s.directInterface().remove("mod_info_advanced_conflicts");
  s.directInterface().remove("mod_info_conflicts_overwrite");
  s.directInterface().remove("mod_info_conflicts_noconflict");
  s.directInterface().remove("mod_info_conflicts_overwritten");

  for (const auto& tabInfo : m_tabs) {
    tabInfo.tab->saveState(s);
  }
}

void ModInfoDialog::restoreState(const Settings& s)
{
  for (const auto& tabInfo : m_tabs) {
    tabInfo.tab->restoreState(s);
  }
}

QString ModInfoDialog::saveTabState() const
{
  if (static_cast<int>(m_tabs.size()) != ui->tabWidget->count()) {
    // only save tab state when all tabs are visible
    return {};
  }

  QString result;
  QTextStream stream(&result);

  for (int i=0; i<ui->tabWidget->count(); ++i) {
    stream << ui->tabWidget->widget(i)->objectName() << " ";
  }

  return result.trimmed();
}

std::vector<QString> ModInfoDialog::getOrderedTabNames() const
{
  const auto& settings = Settings::instance().directInterface();

  std::vector<QString> v;

  if (settings.contains("mod_info_tabs")) {
    // old byte array
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

void ModInfoDialog::reAddTabs(const std::vector<bool>& visibility, ETabs sel)
{
  Q_ASSERT(visibility.size() == m_tabs.size());

  // ordered tab names from settings
  const auto orderedNames = getOrderedTabNames();

  bool canSort = true;

  // gathering visible tabs
  std::vector<TabInfo*> visibleTabs;
  for (std::size_t i=0; i<m_tabs.size(); ++i) {
    if (visibility[i]) {
      visibleTabs.push_back(&m_tabs[i]);

      if (canSort) {
        const auto objectName = m_tabs[i].widget->objectName();
        auto itor = std::find(orderedNames.begin(), orderedNames.end(), objectName);
        if (itor == orderedNames.end()) {
          qCritical() << "can't sort tabs, '" << objectName << "' not found";
          canSort = false;
        }
      }
    }
  }

  // sorting tabs
  if (canSort) {
    std::sort(visibleTabs.begin(), visibleTabs.end(), [&](auto&& a, auto&& b){
      auto aItor = std::find(orderedNames.begin(), orderedNames.end(), a->widget->objectName());
      auto bItor = std::find(orderedNames.begin(), orderedNames.end(), b->widget->objectName());

      // this was checked above
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

    tabInfo.realPos = static_cast<int>(i);
    ui->tabWidget->addTab(tabInfo.widget, tabInfo.icon, tabInfo.caption);

    if (tabInfo.tab->tabID() == sel) {
      ui->tabWidget->setCurrentIndex(static_cast<int>(i));
    }
  }
}

void ModInfoDialog::onOriginModified(int originID)
{
  emit originModified(originID);
  updateTabs(true);
}

void ModInfoDialog::onDeleteShortcut()
{
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

void ModInfoDialog::on_closeButton_clicked()
{
  if (tryClose()) {
    close();
  }
}

bool ModInfoDialog::tryClose()
{
  for (auto& tabInfo : m_tabs) {
    if (!tabInfo.tab->canClose()) {
      return false;
    }
  }

  return true;
}

void ModInfoDialog::onTabChanged()
{
  if (m_arrangingTabs) {
    // this can be fired while re-arranging tabs, which happens before mods
    // are given to tabs, and might trigger first activation, which breaks all
    // sorts of things
    return;
  }

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

void ModInfoDialog::on_nextButton_clicked()
{
  auto mod = m_mainWindow->nextModInList();
  if (!mod || mod == m_mod) {
    return;
  }

  setMod(mod);
  update();
}

void ModInfoDialog::on_prevButton_clicked()
{
  auto mod = m_mainWindow->previousModInList();
  if (!mod || mod == m_mod) {
    return;
  }

  setMod(mod);
  update();
}
