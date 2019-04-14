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


ModInfoDialog::ModInfoDialog(ModInfo::Ptr modInfo, const DirectoryEntry *directory, bool unmanaged, OrganizerCore *organizerCore, PluginContainer *pluginContainer, QWidget *parent)
  : TutorableDialog("ModInfoDialog", parent), ui(new Ui::ModInfoDialog), m_ModInfo(modInfo),
  m_ThumbnailMapper(this), m_RequestStarted(false),
  m_DeleteAction(nullptr), m_RenameAction(nullptr), m_OpenAction(nullptr),
  m_Directory(directory), m_Origin(nullptr),
  m_OrganizerCore(organizerCore), m_PluginContainer(pluginContainer)
{
  ui->setupUi(this);
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

  connect(&m_ThumbnailMapper, SIGNAL(mapped(const QString&)), this, SIGNAL(thumbnailClickedSignal(const QString&)));
  connect(this, SIGNAL(thumbnailClickedSignal(const QString&)), this, SLOT(thumbnailClicked(const QString&)));
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

  refreshLists();

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
    ui->tabWidget->setTabEnabled(TAB_INIFILES, false);
    ui->tabWidget->setTabEnabled(TAB_CATEGORIES, false);
    ui->tabWidget->setTabEnabled(TAB_NEXUS, false);
    ui->tabWidget->setTabEnabled(TAB_FILETREE, false);
    ui->tabWidget->setTabEnabled(TAB_NOTES, false);
    ui->tabWidget->setTabEnabled(TAB_ESPS, false);
    ui->tabWidget->setTabEnabled(TAB_TEXTFILES, false);
    ui->tabWidget->setTabEnabled(TAB_IMAGES, false);
  } else {
    initFiletree(modInfo);
    addCategories(CategoryFactory::instance(), modInfo->getCategories(), ui->categoriesTree->invisibleRootItem(), 0);
    refreshPrimaryCategoriesBox();
    ui->tabWidget->setTabEnabled(TAB_TEXTFILES, ui->textFileList->count() != 0);
    ui->tabWidget->setTabEnabled(TAB_IMAGES, ui->thumbnailArea->count() != 0);
    ui->tabWidget->setTabEnabled(TAB_ESPS, (ui->inactiveESPList->count() != 0) || (ui->activeESPList->count() != 0));
  }
  initINITweaks();

  ui->tabWidget->setTabEnabled(TAB_CONFLICTS, m_Origin != nullptr);


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
  saveIniTweaks(); // ini tweaks are written to the ini file directly. This is the only information not managed by ModInfo
  delete ui->descriptionView->page();
  delete ui->descriptionView;
  delete ui;
  delete m_Settings;
}


