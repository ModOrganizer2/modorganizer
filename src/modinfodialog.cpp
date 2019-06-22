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
#include "descriptionpage.h"
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


ModInfoDialog::ModInfoDialog(ModInfo::Ptr modInfo, const DirectoryEntry *directory, bool unmanaged, OrganizerCore *organizerCore, PluginContainer *pluginContainer, QWidget *parent)
  : TutorableDialog("ModInfoDialog", parent), ui(new Ui::ModInfoDialog), m_ModInfo(modInfo),
  m_RequestStarted(false),
  m_NewFolderAction(nullptr), m_OpenAction(nullptr), m_PreviewAction(nullptr),
  m_RenameAction(nullptr), m_DeleteAction(nullptr), m_HideAction(nullptr),
  m_UnhideAction(nullptr), m_Directory(directory), m_Origin(nullptr),
  m_OrganizerCore(organizerCore), m_PluginContainer(pluginContainer)
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

  QString metaFileName = m_RootPath.mid(0).append("/meta.ini");
  m_Settings = new QSettings(metaFileName, QSettings::IniFormat);

  QLineEdit *modIDEdit = findChild<QLineEdit*>("modIDEdit");
  ui->modIDEdit->setValidator(new QIntValidator(modIDEdit));
  ui->modIDEdit->setText(QString("%1").arg(modInfo->getNexusID()));

  connect(ui->modIDEdit, SIGNAL(linkClicked(QString)), this, SLOT(linkClicked(QString)));

  QString gameName = modInfo->getGameName();
  ui->sourceGameEdit->addItem(organizerCore->managedGame()->gameName(), organizerCore->managedGame()->gameShortName());
  if (organizerCore->managedGame()->validShortNames().size() == 0) {
    ui->sourceGameEdit->setDisabled(true);
  } else {
    for (auto game : pluginContainer->plugins<IPluginGame>()) {
      for (QString gameName : organizerCore->managedGame()->validShortNames()) {
        if (game->gameShortName().compare(gameName, Qt::CaseInsensitive) == 0) {
          ui->sourceGameEdit->addItem(game->gameName(), game->gameShortName());
          break;
        }
      }
    }
  }
  ui->sourceGameEdit->setCurrentIndex(ui->sourceGameEdit->findData(gameName));

  ui->commentsEdit->setText(modInfo->comments());
  ui->notesEdit->setText(modInfo->notes());

  ui->descriptionView->setPage(new DescriptionPage());

  connect(m_ModInfo.data(), SIGNAL(modDetailsUpdated(bool)), this, SLOT(modDetailsUpdated(bool)));
  connect(ui->descriptionView->page(), SIGNAL(linkClicked(QUrl)), this, SLOT(linkClicked(QUrl)));
  //TODO: No easy way to delegate links
  //ui->descriptionView->page()->acceptNavigationRequest(QWebEnginePage::DelegateAllLinks);

  new QShortcut(QKeySequence::Delete, this, SLOT(delete_activated()));

  if (directory->originExists(ToWString(modInfo->name()))) {
    m_Origin = &directory->getOriginByName(ToWString(modInfo->name()));
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
    //ui->tabWidget->setTabEnabled(TAB_CATEGORIES, false);
    addCategories(CategoryFactory::instance(), modInfo->getCategories(), ui->categoriesTree->invisibleRootItem(), 0);
    refreshPrimaryCategoriesBox();
    ui->tabWidget->setTabEnabled(TAB_NEXUS, false);
    //ui->tabWidget->setTabEnabled(TAB_NOTES, false);
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
    ui->tabWidget->setTabEnabled(TAB_FILETREE, false);
    ui->tabWidget->setTabEnabled(TAB_NOTES, false);
  } else {
    ui->tabWidget->setTabEnabled(TAB_TEXTFILES, true);
    ui->tabWidget->setTabEnabled(TAB_INIFILES, true);
    ui->tabWidget->setTabEnabled(TAB_IMAGES, true);
    ui->tabWidget->setTabEnabled(TAB_ESPS, true);
    ui->tabWidget->setTabEnabled(TAB_CONFLICTS, true);

    initFiletree(modInfo);
    addCategories(CategoryFactory::instance(), modInfo->getCategories(), ui->categoriesTree->invisibleRootItem(), 0);
    refreshPrimaryCategoriesBox();
  }



  ui->endorseBtn->setVisible(Settings::instance().endorsementIntegration());
  ui->endorseBtn->setEnabled((m_ModInfo->endorsedState() == ModInfo::ENDORSED_FALSE) ||
                             (m_ModInfo->endorsedState() == ModInfo::ENDORSED_NEVER));

  // activate first enabled tab
  for (int i = 0; i < ui->tabWidget->count(); ++i) {
    if (ui->tabWidget->isTabEnabled(i)) {
      ui->tabWidget->setCurrentIndex(i);
      break;
    }
  }

  if (ui->tabWidget->currentIndex() == TAB_NEXUS) {
    activateNexusTab();
  }

  for (auto& tab : m_tabs) {
    tab->setMod(m_ModInfo, m_Origin);
  }
}

