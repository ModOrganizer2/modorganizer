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
  MainWindow* mw, OrganizerCore* core, PluginContainer* plugin) :
    TutorableDialog("ModInfoDialog", mw),
    ui(new Ui::ModInfoDialog), m_mainWindow(mw),
    m_core(core), m_plugin(plugin), m_initialTab(ETabs(-1))
{
  ui->setupUi(this);

  auto* sc = new QShortcut(QKeySequence::Delete, this);
  connect(sc, &QShortcut::activated, [&]{ onDeleteShortcut(); });

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
        onOriginModified(static_cast<std::size_t>(i), originID);
      });

    connect(
      tabInfo.tab.get(), &ModInfoDialogTab::modOpen,
      [&](const QString& name){
        setMod(name);
        update();
      });
  }
}

ModInfoDialog::~ModInfoDialog() = default;

std::vector<ModInfoDialog::TabInfo> ModInfoDialog::createTabs()
{
  std::vector<TabInfo> v;

  v.push_back(createTab<TextFilesTab>(TAB_TEXTFILES));
  v.push_back(createTab<IniFilesTab>(TAB_INIFILES));
  v.push_back(createTab<ImagesTab>(TAB_IMAGES));
  v.push_back(createTab<ESPsTab>(TAB_ESPS));
  v.push_back(createTab<ConflictsTab>(TAB_CONFLICTS));
  v.push_back(createTab<CategoriesTab>(TAB_CATEGORIES));
  v.push_back(createTab<NexusTab>(TAB_NEXUS));
  v.push_back(createTab<NotesTab>(TAB_NOTES));
  v.push_back(createTab<FileTreeTab>(TAB_FILETREE));

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
  m_mod = mod;
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

void ModInfoDialog::update(bool firstTime)
{
  setWindowTitle(m_mod->name());
  setTabsVisibility(firstTime);

  updateTabs();

  if (m_initialTab >= 0) {
    switchToTab(m_initialTab);
    m_initialTab = ETabs(-1);
  }
}

void ModInfoDialog::setTabsVisibility(bool firstTime)
{
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
    tabInfo.tab->update();
  }

  setTabsColors();
}

void ModInfoDialog::feedFiles(bool becauseOriginChanged)
{
  namespace fs = std::filesystem;
  const auto rootPath = m_mod->absolutePath();

  if (rootPath.length() > 0) {

    for (const auto& entry : fs::recursive_directory_iterator(rootPath.toStdString())) {
      if (!entry.is_regular_file()) {
        continue;
      }

      const auto fileName = QString::fromWCharArray(entry.path().c_str());

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
      ui->tabWidget->palette().color(QPalette::Disabled, QPalette::WindowText);

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

void ModInfoDialog::onOriginModified(std::size_t tabIndex, int originID)
{
  emit originModified(originID);
  updateTabs(true);
}

void ModInfoDialog::onDeleteShortcut()
{
  for (auto& tabInfo : m_tabs) {
    if (tabInfo.tab->deleteRequested()) {
      break;
    }
  }
}

void ModInfoDialog::on_closeButton_clicked()
{
  for (auto& tabInfo : m_tabs) {
    if (!tabInfo.tab->canClose()) {
      return;
    }
  }

  close();
}

void ModInfoDialog::on_tabWidget_currentChanged(int index)
{
}

void ModInfoDialog::on_nextButton_clicked()
{
  auto mod = m_mainWindow->nextModInList();
  if (mod == m_mod) {
    return;
  }

  setMod(mod);
  update();
}

void ModInfoDialog::on_prevButton_clicked()
{
  auto mod = m_mainWindow->previousModInList();
  if (mod == m_mod) {
    return;
  }

  setMod(mod);
  update();
}