void ModInfoDialog::initINITweaks()
{
  int numTweaks = m_Settings->beginReadArray("INI Tweaks");
  for (int i = 0; i < numTweaks; ++i) {
    m_Settings->setArrayIndex(i);
    QList<QListWidgetItem*> items = ui->iniTweaksList->findItems(m_Settings->value("name").toString(), Qt::MatchFixedString);
    if (items.size() != 0) {
      items.at(0)->setCheckState(Qt::Checked);
    }
  }
  m_Settings->endArray();
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

  m_DeleteAction = new QAction(tr("&Delete"), ui->fileTree);
  m_RenameAction = new QAction(tr("&Rename"), ui->fileTree);
  m_HideAction = new QAction(tr("&Hide"), ui->fileTree);
  m_UnhideAction = new QAction(tr("&Unhide"), ui->fileTree);
  m_OpenAction = new QAction(tr("&Open"), ui->fileTree);
  m_NewFolderAction = new QAction(tr("&New Folder"), ui->fileTree);
  QObject::connect(m_DeleteAction, SIGNAL(triggered()), this, SLOT(deleteTriggered()));
  QObject::connect(m_RenameAction, SIGNAL(triggered()), this, SLOT(renameTriggered()));
  QObject::connect(m_OpenAction, SIGNAL(triggered()), this, SLOT(openTriggered()));
  QObject::connect(m_NewFolderAction, SIGNAL(triggered()), this, SLOT(createDirectoryTriggered()));
  QObject::connect(m_HideAction, SIGNAL(triggered()), this, SLOT(hideTriggered()));
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
  int numNonConflicting = 0;
  int numOverwrite = 0;
  int numOverwritten = 0;

  ui->overwriteTree->clear();
  ui->overwrittenTree->clear();

  if (m_Origin != nullptr) {
    std::vector<FileEntry::Ptr> files = m_Origin->getFiles();
    for (auto iter = files.begin(); iter != files.end(); ++iter) {
      QString relativeName = QDir::fromNativeSeparators(ToQString((*iter)->getRelativePath()));
      QString fileName = relativeName.mid(0).prepend(m_RootPath);
      bool archive;
      if ((*iter)->getOrigin(archive) == m_Origin->getID()) {
        std::vector<std::pair<int, std::pair<std::wstring, int>>> alternatives = (*iter)->getAlternatives();
        if (!alternatives.empty()) {
          std::wostringstream altString;
          for (std::vector<std::pair<int, std::pair<std::wstring, int>>>::iterator altIter = alternatives.begin();
               altIter != alternatives.end(); ++altIter) {
            if (altIter != alternatives.begin()) {
              altString << ", ";
            }
            altString << m_Directory->getOriginByID(altIter->first).getName();
          }
          QStringList fields(relativeName.prepend("..."));
          fields.append(ToQString(altString.str()));

          QTreeWidgetItem *item = new QTreeWidgetItem(fields);
          item->setData(0, Qt::UserRole, fileName);
          item->setData(1, Qt::UserRole, ToQString(m_Directory->getOriginByID(alternatives.back().first).getName()));
          item->setData(1, Qt::UserRole + 1, alternatives.back().first);
          item->setData(1, Qt::UserRole + 2, archive);
          if (archive) {
            QFont font = item->font(0);
            font.setItalic(true);
            item->setFont(0, font);
            item->setFont(1, font);
          }
          ui->overwriteTree->addTopLevelItem(item);
          ++numOverwrite;
        } else {// otherwise don't display the file
          ++numNonConflicting;
        }
      } else {
        FilesOrigin &realOrigin = m_Directory->getOriginByID((*iter)->getOrigin(archive));
        QStringList fields(relativeName);
        fields.append(ToQString(realOrigin.getName()));
        QTreeWidgetItem *item = new QTreeWidgetItem(fields);
        item->setData(0, Qt::UserRole, fileName);
        item->setData(1, Qt::UserRole, ToQString(realOrigin.getName()));
        item->setData(1, Qt::UserRole + 2, archive);
        if (archive) {
          QFont font = item->font(0);
          font.setItalic(true);
          item->setFont(0, font);
          item->setFont(1, font);
        }
        ui->overwrittenTree->addTopLevelItem(item);
        ++numOverwritten;
      }
    }
  }

  if (m_RootPath.length() > 0) {
    QDirIterator dirIterator(m_RootPath, QDir::Files, QDirIterator::Subdirectories);
    while (dirIterator.hasNext()) {
      QString fileName = dirIterator.next();

      if (fileName.endsWith(".txt", Qt::CaseInsensitive)) {
        ui->textFileList->addItem(fileName.mid(m_RootPath.length() + 1));
      } else if ((fileName.endsWith(".ini", Qt::CaseInsensitive) || fileName.endsWith(".cfg", Qt::CaseInsensitive)) &&
                 !fileName.endsWith("meta.ini")) {
        QString namePart = fileName.mid(m_RootPath.length() + 1);
        if (namePart.startsWith("INI Tweaks", Qt::CaseInsensitive)) {
          QListWidgetItem *newItem = new QListWidgetItem(namePart.mid(11), ui->iniTweaksList);
          newItem->setData(Qt::UserRole, namePart);
          newItem->setFlags(newItem->flags() | Qt::ItemIsUserCheckable);
          newItem->setCheckState(Qt::Unchecked);
          ui->iniTweaksList->addItem(newItem);
        } else {
          ui->iniFileList->addItem(namePart);
        }
      } else if (fileName.endsWith(".esp", Qt::CaseInsensitive) ||
                 fileName.endsWith(".esm", Qt::CaseInsensitive) ||
                 fileName.endsWith(".esl", Qt::CaseInsensitive)) {
        QString relativePath = fileName.mid(m_RootPath.length() + 1);
        if (relativePath.contains('/')) {
          QFileInfo fileInfo(fileName);
          QListWidgetItem *newItem = new QListWidgetItem(fileInfo.fileName());
          newItem->setData(Qt::UserRole, relativePath);
          ui->inactiveESPList->addItem(newItem);
        } else {
          ui->activeESPList->addItem(relativePath);
        }
      } else if ((fileName.endsWith(".png", Qt::CaseInsensitive)) ||
                 (fileName.endsWith(".jpg", Qt::CaseInsensitive))) {
        QImage image = QImage(fileName);
        if (!image.isNull()) {
          if (static_cast<float>(image.width()) / static_cast<float>(image.height()) > 1.34) {
            image = image.scaledToWidth(128);
          } else {
            image = image.scaledToHeight(96);
          }

          QPushButton *thumbnailButton = new QPushButton(QPixmap::fromImage(image), "");
          thumbnailButton->setIconSize(QSize(image.width(), image.height()));
          connect(thumbnailButton, SIGNAL(clicked()), &m_ThumbnailMapper, SLOT(map()));
          m_ThumbnailMapper.setMapping(thumbnailButton, fileName);
          ui->thumbnailArea->addWidget(thumbnailButton);
        }
      }
    }
  }

  ui->overwriteCount->display(numOverwrite);
  ui->overwrittenCount->display(numOverwritten);
  ui->noConflictCount->display(numNonConflicting);
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
  if (allowNavigateFromTXT() && allowNavigateFromINI()) {
    this->close();
  }
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

void ModInfoDialog::thumbnailClicked(const QString &fileName)
{
  QLabel *imageLabel = findChild<QLabel*>("imageLabel");
  imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
  QImage image(fileName);
  if (static_cast<float>(image.width()) / static_cast<float>(image.height()) > 1.34) {
    image = image.scaledToWidth(imageLabel->geometry().width());
  } else {
    image = image.scaledToHeight(imageLabel->geometry().height());
  }
  imageLabel->setPixmap(QPixmap::fromImage(image));
}

bool ModInfoDialog::allowNavigateFromTXT()
{
  if (ui->saveTXTButton->isEnabled()) {
    int res = QMessageBox::question(this, tr("Save changes?"), tr("Save changes to \"%1\"?").arg(ui->textFileView->property("currentFile").toString()),
                                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (res == QMessageBox::Cancel) {
      return false;
    } else if (res == QMessageBox::Yes) {
      saveCurrentTextFile();
    }
  }
  return true;
}


bool ModInfoDialog::allowNavigateFromINI()
{
  if (ui->saveButton->isEnabled()) {
    int res = QMessageBox::question(this, tr("Save changes?"), tr("Save changes to \"%1\"?").arg(ui->iniFileView->property("currentFile").toString()),
                                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (res == QMessageBox::Cancel) {
      return false;
    } else if (res == QMessageBox::Yes) {
      saveCurrentIniFile();
    }
  }
  return true;
}


void ModInfoDialog::on_textFileList_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
  QString fullPath = m_RootPath + "/" + current->text();

  QVariant currentFile = ui->textFileView->property("currentFile");
  if (currentFile.isValid() && (currentFile.toString() == fullPath)) {
    // the new file is the same as the currently displayed file. May be the result of a cancelation
    return;
  }

  if (allowNavigateFromTXT()) {
    openTextFile(fullPath);
  } else {
    ui->textFileList->setCurrentItem(previous, QItemSelectionModel::Current);
  }
}


void ModInfoDialog::openTextFile(const QString &fileName)
{
  QString encoding;
  ui->textFileView->setText(MOBase::readFileText(fileName, &encoding));
  ui->textFileView->setProperty("currentFile", fileName);
  ui->textFileView->setProperty("encoding", encoding);
  ui->saveTXTButton->setEnabled(false);
}


void ModInfoDialog::openIniFile(const QString &fileName)
{
  QFile iniFile(fileName);
  iniFile.open(QIODevice::ReadOnly);
  QByteArray buffer = iniFile.readAll();

  QTextCodec *codec = QTextCodec::codecForUtfText(buffer, QTextCodec::codecForName("utf-8"));
  QTextEdit *iniFileView = findChild<QTextEdit*>("iniFileView");
  iniFileView->setText(codec->toUnicode(buffer));
  iniFileView->setProperty("currentFile", fileName);
  iniFileView->setProperty("encoding", codec->name());
  iniFile.close();

  ui->saveButton->setEnabled(false);
}


void ModInfoDialog::saveIniTweaks()
{
  m_Settings->remove("INI Tweaks");
  m_Settings->beginWriteArray("INI Tweaks");

  int countEnabled = 0;
  for (int i = 0; i < ui->iniTweaksList->count(); ++i) {
    if (ui->iniTweaksList->item(i)->checkState() == Qt::Checked) {
      m_Settings->setArrayIndex(countEnabled++);
      m_Settings->setValue("name", ui->iniTweaksList->item(i)->text());
    }
  }
  m_Settings->endArray();
}


void ModInfoDialog::on_iniFileList_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
  QString fullPath = m_RootPath + "/" + current->text();

  QVariant currentFile = ui->iniFileView->property("currentFile");
  if (currentFile.isValid() && (currentFile.toString() == fullPath)) {
    // the new file is the same as the currently displayed file. May be the result of a cancelation
    return;
  }

  if (allowNavigateFromINI()) {
    openIniFile(fullPath);
  } else {
    ui->iniFileList->setCurrentItem(previous, QItemSelectionModel::Current);
  }
}


void ModInfoDialog::on_iniTweaksList_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous)
{
  QString fullPath = m_RootPath + "/" + current->data(Qt::UserRole).toString();

  QVariant currentFile = ui->iniFileView->property("currentFile");
  if (currentFile.isValid() && (currentFile.toString() == fullPath)) {
    // the new file is the same as the currently displayed file. May be the result of a cancelation
    return;
  }

  if (allowNavigateFromINI()) {
    openIniFile(fullPath);
  } else {
    ui->iniFileList->setCurrentItem(previous, QItemSelectionModel::Current);
  }

}