ModInfoDialog::~ModInfoDialog()
{
  m_ModInfo->setComments(ui->commentsEdit->text());
  //Avoid saving html stump if notes field is empty.
  if (ui->notesEdit->toPlainText().isEmpty())
    m_ModInfo->setNotes(ui->notesEdit->toPlainText());
  else
    m_ModInfo->setNotes(ui->notesEdit->toHtml());
  saveCategories(ui->categoriesTree->invisibleRootItem());
  delete ui->descriptionView->page();
  delete ui->descriptionView;
  delete ui;
  delete m_Settings;
}

std::vector<std::unique_ptr<ModInfoDialogTab>> ModInfoDialog::createTabs()
{
  std::vector<std::unique_ptr<ModInfoDialogTab>> v;

  v.push_back(std::make_unique<TextFilesTab>(this, ui));
  v.push_back(std::make_unique<IniFilesTab>(this, ui));
  v.push_back(std::make_unique<ImagesTab>(this, ui));
  v.push_back(std::make_unique<ESPsTab>(this, ui));
  v.push_back(std::make_unique<ConflictsTab>(
    this, ui, *m_OrganizerCore, *m_PluginContainer));

  return v;
}

int ModInfoDialog::exec()
{
  refreshLists();
  return TutorableDialog::exec();
}

