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
#include "modinfodialogtextfiles.h"
#include "modinfodialogimages.h"
#include "modinfodialogesps.h"
#include "modinfodialogconflicts.h"
#include "modinfodialogcategories.h"
#include "modinfodialognexus.h"
#include "modinfodialogfiletree.h"

using namespace MOBase;
using namespace MOShared;

const int max_scan_for_context_menu = 50;


class ModFileListWidget : public QListWidgetItem {
  friend bool operator<(const ModFileListWidget &LHS, const ModFileListWidget &RHS);
public:
  ModFileListWidget(const QString &text, int sortValue, QListWidget *parent = 0)
    : QListWidgetItem(text, parent, QListWidgetItem::UserType + 1), m_SortValue(sortValue) {}
private:
  int m_SortValue;
};


static bool operator<(const ModFileListWidget &LHS, const ModFileListWidget &RHS)
{
  return LHS.m_SortValue < RHS.m_SortValue;
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


ModInfoDialog::ModInfoDialog(
  ModInfo::Ptr modInfo, bool unmanaged, OrganizerCore *organizerCore,
  PluginContainer *pluginContainer, QWidget *parent) :
    TutorableDialog("ModInfoDialog", parent), ui(new Ui::ModInfoDialog),
    m_ModInfo(modInfo), m_RootPath(modInfo->absolutePath()),
    m_OrganizerCore(organizerCore), m_PluginContainer(pluginContainer),
    m_Origin(nullptr)
{
  ui->setupUi(this);

  auto* ds = m_OrganizerCore->directoryStructure();
  if (ds->originExists(ToWString(m_ModInfo->name()))) {
    m_Origin = &ds->getOriginByName(ToWString(m_ModInfo->name()));
    if (m_Origin->isDisabled()) {
      m_Origin = nullptr;
    }
  }

  this->setWindowTitle(m_ModInfo->name());
  this->setWindowModality(Qt::WindowModal);

  auto* sc = new QShortcut(QKeySequence::Delete, this);
  connect(sc, &QShortcut::activated, [&]{ onDeleteShortcut(); });

  m_tabs = createTabs();
  bool tabSelected = false;

  for (std::size_t i=0; i<m_tabs.size(); ++i) {
    connect(
      m_tabs[i].get(), &ModInfoDialogTab::originModified,
      [&](int originID){ emit originModified(originID); });

    connect(
      m_tabs[i].get(), &ModInfoDialogTab::modOpen,
      [&](const QString& name){
        close();
        emit modOpen(name, static_cast<int>(i));
      });

    bool enabled = true;

    if (unmanaged) {
      enabled = m_tabs[i]->canHandleUnmanaged();
    } else if (m_ModInfo->hasFlag(ModInfo::FLAG_SEPARATOR)) {
      enabled = m_tabs[i]->canHandleSeparators();
    }

    ui->tabWidget->setTabEnabled(static_cast<int>(i), enabled);

    if (!tabSelected && enabled) {
      ui->tabWidget->setCurrentIndex(static_cast<int>(i));
      tabSelected = true;
    }
  }

  for (auto& tab : m_tabs) {
    tab->setMod(m_ModInfo, m_Origin);
  }
}

ModInfoDialog::~ModInfoDialog()
{
  delete ui;
}

std::vector<std::unique_ptr<ModInfoDialogTab>> ModInfoDialog::createTabs()
{
  std::vector<std::unique_ptr<ModInfoDialogTab>> v;

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
  refreshLists();
  return TutorableDialog::exec();
}

void ModInfoDialog::saveState(Settings& s) const
{
  s.directInterface().setValue("mod_info_tabs", saveTabState());

  for (const auto& tab : m_tabs) {
    tab->saveState(s);
  }
}

void ModInfoDialog::restoreState(const Settings& s)
{
  restoreTabState(s.directInterface().value("mod_info_tabs").toByteArray());

  for (const auto& tab : m_tabs) {
    tab->restoreState(s);
  }
}

void ModInfoDialog::restoreTabState(const QByteArray &state)
{
  QDataStream stream(state);
  int count = 0;
  stream >> count;

  QStringList tabIds;

  // first, only determine the new mapping
  for (int newPos = 0; newPos < count; ++newPos) {
    QString tabId;
    stream >> tabId;
    tabIds.append(tabId);
    int oldPos = tabIndex(tabId);
    if (oldPos != -1) {
      m_RealTabPos[newPos] = oldPos;
    } else {
      m_RealTabPos[newPos] = newPos;
    }
  }

  // then actually move the tabs
  QTabBar *tabBar = ui->tabWidget->tabBar();
  ui->tabWidget->blockSignals(true);
  for (int newPos = 0; newPos < count; ++newPos) {
    QString tabId = tabIds.at(newPos);
    int oldPos = tabIndex(tabId);
    tabBar->moveTab(oldPos, newPos);
  }
  ui->tabWidget->blockSignals(false);
}

QByteArray ModInfoDialog::saveTabState() const
{
  QByteArray result;
  QDataStream stream(&result, QIODevice::WriteOnly);
  stream << ui->tabWidget->count();
  for (int i = 0; i < ui->tabWidget->count(); ++i) {
    stream << ui->tabWidget->widget(i)->objectName();
  }

  return result;
}

int ModInfoDialog::tabIndex(const QString& tabId)
{
  for (int i = 0; i < ui->tabWidget->count(); ++i) {
    if (ui->tabWidget->widget(i)->objectName() == tabId) {
      return i;
    }
  }
  return -1;
}

void ModInfoDialog::onDeleteShortcut()
{
  for (auto& t : m_tabs) {
    if (t->deleteRequested()) {
      break;
    }
  }
}

void ModInfoDialog::refreshLists()
{
  for (auto& tab : m_tabs) {
    tab->update();
  }

  if (m_RootPath.length() > 0) {
    QDirIterator dirIterator(m_RootPath, QDir::Files, QDirIterator::Subdirectories);
    while (dirIterator.hasNext()) {
      QString fileName = dirIterator.next();

      for (auto& tab : m_tabs) {
        if (tab->feedFile(m_RootPath, fileName)) {
          break;
        }
      }
    }
  }
}

void ModInfoDialog::on_closeButton_clicked()
{
  for (auto& tab : m_tabs) {
    if (!tab->canClose()) {
      return;
    }
  }

  close();
}

void ModInfoDialog::openTab(int tab)
{
  if (ui->tabWidget->isTabEnabled(tab)) {
    ui->tabWidget->setCurrentIndex(tab);
  }
}

void ModInfoDialog::on_tabWidget_currentChanged(int index)
{
}

void ModInfoDialog::on_nextButton_clicked()
{
	int currentTab = ui->tabWidget->currentIndex();
	int tab = m_RealTabPos[currentTab];

    emit modOpenNext(tab);
    this->accept();
}

void ModInfoDialog::on_prevButton_clicked()
{
	int currentTab = ui->tabWidget->currentIndex();
	int tab = m_RealTabPos[currentTab];

    emit modOpenPrev(tab);
    this->accept();
}