void ModInfoDialog::on_saveButton_clicked()
{
  saveCurrentIniFile();
}


void ModInfoDialog::on_saveTXTButton_clicked()
{
  saveCurrentTextFile();
}


void ModInfoDialog::saveCurrentTextFile()
{
  QVariant fileNameVar = ui->textFileView->property("currentFile");
  QVariant encodingVar = ui->textFileView->property("encoding");
  if (fileNameVar.isValid() && encodingVar.isValid()) {
    QString fileName = fileNameVar.toString();
    QFile txtFile(fileName);
    txtFile.open(QIODevice::WriteOnly);
    txtFile.resize(0);
    QTextCodec *codec = QTextCodec::codecForName(encodingVar.toString().toUtf8());
    QString data = ui->textFileView->toPlainText().replace("\n", "\r\n");
    txtFile.write(codec->fromUnicode(data));
  } else {
    reportError("no file selected");
  }
  ui->saveTXTButton->setEnabled(false);
}


void ModInfoDialog::saveCurrentIniFile()
{
  QVariant fileNameVar = ui->iniFileView->property("currentFile");
  QVariant encodingVar = ui->iniFileView->property("encoding");
  if (fileNameVar.isValid() && !fileNameVar.toString().isEmpty()) {
    QString fileName = fileNameVar.toString();
    QDir().mkpath(QFileInfo(fileName).absolutePath());
    QFile txtFile(fileName);
    txtFile.open(QIODevice::WriteOnly);
    txtFile.resize(0);
    QTextCodec *codec = QTextCodec::codecForName(encodingVar.toString().toUtf8());
    QString data = ui->iniFileView->toPlainText().replace("\n", "\r\n");
    txtFile.write(codec->fromUnicode(data));
  } else {
    reportError("no file selected");
  }
  ui->saveButton->setEnabled(false);
}