void ModInfoDialog::initFiletree(ModInfo::Ptr modInfo)
{
  ui->fileTree = findChild<QTreeView*>("fileTree");

  m_FileSystemModel = new QFileSystemModel(this);
  m_FileSystemModel->setReadOnly(false);
  m_FileSystemModel->setRootPath(m_RootPath);
  ui->fileTree->setModel(m_FileSystemModel);
  ui->fileTree->setRootIndex(m_FileSystemModel->index(m_RootPath));
  ui->fileTree->setColumnWidth(0, 300);

  m_NewFolderAction = new QAction(tr("&New Folder"), ui->fileTree);
  m_OpenAction = new QAction(tr("&Open"), ui->fileTree);
  m_PreviewAction = new QAction(tr("&Preview"), ui->fileTree);
  m_RenameAction = new QAction(tr("&Rename"), ui->fileTree);
  m_DeleteAction = new QAction(tr("&Delete"), ui->fileTree);
  m_HideAction = new QAction(tr("&Hide"), ui->fileTree);
  m_UnhideAction = new QAction(tr("&Unhide"), ui->fileTree);

  connect(m_NewFolderAction, SIGNAL(triggered()), this, SLOT(createDirectoryTriggered()));
  connect(m_OpenAction, SIGNAL(triggered()), this, SLOT(openTriggered()));
  connect(m_PreviewAction, SIGNAL(triggered()), this, SLOT(previewTriggered()));
  connect(m_RenameAction, SIGNAL(triggered()), this, SLOT(renameTriggered()));
  connect(m_DeleteAction, SIGNAL(triggered()), this, SLOT(deleteTriggered()));
  connect(m_HideAction, SIGNAL(triggered()), this, SLOT(hideTriggered()));
  connect(m_UnhideAction, SIGNAL(triggered()), this, SLOT(unhideTriggered()));
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

void ModInfoDialog::refreshLists()
{
  for (auto& tab : m_tabs) {
    tab->update();
  }

  refreshFiles();
}

void ModInfoDialog::refreshFiles()
{
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

void ModInfoDialog::addCategories(const CategoryFactory &factory, const std::set<int> &enabledCategories, QTreeWidgetItem *root, int rootLevel)
{
  for (int i = 0; i < static_cast<int>(factory.numCategories()); ++i) {
    if (factory.getParentID(i) != rootLevel) {
      continue;
    }
    int categoryID = factory.getCategoryID(i);
    QTreeWidgetItem *newItem
        = new QTreeWidgetItem(QStringList(factory.getCategoryName(i)));
    newItem->setFlags(newItem->flags() | Qt::ItemIsUserCheckable);
    newItem->setCheckState(0, enabledCategories.find(categoryID)
                                      != enabledCategories.end()
                                  ? Qt::Checked
                                  : Qt::Unchecked);
    newItem->setData(0, Qt::UserRole, categoryID);
    if (factory.hasChildren(i)) {
      addCategories(factory, enabledCategories, newItem, categoryID);
    }
    root->addChild(newItem);
  }
}


void ModInfoDialog::saveCategories(QTreeWidgetItem *currentNode)
{
  for (int i = 0; i < currentNode->childCount(); ++i) {
    QTreeWidgetItem *childNode = currentNode->child(i);
    m_ModInfo->setCategory(childNode->data(0, Qt::UserRole).toInt(), childNode->checkState(0));
    saveCategories(childNode);
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



QString ModInfoDialog::getModVersion() const
{
  return m_Settings->value("version", "").toString();
}


const int ModInfoDialog::getModID() const
{
  return m_Settings->value("modid", 0).toInt();
}

void ModInfoDialog::openTab(int tab)
{
  QTabWidget *tabWidget = findChild<QTabWidget*>("tabWidget");
  if (tabWidget->isTabEnabled(tab)) {
    tabWidget->setCurrentIndex(tab);
  }
}

void ModInfoDialog::on_visitNexusLabel_linkActivated(const QString &link)
{
  emit linkActivated(link);
}

void ModInfoDialog::linkClicked(const QUrl &url)
{
  //Ideally we'd ask the mod for the game and the web service then pass the game
  //and URL to the web service
  if (NexusInterface::instance(m_PluginContainer)->isURLGameRelated(url)) {
    emit linkActivated(url.toString());
  } else {
    shell::OpenLink(url);
  }
}

void ModInfoDialog::linkClicked(QString url)
{
  emit linkActivated(url);
}


void ModInfoDialog::refreshNexusData(int modID)
{
  if ((!m_RequestStarted) && (modID > 0)) {
    m_RequestStarted = true;

    m_ModInfo->updateNXMInfo();

    MessageDialog::showMessage(tr("Info requested, please wait"), this);
  }
}


QString ModInfoDialog::getFileCategory(int categoryID)
{
  switch (categoryID) {
    case 1: return tr("Main");
    case 2: return tr("Update");
    case 3: return tr("Optional");
    case 4: return tr("Old");
    case 5: return tr("Miscellaneous");
    case 6: return tr("Deleted");
    default: return tr("Unknown");
  }
}


void ModInfoDialog::updateVersionColor()
{
//  QPalette versionColor;
  if (m_ModInfo->getVersion() != m_ModInfo->getNewestVersion()) {
    ui->versionEdit->setStyleSheet("color: red");
//    versionColor.setColor(QPalette::Text, Qt::red);
    ui->versionEdit->setToolTip(tr("Current Version: %1").arg(m_ModInfo->getNewestVersion().canonicalString()));
  } else {
    ui->versionEdit->setStyleSheet("color: green");
//    versionColor.setColor(QPalette::Text, Qt::green);
    ui->versionEdit->setToolTip(tr("No update available"));
  }
//  ui->versionEdit->setPalette(versionColor);
}


void ModInfoDialog::modDetailsUpdated(bool success)
{
  QString nexusDescription = m_ModInfo->getNexusDescription();
  QString descriptionAsHTML = "<html>"
    "<head><style class=\"nexus-description\">body {font-style: sans-serif; background: #707070; } a { color: #5EA2E5; }</style></head>"
    "<body>%1</body>"
    "</html>";

  if (!nexusDescription.isEmpty()) {
    descriptionAsHTML = descriptionAsHTML.arg(BBCode::convertToHTML(nexusDescription));
  } else {
    descriptionAsHTML = descriptionAsHTML.arg(tr("<div style=\"text-align: center;\"><h1>Uh oh!</h1><p>Sorry, there is no description available for this mod. :(</p></div>"));
  }

  ui->descriptionView->page()->setHtml(descriptionAsHTML);

  updateVersionColor();
}


void ModInfoDialog::activateNexusTab()
{
  QLineEdit *modIDEdit = findChild<QLineEdit*>("modIDEdit");
  int modID = modIDEdit->text().toInt();
  if (modID > 0) {
    QString nexusLink = NexusInterface::instance(m_PluginContainer)->getModURL(modID, m_ModInfo->getGameName());
    QLabel *visitNexusLabel = findChild<QLabel*>("visitNexusLabel");
    visitNexusLabel->setText(tr("<a href=\"%1\">Visit on Nexus</a>").arg(nexusLink));
    visitNexusLabel->setToolTip(nexusLink);
    m_ModInfo->setURL(nexusLink);

    if (m_ModInfo->getNexusDescription().isEmpty() || QDateTime::currentDateTimeUtc() >= m_ModInfo->getLastNexusQuery().addDays(1)) {
      refreshNexusData(modID);
    } else {
      modDetailsUpdated(true);
    }
  } else
    modDetailsUpdated(true);
  QLineEdit *versionEdit = findChild<QLineEdit*>("versionEdit");
  QString currentVersion = m_Settings->value("version", "???").toString();
  versionEdit->setText(currentVersion);
  ui->customUrlLineEdit->setText(m_ModInfo->getURL());
}


void ModInfoDialog::on_tabWidget_currentChanged(int index)
{
  if (index == TAB_NEXUS || m_RealTabPos[index] == TAB_NEXUS) {
    activateNexusTab();
  }
}


void ModInfoDialog::on_modIDEdit_editingFinished()
{
  int oldID = m_Settings->value("modid", 0).toInt();
  int modID = ui->modIDEdit->text().toInt();
  if (oldID != modID){
    m_ModInfo->setNexusID(modID);

    ui->descriptionView->page()->setHtml("");
    if (modID != 0) {
      m_RequestStarted = false;
      refreshNexusData(modID);
    }
  }
}

void ModInfoDialog::on_sourceGameEdit_currentIndexChanged(int)
{
  for (auto game : m_PluginContainer->plugins<IPluginGame>()) {
    if (game->gameName() == ui->sourceGameEdit->currentText()) {
      m_ModInfo->setGameName(game->gameShortName());
      return;
    }
  }
}

void ModInfoDialog::on_versionEdit_editingFinished()
{
  VersionInfo version(ui->versionEdit->text());
  m_ModInfo->setVersion(version);
  updateVersionColor();
}

void ModInfoDialog::on_customUrlLineEdit_editingFinished()
{
  m_ModInfo->setURL(ui->customUrlLineEdit->text());
}

bool ModInfoDialog::recursiveDelete(const QModelIndex &index)
{
  for (int childRow = 0; childRow < m_FileSystemModel->rowCount(index); ++childRow) {
    QModelIndex childIndex = m_FileSystemModel->index(childRow, 0, index);
    if (m_FileSystemModel->isDir(childIndex)) {
      if (!recursiveDelete(childIndex)) {
        qCritical("failed to delete %s", m_FileSystemModel->fileName(childIndex).toUtf8().constData());
        return false;
      }
    } else {
      if (!m_FileSystemModel->remove(childIndex)) {
        qCritical("failed to delete %s", m_FileSystemModel->fileName(childIndex).toUtf8().constData());
        return false;
      }
    }
  }
  if (!m_FileSystemModel->remove(index)) {
    qCritical("failed to delete %s", m_FileSystemModel->fileName(index).toUtf8().constData());
    return false;
  }
  return true;
}


void ModInfoDialog::on_openInExplorerButton_clicked()
{
  shell::ExploreFile(m_ModInfo->absolutePath());
}

void ModInfoDialog::deleteFile(const QModelIndex &index)
{
  bool res = m_FileSystemModel->isDir(index) ? recursiveDelete(index)
                                             : m_FileSystemModel->remove(index);
  if (!res) {
    QString fileName = m_FileSystemModel->fileName(index);
    reportError(tr("Failed to delete %1").arg(fileName));
  }
}

void ModInfoDialog::delete_activated()
{
	if (ui->fileTree->hasFocus()) {
		QItemSelectionModel *selection = ui->fileTree->selectionModel();

		if (selection->hasSelection() && selection->selectedRows().count() >= 1) {

			if (selection->selectedRows().count() == 0) {
				return;
			}
			else if (selection->selectedRows().count() == 1) {
				QString fileName = m_FileSystemModel->fileName(selection->selectedRows().at(0));
				if (QMessageBox::question(this, tr("Confirm"), tr("Are you sure you want to delete \"%1\"?").arg(fileName),
					QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
					return;
				}
			}
			else {
				if (QMessageBox::question(this, tr("Confirm"), tr("Are you sure you want to delete the selected files?"),
					QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
					return;
				}
			}

			foreach(QModelIndex index, selection->selectedRows()) {
				deleteFile(index);
			}
		}
	}
}

void ModInfoDialog::deleteTriggered()
{
  if (m_FileSelection.count() == 0) {
    return;
  } else if (m_FileSelection.count() == 1) {
    QString fileName = m_FileSystemModel->fileName(m_FileSelection.at(0));
    if (QMessageBox::question(this, tr("Confirm"), tr("Are you sure you want to delete \"%1\"?").arg(fileName),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
      return;
    }
  } else {
    if (QMessageBox::question(this, tr("Confirm"), tr("Are you sure you want to delete the selected files?"),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
      return;
    }
  }

  foreach(QModelIndex index, m_FileSelection) {
    deleteFile(index);
  }
}


void ModInfoDialog::renameTriggered()
{
  QModelIndex selection = m_FileSelection.at(0);
  QModelIndex index = selection.sibling(selection.row(), 0);
  if (!index.isValid() || m_FileSystemModel->isReadOnly()) {
      return;
  }

  ui->fileTree->edit(index);
}


void ModInfoDialog::hideTriggered()
{
  changeFiletreeVisibility(false);
}


void ModInfoDialog::unhideTriggered()
{
  changeFiletreeVisibility(true);
}

void ModInfoDialog::changeFiletreeVisibility(bool visible)
{
  bool changed = false;
  bool stop = false;

  qDebug().nospace()
    << (visible ? "unhiding" : "hiding") << " "
    << m_FileSelection.size() << " filetree files";

  QFlags<FileRenamer::RenameFlags> flags =
    (visible ? FileRenamer::UNHIDE : FileRenamer::HIDE);

  if (m_FileSelection.size() > 1) {
    flags |= FileRenamer::MULTIPLE;
  }

  FileRenamer renamer(this, flags);

  for (const auto& index : m_FileSelection) {
    if (stop) {
      break;
    }

    const QString path = m_FileSystemModel->filePath(index);
    auto result = FileRenamer::RESULT_CANCEL;

    if (visible) {
      if (!canUnhideFile(false, path)) {
        qDebug().nospace() << "cannot unhide " << path << ", skipping";
        continue;
      }
      result = unhideFile(renamer, path);
    } else {
      if (!canHideFile(false, path)) {
        qDebug().nospace() << "cannot hide " << path << ", skipping";
        continue;
      }
      result = hideFile(renamer, path);
    }

    switch (result) {
      case FileRenamer::RESULT_OK: {
        // will trigger a refresh at the end
        changed = true;
        break;
      }

      case FileRenamer::RESULT_SKIP: {
        // nop
        break;
      }

      case FileRenamer::RESULT_CANCEL: {
        // stop right now, but make sure to refresh if needed
        stop = true;
        break;
      }
    }
  }

  qDebug().nospace() << (visible ? "unhiding" : "hiding") << " filetree files done";

  if (changed) {
    qDebug().nospace() << "triggering refresh";
    if (m_Origin) {
      emit originModified(m_Origin->getID());
    }
    refreshLists();
  }
}


void ModInfoDialog::openTriggered()
{
  if (m_FileSelection.size() == 1) {
    const auto index = m_FileSelection.at(0);
    if (!index.isValid()) {
      return;
    }

    QString fileName = m_FileSystemModel->filePath(index);
    shell::OpenFile(fileName);
  }
}

void ModInfoDialog::previewTriggered()
{
  if (m_FileSelection.size() == 1) {
    const auto index = m_FileSelection.at(0);
    if (!index.isValid()) {
      return;
    }

    QString fileName = m_FileSystemModel->filePath(index);
    m_OrganizerCore->previewFile(this, m_ModInfo->name(), fileName);
  }
}

void ModInfoDialog::createDirectoryTriggered()
{
  QModelIndex selection = m_FileSelection.at(0);

  QModelIndex index = m_FileSystemModel->isDir(selection) ? selection
                                                          : selection.parent();
  index = index.sibling(index.row(), 0);

  QString name = tr("New Folder");
  QString path = m_FileSystemModel->filePath(index).append("/");

  QModelIndex existingIndex = m_FileSystemModel->index(path + name);
  int suffix = 1;
  while (existingIndex.isValid()) {
    name = tr("New Folder") + QString::number(suffix++);
    existingIndex = m_FileSystemModel->index(path + name);
  }

  QModelIndex newIndex = m_FileSystemModel->mkdir(index, name);
  if (!newIndex.isValid()) {
    reportError(tr("Failed to create \"%1\"").arg(name));
    return;
  }

  ui->fileTree->setCurrentIndex(newIndex);
  ui->fileTree->edit(newIndex);
}


void ModInfoDialog::on_fileTree_customContextMenuRequested(const QPoint &pos)
{
  QItemSelectionModel *selectionModel = ui->fileTree->selectionModel();
  m_FileSelection = selectionModel->selectedRows(0);

  QMenu menu(ui->fileTree);

  menu.addAction(m_NewFolderAction);

  if (selectionModel->hasSelection()) {
    bool enableOpen = true;
    bool enablePreview = true;
    bool enableRename = true;
    bool enableDelete = true;
    bool enableHide = true;
    bool enableUnhide = true;

    if (m_FileSelection.size() == 1) {
      // single selection

      // only enable open action if a file is selected
      bool hasFiles = false;

      foreach(QModelIndex idx, m_FileSelection) {
        if (m_FileSystemModel->fileInfo(idx).isFile()) {
          hasFiles = true;
          break;
        }
      }

      if (!hasFiles) {
        enableOpen = false;
        enablePreview = false;
      }

      const QString fileName = m_FileSystemModel->fileName(m_FileSelection.at(0));

      if (!canPreviewFile(*m_PluginContainer, false, fileName)) {
        enablePreview = false;
      }

      if (!canHideFile(false, fileName)) {
        enableHide = false;
      }

      if (!canUnhideFile(false, fileName)) {
        enableUnhide = false;
      }
    } else {
      // this is a multiple selection, don't show open action so users don't open
      // a thousand files
      enableOpen = false;
      enablePreview = false;
      enableRename = false;

      if (m_FileSelection.size() < max_scan_for_context_menu) {
        // if the number of selected items is low, checking them to accurately
        // show the menu items is worth it
        enableHide = false;
        enableUnhide = false;

        for (const auto& index : m_FileSelection) {
          const QString fileName = m_FileSystemModel->fileName(index);

          if (canHideFile(false, fileName)) {
            enableHide = true;
          }

          if (canUnhideFile(false, fileName)) {
            enableUnhide = true;
          }

          if (enableHide && enableUnhide) {
            // found both, no need to check more
            break;
          }
        }
      }
    }

    if (enableOpen) {
      menu.addAction(m_OpenAction);
    }

    if (enablePreview) {
      menu.addAction(m_PreviewAction);
    }

    if (enableRename) {
      menu.addAction(m_RenameAction);
    }

    if (enableDelete) {
      menu.addAction(m_DeleteAction);
    }

    if (enableHide) {
      menu.addAction(m_HideAction);
    }

    if (enableUnhide) {
      menu.addAction(m_UnhideAction);
    }
  } else {
    m_FileSelection.clear();
    m_FileSelection.append(m_FileSystemModel->index(m_FileSystemModel->rootPath(), 0));
  }

  menu.exec(ui->fileTree->viewport()->mapToGlobal(pos));
}


void ModInfoDialog::on_categoriesTree_itemChanged(QTreeWidgetItem *item, int)
{
  QTreeWidgetItem *parent = item->parent();
  while ((parent != nullptr) && ((parent->flags() & Qt::ItemIsUserCheckable) != 0) && (parent->checkState(0) == Qt::Unchecked)) {
    parent->setCheckState(0, Qt::Checked);
    parent = parent->parent();
  }
  refreshPrimaryCategoriesBox();
}


void ModInfoDialog::addCheckedCategories(QTreeWidgetItem *tree)
{
  for (int i = 0; i < tree->childCount(); ++i) {
    QTreeWidgetItem *child = tree->child(i);
    if (child->checkState(0) == Qt::Checked) {
      ui->primaryCategoryBox->addItem(child->text(0), child->data(0, Qt::UserRole));
      addCheckedCategories(child);
    }
  }
}


void ModInfoDialog::refreshPrimaryCategoriesBox()
{
  ui->primaryCategoryBox->clear();
  int primaryCategory = m_ModInfo->getPrimaryCategory();
  addCheckedCategories(ui->categoriesTree->invisibleRootItem());
  for (int i = 0; i < ui->primaryCategoryBox->count(); ++i) {
    if (ui->primaryCategoryBox->itemData(i).toInt() == primaryCategory) {
      ui->primaryCategoryBox->setCurrentIndex(i);
      break;
    }
  }
}

void ModInfoDialog::on_primaryCategoryBox_currentIndexChanged(int index)
{
  if (index != -1) {
    m_ModInfo->setPrimaryCategory(ui->primaryCategoryBox->itemData(index).toInt());
  }
}

void ModInfoDialog::on_refreshButton_clicked()
{
  if (m_ModInfo->getNexusID() > 0) {
    QLineEdit *modIDEdit = findChild<QLineEdit*>("modIDEdit");
    int modID = modIDEdit->text().toInt();
    refreshNexusData(modID);
  } else
    qInfo("Mod has no valid Nexus ID, info can't be updated.");
}

void ModInfoDialog::on_endorseBtn_clicked()
{
  emit endorseMod(m_ModInfo);
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
