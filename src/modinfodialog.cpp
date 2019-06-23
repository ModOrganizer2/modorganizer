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
#include "mainwindow.h"

#include "modidlineedit.h"
#include "iplugingame.h"
#include "nexusinterface.h"
#include "report.h"
#include "utility.h"
#include "messagedialog.h"
#include "bbcode.h"
#include "questionboxmemory.h"
#include "settings.h"
#include "categories.h"
#include "organizercore.h"
#include "pluginlistsortproxy.h"
#include "previewgenerator.h"
#include "previewdialog.h"
#include "texteditor.h"

#include "modinfodialogtextfiles.h"
#include "modinfodialogimages.h"
#include "modinfodialogesps.h"
#include "modinfodialogconflicts.h"
#include "modinfodialogcategories.h"
#include "modinfodialognexus.h"
#include "modinfodialogfiletree.h"

#include <QDir>
#include <QDirIterator>
#include <QPushButton>
#include <QInputDialog>
#include <QMessageBox>
#include <QMenu>
#include <QFileSystemModel>
#include <QInputDialog>
#include <QPointer>
#include <QFileDialog>
#include <QShortcut>

#include <Shlwapi.h>

#include <sstream>


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


ModInfoDialog::ModInfoDialog(ModInfo::Ptr modInfo, bool unmanaged, OrganizerCore *organizerCore, PluginContainer *pluginContainer, QWidget *parent)
  : TutorableDialog("ModInfoDialog", parent), ui(new Ui::ModInfoDialog), m_ModInfo(modInfo),
   m_Origin(nullptr), m_OrganizerCore(organizerCore), m_PluginContainer(pluginContainer)
{
  ui->setupUi(this);

  m_tabs = createTabs();

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
  }

  this->setWindowTitle(modInfo->name());
  this->setWindowModality(Qt::WindowModal);

  m_RootPath = modInfo->absolutePath();

  auto* sc = new QShortcut(QKeySequence::Delete, this);
  connect(sc, &QShortcut::activated, [&]{ onDeleteShortcut(); });

  auto* ds = m_OrganizerCore->directoryStructure();
  if (ds->originExists(ToWString(modInfo->name()))) {
    m_Origin = &ds->getOriginByName(ToWString(modInfo->name()));
    if (m_Origin->isDisabled()) {
      m_Origin = nullptr;
    }
  }

  if (modInfo->hasFlag(ModInfo::FLAG_SEPARATOR))
  {
    ui->tabWidget->setTabEnabled(TAB_TEXTFILES, false);
    ui->tabWidget->setTabEnabled(TAB_INIFILES, false);
    ui->tabWidget->setTabEnabled(TAB_IMAGES, false);
    ui->tabWidget->setTabEnabled(TAB_ESPS, false);
    ui->tabWidget->setTabEnabled(TAB_CONFLICTS, false);
    ui->tabWidget->setTabEnabled(TAB_CATEGORIES, true);
    ui->tabWidget->setTabEnabled(TAB_NEXUS, false);
    ui->tabWidget->setTabEnabled(TAB_NOTES, true);
    ui->tabWidget->setTabEnabled(TAB_FILETREE, false);
  }
  else if (unmanaged)
  {
    ui->tabWidget->setTabEnabled(TAB_TEXTFILES, false);
    ui->tabWidget->setTabEnabled(TAB_INIFILES, false);
    ui->tabWidget->setTabEnabled(TAB_IMAGES, false);
    ui->tabWidget->setTabEnabled(TAB_ESPS, false);
    ui->tabWidget->setTabEnabled(TAB_CONFLICTS, true);
    ui->tabWidget->setTabEnabled(TAB_CATEGORIES, false);
    ui->tabWidget->setTabEnabled(TAB_NEXUS, false);
    ui->tabWidget->setTabEnabled(TAB_NOTES, false);
    ui->tabWidget->setTabEnabled(TAB_FILETREE, false);
  } else {
    ui->tabWidget->setTabEnabled(TAB_TEXTFILES, true);
    ui->tabWidget->setTabEnabled(TAB_INIFILES, true);
    ui->tabWidget->setTabEnabled(TAB_IMAGES, true);
    ui->tabWidget->setTabEnabled(TAB_ESPS, true);
    ui->tabWidget->setTabEnabled(TAB_CONFLICTS, true);
    ui->tabWidget->setTabEnabled(TAB_CATEGORIES, true);
    ui->tabWidget->setTabEnabled(TAB_NEXUS, true);
    ui->tabWidget->setTabEnabled(TAB_NOTES, true);
    ui->tabWidget->setTabEnabled(TAB_FILETREE, true);
  }

  // activate first enabled tab
  for (int i = 0; i < ui->tabWidget->count(); ++i) {
    if (ui->tabWidget->isTabEnabled(i)) {
      ui->tabWidget->setCurrentIndex(i);
      break;
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

template <class... Ts>
std::vector<std::unique_ptr<ModInfoDialogTab>> createTabsImpl(
  OrganizerCore& oc, PluginContainer& plugin,
  ModInfoDialog* self, Ui::ModInfoDialog* ui)
{
  std::vector<std::unique_ptr<ModInfoDialogTab>> v;
  (v.push_back(std::make_unique<Ts>(oc, plugin, self, ui)), ...);

  return v;
}

std::vector<std::unique_ptr<ModInfoDialogTab>> ModInfoDialog::createTabs()
{
  return createTabsImpl<
    TextFilesTab, IniFilesTab, ImagesTab, ESPsTab,
    ConflictsTab, CategoriesTab, NexusTab, NotesTab, FileTreeTab>(
      *m_OrganizerCore, *m_PluginContainer, this, ui);
}

int ModInfoDialog::exec()
{
  refreshLists();
  return TutorableDialog::exec();
}

int ModInfoDialog::tabIndex(const QString &tabId)
{
  for (int i = 0; i < ui->tabWidget->count(); ++i) {
    if (ui->tabWidget->widget(i)->objectName() == tabId) {
      return i;
    }
  }
  return -1;
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
  QTabBar *tabBar = ui->tabWidget->findChild<QTabBar*>("qt_tabwidget_tabbar"); // magic name = bad
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
  QTabWidget *tabWidget = findChild<QTabWidget*>("tabWidget");
  if (tabWidget->isTabEnabled(tab)) {
    tabWidget->setCurrentIndex(tab);
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