void ModInfoDialog::on_iniFileView_textChanged()
{
  QPushButton* saveButton = findChild<QPushButton*>("saveButton");
  saveButton->setEnabled(true);
}


void ModInfoDialog::on_textFileView_textChanged()
{
  ui->saveTXTButton->setEnabled(true);
}


void ModInfoDialog::on_activateESP_clicked()
{
  QListWidget *activeESPList = findChild<QListWidget*>("activeESPList");
  QListWidget *inactiveESPList = findChild<QListWidget*>("inactiveESPList");

  int selectedRow = inactiveESPList->currentRow();
  if (selectedRow < 0) {
    return;
  }

  QListWidgetItem *selectedItem = inactiveESPList->takeItem(selectedRow);

  QDir root(m_RootPath);
  bool renamed = false;

  while (root.exists(selectedItem->text())) {
    bool okClicked = false;
    QString newName = QInputDialog::getText(this, tr("File Exists"), tr("A file with that name exists, please enter a new one"), QLineEdit::Normal, selectedItem->text(), &okClicked);
    if (!okClicked) {
      inactiveESPList->insertItem(selectedRow, selectedItem);
      return;
    } else if (newName.size() > 0) {
      selectedItem->setText(newName);
      renamed = true;
    }
  }

  if (root.rename(selectedItem->data(Qt::UserRole).toString(), selectedItem->text())) {
    activeESPList->addItem(selectedItem);
    if (renamed) {
      selectedItem->setData(Qt::UserRole, QVariant());
    }
  } else {
    inactiveESPList->insertItem(selectedRow, selectedItem);
    reportError(tr("failed to move file"));
  }
}


void ModInfoDialog::on_deactivateESP_clicked()
{
  QListWidget *activeESPList = findChild<QListWidget*>("activeESPList");
  QListWidget *inactiveESPList = findChild<QListWidget*>("inactiveESPList");

  int selectedRow = activeESPList->currentRow();
  if (selectedRow < 0) {
    return;
  }

  QDir root(m_RootPath);

  QListWidgetItem *selectedItem = activeESPList->takeItem(selectedRow);

  // if we moved the file from optional to active in this session, we move the file back to
  // where it came from. Otherwise, it is moved to the new folder "optional"
  if (selectedItem->data(Qt::UserRole).isNull()) {
    selectedItem->setData(Qt::UserRole, QString("optional/") + selectedItem->text());
    if (!root.exists("optional")) {
      if (!root.mkdir("optional")) {
        reportError(tr("failed to create directory \"optional\""));
        activeESPList->insertItem(selectedRow, selectedItem);
        return;
      }
    }
  }

  if (root.rename(selectedItem->text(), selectedItem->data(Qt::UserRole).toString())) {
    inactiveESPList->addItem(selectedItem);
  } else {
    activeESPList->insertItem(selectedRow, selectedItem);
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
    ::ShellExecuteW(nullptr, L"open", ToWString(url.toString()).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
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
	::ShellExecuteW(nullptr, L"explore", ToWString(m_ModInfo->absolutePath()).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
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
				if (QMessageBox::question(this, tr("Confirm"), tr("Are sure you want to delete \"%1\"?").arg(fileName),
					QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
					return;
				}
			}
			else {
				if (QMessageBox::question(this, tr("Confirm"), tr("Are sure you want to delete the selected files?"),
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
    if (QMessageBox::question(this, tr("Confirm"), tr("Are sure you want to delete \"%1\"?").arg(fileName),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
      return;
    }
  } else {
    if (QMessageBox::question(this, tr("Confirm"), tr("Are sure you want to delete the selected files?"),
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
  for (QModelIndexList::const_iterator iter = m_FileSelection.constBegin();
       iter != m_FileSelection.constEnd(); ++iter) {
    QString path = m_FileSystemModel->filePath(*iter);
    if (!path.endsWith(ModInfo::s_HiddenExt)) {
      hideFile(path);
    }
  }
}


void ModInfoDialog::unhideTriggered()
{
  for (QModelIndexList::const_iterator iter = m_FileSelection.constBegin();
       iter != m_FileSelection.constEnd(); ++iter) {
    QString path = m_FileSystemModel->filePath(*iter);
    if (path.endsWith(ModInfo::s_HiddenExt)) {
      unhideFile(path);
    }
  }
}


void ModInfoDialog::openFile(const QModelIndex &index)
{
  QString fileName = m_FileSystemModel->filePath(index);

  HINSTANCE res = ::ShellExecuteW(nullptr, L"open", ToWString(fileName).c_str(), nullptr, nullptr, SW_SHOW);
  if ((unsigned long long)res <= 32) {
    qCritical("failed to invoke %s: %d", qUtf8Printable(fileName), res);
  }
}


void ModInfoDialog::openTriggered()
{
  foreach(QModelIndex idx, m_FileSelection) {
    openFile(idx);
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

//  m_FileSelection = ui->fileTree->indexAt(pos);
  QMenu menu(ui->fileTree);

  menu.addAction(m_NewFolderAction);

  bool hasFiles = false;

  foreach(QModelIndex idx, m_FileSelection) {
    if (m_FileSystemModel->fileInfo(idx).isFile()) {
      hasFiles = true;
      break;
    }
  }

  if (selectionModel->hasSelection()) {
    if (hasFiles) {
      menu.addAction(m_OpenAction);
    }
    menu.addAction(m_RenameAction);
    menu.addAction(m_DeleteAction);
    if (m_FileSystemModel->fileName(m_FileSelection.at(0)).endsWith(ModInfo::s_HiddenExt)) {
      menu.addAction(m_UnhideAction);
    } else {
      menu.addAction(m_HideAction);
    }
  } else {
    m_FileSelection.clear();
    m_FileSelection.append(m_FileSystemModel->index(m_FileSystemModel->rootPath(), 0));
  }
  menu.exec(ui->fileTree->mapToGlobal(pos));
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


void ModInfoDialog::on_overwriteTree_itemDoubleClicked(QTreeWidgetItem *item, int)
{
  this->close();
  emit modOpen(item->data(1, Qt::UserRole).toString(), TAB_CONFLICTS);
}


bool ModInfoDialog::hideFile(const QString &oldName)
{
  QString newName = oldName + ModInfo::s_HiddenExt;

  if (QFileInfo(newName).exists()) {
    if (QMessageBox::question(this, tr("Replace file?"), tr("There already is a hidden version of this file. Replace it?"),
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      if (!QFile(newName).remove()) {
        QMessageBox::critical(this, tr("File operation failed"), tr("Failed to remove \"%1\". Maybe you lack the required file permissions?").arg(newName));
        return false;
      }
    } else {
      return false;
    }
  }

  if (QFile::rename(oldName, newName)) {
    return true;
  } else {
    reportError(tr("failed to rename %1 to %2").arg(oldName).arg(QDir::toNativeSeparators(newName)));
    return false;
  }
}


bool ModInfoDialog::unhideFile(const QString &oldName)
{
  QString newName = oldName.left(oldName.length() - ModInfo::s_HiddenExt.length());
  if (QFileInfo(newName).exists()) {
    if (QMessageBox::question(this, tr("Replace file?"), tr("There already is a visible version of this file. Replace it?"),
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      if (!QFile(newName).remove()) {
        QMessageBox::critical(this, tr("File operation failed"), tr("Failed to remove \"%1\". Maybe you lack the required file permissions?").arg(newName));
        return false;
      }
    } else {
      return false;
    }
  }
  if (QFile::rename(oldName, newName)) {
    return true;
  } else {
    reportError(tr("failed to rename %1 to %2").arg(QDir::toNativeSeparators(oldName)).arg(QDir::toNativeSeparators(newName)));
    return false;
  }
}


void ModInfoDialog::hideConflictFile()
{
  if (hideFile(m_ConflictsContextItem->data(0, Qt::UserRole).toString())) {
    emit originModified(m_Origin->getID());
    refreshLists();
  }
}


void ModInfoDialog::unhideConflictFile()
{
  if (unhideFile(m_ConflictsContextItem->data(0, Qt::UserRole).toString())) {
    emit originModified(m_Origin->getID());
    refreshLists();
  }
}

int ModInfoDialog::getBinaryExecuteInfo(const QFileInfo &targetInfo, QFileInfo &binaryInfo, QString &arguments)
{
	QString extension = targetInfo.suffix();
	if ((extension.compare("cmd", Qt::CaseInsensitive) == 0) ||
		(extension.compare("com", Qt::CaseInsensitive) == 0) ||
		(extension.compare("bat", Qt::CaseInsensitive) == 0)) {
		binaryInfo = QFileInfo("C:\\Windows\\System32\\cmd.exe");
		arguments = QString("/C \"%1\"").arg(QDir::toNativeSeparators(targetInfo.absoluteFilePath()));
		return 1;
	}
	else if (extension.compare("exe", Qt::CaseInsensitive) == 0) {
		binaryInfo = targetInfo;
		return 1;
	}
	else if (extension.compare("jar", Qt::CaseInsensitive) == 0) {
		// types that need to be injected into
		std::wstring targetPathW = ToWString(targetInfo.absoluteFilePath());
		QString binaryPath;

		{ // try to find java automatically
			WCHAR buffer[MAX_PATH];
			if (::FindExecutableW(targetPathW.c_str(), nullptr, buffer) > (HINSTANCE)32) {
				DWORD binaryType = 0UL;
				if (!::GetBinaryTypeW(buffer, &binaryType)) {
					qDebug("failed to determine binary type of \"%ls\": %lu", buffer, ::GetLastError());
				}
				else if (binaryType == SCS_32BIT_BINARY) {
					binaryPath = ToQString(buffer);
				}
			}
		}
		if (binaryPath.isEmpty() && (extension == "jar")) {
			// second attempt: look to the registry
			QSettings javaReg("HKEY_LOCAL_MACHINE\\Software\\JavaSoft\\Java Runtime Environment", QSettings::NativeFormat);
			if (javaReg.contains("CurrentVersion")) {
				QString currentVersion = javaReg.value("CurrentVersion").toString();
				binaryPath = javaReg.value(QString("%1/JavaHome").arg(currentVersion)).toString().append("\\bin\\javaw.exe");
			}
		}
		if (binaryPath.isEmpty()) {
			binaryPath = QFileDialog::getOpenFileName(this, tr("Select binary"), QString(), tr("Binary") + " (*.exe)");
		}
		if (binaryPath.isEmpty()) {
			return 0;
		}
		binaryInfo = QFileInfo(binaryPath);
		if (extension == "jar") {
			arguments = QString("-jar \"%1\"").arg(QDir::toNativeSeparators(targetInfo.absoluteFilePath()));
		}
		else {
			arguments = QString("\"%1\"").arg(QDir::toNativeSeparators(targetInfo.absoluteFilePath()));
		}
		return 1;
	}
	else {
		return 2;
	}
}

void ModInfoDialog::openDataFile()
{
	if (m_ConflictsContextItem != nullptr) {
		QFileInfo targetInfo(m_ConflictsContextItem->data(0, Qt::UserRole).toString());
		QFileInfo binaryInfo;
		QString arguments;
		switch (getBinaryExecuteInfo(targetInfo, binaryInfo, arguments)) {
		case 1: {
			m_OrganizerCore->spawnBinaryDirect(
				binaryInfo, arguments, m_OrganizerCore->currentProfile()->name(),
				targetInfo.absolutePath(), "", "");
		} break;
		case 2: {
			::ShellExecuteW(nullptr, L"open",
				ToWString(targetInfo.absoluteFilePath()).c_str(),
				nullptr, nullptr, SW_SHOWNORMAL);
		} break;
		default: {
			// nop
		} break;
		}
	}
}

void ModInfoDialog::previewDataFile()
{
	QString fileName = QDir::fromNativeSeparators(m_ConflictsContextItem->data(0, Qt::UserRole).toString());

	// what we have is an absolute path to the file in its actual location (for the primary origin)
	// what we want is the path relative to the virtual data directory

	// we need to look in the virtual directory for the file to make sure the info is up to date.

	// check if the file comes from the actual data folder instead of a mod
	QDir gameDirectory = m_OrganizerCore->managedGame()->dataDirectory().absolutePath();
	QString relativePath = gameDirectory.relativeFilePath(fileName);
	QDir direRelativePath = gameDirectory.relativeFilePath(fileName);
	// if the file is on a different drive the dirRelativePath will actually be an absolute path so we make sure that is not the case
	if (!direRelativePath.isAbsolute() && !relativePath.startsWith("..")) {
		fileName = relativePath;
	}
	else {
		// crude: we search for the next slash after the base mod directory to skip everything up to the data-relative directory
		int offset = m_OrganizerCore->settings().getModDirectory().size() + 1;
		offset = fileName.indexOf("/", offset);
		fileName = fileName.mid(offset + 1);
	}



	const FileEntry::Ptr file = m_OrganizerCore->directoryStructure()->searchFile(ToWString(fileName), nullptr);

	if (file.get() == nullptr) {
		reportError(tr("file not found: %1").arg(qUtf8Printable(fileName)));
		return;
	}

	// set up preview dialog
	PreviewDialog preview(fileName);
	auto addFunc = [&](int originId) {
		FilesOrigin &origin = m_OrganizerCore->directoryStructure()->getOriginByID(originId);
		QString filePath = QDir::fromNativeSeparators(ToQString(origin.getPath())) + "/" + fileName;
		if (QFile::exists(filePath)) {
			// it's very possible the file doesn't exist, because it's inside an archive. we don't support that
			QWidget *wid = m_PluginContainer->previewGenerator().genPreview(filePath);
			if (wid == nullptr) {
				reportError(tr("failed to generate preview for %1").arg(filePath));
			}
			else {
				preview.addVariant(ToQString(origin.getName()), wid);
			}
		}
	};

	addFunc(file->getOrigin());
	for (auto alt : file->getAlternatives()) {
		addFunc(alt.first);
	}
	if (preview.numVariants() > 0) {
		preview.exec();
	}
	else {
		QMessageBox::information(this, tr("Sorry"), tr("Sorry, can't preview anything. This function currently does not support extracting from bsas."));
	}
}


void ModInfoDialog::on_overwriteTree_customContextMenuRequested(const QPoint &pos)
{
  m_ConflictsContextItem = ui->overwriteTree->itemAt(pos.x(), pos.y());

  if (m_ConflictsContextItem != nullptr) {
    // offer to hide/unhide file, but not for files from archives
    if (!m_ConflictsContextItem->data(1, Qt::UserRole + 2).toBool()) {
      QMenu menu;
      if (m_ConflictsContextItem->text(0).endsWith(ModInfo::s_HiddenExt)) {
        menu.addAction(tr("Un-Hide"), this, SLOT(unhideConflictFile()));
      } else {
        menu.addAction(tr("Hide"), this, SLOT(hideConflictFile()));
      }

	  menu.addAction(tr("Open/Execute"), this, SLOT(openDataFile()));

      QString fileName = m_ConflictsContextItem->data(0, Qt::UserRole).toString();
      if (m_PluginContainer->previewGenerator().previewSupported(QFileInfo(fileName).suffix())) {
        menu.addAction(tr("Preview"), this, SLOT(previewDataFile()));
      }

      menu.exec(ui->overwriteTree->mapToGlobal(pos));
    }
  }
}

void ModInfoDialog::on_overwrittenTree_customContextMenuRequested(const QPoint &pos)
{
	m_ConflictsContextItem = ui->overwrittenTree->itemAt(pos.x(), pos.y());

	if (m_ConflictsContextItem != nullptr) {
		if (!m_ConflictsContextItem->data(1, Qt::UserRole + 2).toBool()) {
			QMenu menu;

			menu.addAction(tr("Open/Execute"), this, SLOT(openDataFile()));

			QString fileName = m_ConflictsContextItem->data(0, Qt::UserRole).toString();
			if (m_PluginContainer->previewGenerator().previewSupported(QFileInfo(fileName).suffix())) {
				menu.addAction(tr("Preview"), this, SLOT(previewDataFile()));
			}

			menu.exec(ui->overwrittenTree->mapToGlobal(pos));
		}
	}
}


void ModInfoDialog::on_overwrittenTree_itemDoubleClicked(QTreeWidgetItem *item, int)
{
  emit modOpen(item->data(1, Qt::UserRole).toString(), TAB_CONFLICTS);
  this->accept();
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


void ModInfoDialog::createTweak()
{
  QString name = QInputDialog::getText(this, tr("Name"), tr("Please enter a name"));
  if (name.isNull()) {
    return;
  } else if (!fixDirectoryName(name)) {
    QMessageBox::critical(this, tr("Error"), tr("Invalid name. Must be a valid file name"));
    return;
  } else if (ui->iniTweaksList->findItems(name, Qt::MatchFixedString).count() != 0) {
    QMessageBox::critical(this, tr("Error"), tr("A tweak by that name exists"));
    return;
  }

  QListWidgetItem *newTweak = new QListWidgetItem(name + ".ini");
  newTweak->setData(Qt::UserRole, "INI Tweaks/" + name + ".ini");
  newTweak->setFlags(newTweak->flags() | Qt::ItemIsUserCheckable);
  newTweak->setCheckState(Qt::Unchecked);
  ui->iniTweaksList->addItem(newTweak);
}

void ModInfoDialog::on_iniTweaksList_customContextMenuRequested(const QPoint &pos)
{
  QMenu menu;
  menu.addAction(tr("Create Tweak"), this, SLOT(createTweak()));
  menu.exec(ui->iniTweaksList->mapToGlobal(pos));
}
