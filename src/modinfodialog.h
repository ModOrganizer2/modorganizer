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

#ifndef MODINFODIALOG_H
#define MODINFODIALOG_H


#include "modinfo.h"
#include "tutorabledialog.h"
#include "plugincontainer.h"
#include "organizercore.h"
#include "filterwidget.h"
#include "filerenamer.h"
#include "expanderwidget.h"

#include <QDialog>
#include <QSignalMapper>
#include <QSettings>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QModelIndex>
#include <QAction>
#include <QPlainTextEdit>
#include <QListWidgetItem>
#include <QTreeWidgetItem>
#include <QTextCodec>
#include <set>
#include <directoryentry.h>


namespace Ui {
    class ModInfoDialog;
}

class QFileSystemModel;
class QTreeView;
class CategoryFactory;
class TextEditor;


class ModInfoDialogTab : public QObject
{
  Q_OBJECT;

public:
  ModInfoDialogTab() = default;
  ModInfoDialogTab(const ModInfoDialogTab&) = delete;
  ModInfoDialogTab& operator=(const ModInfoDialogTab&) = delete;
  ModInfoDialogTab(ModInfoDialogTab&&) = default;
  ModInfoDialogTab& operator=(ModInfoDialogTab&&) = default;
  virtual ~ModInfoDialogTab() = default;

  virtual void clear() = 0;
  virtual bool feedFile(const QString& rootPath, const QString& filename) = 0;
  virtual bool canClose();
  virtual void saveState(Settings& s);
  virtual void restoreState(const Settings& s);

  virtual void setMod(ModInfo::Ptr mod, MOShared::FilesOrigin* origin);
  virtual void update();

signals:
  void originModified(int originID);
  void modOpen(QString name);

protected:
  void emitOriginModified(int originID);
  void emitModOpen(QString name);
};


class ElideLeftDelegate : public QStyledItemDelegate
{
public:
  using QStyledItemDelegate::QStyledItemDelegate;

protected:
  void initStyleOption(QStyleOptionViewItem* o, const QModelIndex& i) const
  {
    QStyledItemDelegate::initStyleOption(o, i);
    o->textElideMode = Qt::ElideLeft;
  }
};


bool canPreviewFile(PluginContainer& pluginContainer, bool isArchive, const QString& filename);
bool canOpenFile(bool isArchive, const QString& filename);
bool canHideFile(bool isArchive, const QString& filename);
bool canUnhideFile(bool isArchive, const QString& filename);

FileRenamer::RenameResults hideFile(FileRenamer& renamer, const QString &oldName);
FileRenamer::RenameResults unhideFile(FileRenamer& renamer, const QString &oldName);


/**
 * this is a larger dialog used to visualise information about the mod.
 * @todo this would probably a good place for a plugin-system
 **/
class ModInfoDialog : public MOBase::TutorableDialog
{
    Q_OBJECT

public:

  enum ETabs {
    TAB_TEXTFILES,
    TAB_INIFILES,
    TAB_IMAGES,
    TAB_ESPS,
    TAB_CONFLICTS,
    TAB_CATEGORIES,
    TAB_NEXUS,
    TAB_NOTES,
    TAB_FILETREE
  };

public:

 /**
  * @brief constructor
  *
  * @param modInfo info structure about the mod to display
  * @param parent parend widget
  **/
  explicit ModInfoDialog(ModInfo::Ptr modInfo, const MOShared::DirectoryEntry *directory, bool unmanaged, OrganizerCore *organizerCore, PluginContainer *pluginContainer, QWidget *parent = 0);

  ~ModInfoDialog();

  /**
   * @brief retrieve the (user-modified) version of the mod
   *
   * @return the (user-modified) version of the mod
   **/
  QString getModVersion() const;

  /**
   * @brief retrieve the (user-modified) mod id
   *
   * @return the (user-modified) id of the mod
   **/
  const int getModID() const;

  /**
   * @brief open the specified tab in the dialog if it's enabled
   *
   * @param tab the tab to activate
   **/
  void openTab(int tab);

  int exec() override;

  void saveState(Settings& s) const;
  void restoreState(const Settings& s);

signals:

  void linkActivated(const QString &link);
  void downloadRequest(const QString &link);
  void modOpen(const QString &modName, int tab);
  void modOpenNext(int tab=-1);
  void modOpenPrev(int tab=-1);
  void originModified(int originID);
  void endorseMod(ModInfo::Ptr nexusID);

public slots:

  void modDetailsUpdated(bool success);

private:

  void initFiletree(ModInfo::Ptr modInfo);

  void refreshLists();

  void addCategories(const CategoryFactory &factory, const std::set<int> &enabledCategories, QTreeWidgetItem *root, int rootLevel);

  void updateVersionColor();

  void refreshNexusData(int modID);
  void activateNexusTab();
  QString getFileCategory(int categoryID);
  bool recursiveDelete(const QModelIndex &index);
  void deleteFile(const QModelIndex &index);
  void saveCategories(QTreeWidgetItem *currentNode);
  void addCheckedCategories(QTreeWidgetItem *tree);
  void refreshPrimaryCategoriesBox();

  int tabIndex(const QString &tabId);

private slots:
  void linkClicked(const QUrl &url);
  void linkClicked(QString url);

  void delete_activated();

  void createDirectoryTriggered();
  void openTriggered();
  void previewTriggered();
  void renameTriggered();
  void deleteTriggered();
  void hideTriggered();
  void unhideTriggered();

  void on_openInExplorerButton_clicked();
  void on_closeButton_clicked();
  void on_visitNexusLabel_linkActivated(const QString &link);
  void on_modIDEdit_editingFinished();
  void on_sourceGameEdit_currentIndexChanged(int);
  void on_versionEdit_editingFinished();
  void on_customUrlLineEdit_editingFinished();
  void on_tabWidget_currentChanged(int index);
  void on_primaryCategoryBox_currentIndexChanged(int index);
  void on_categoriesTree_itemChanged(QTreeWidgetItem *item, int column);
  void on_fileTree_customContextMenuRequested(const QPoint &pos);

  void on_refreshButton_clicked();

  void on_endorseBtn_clicked();

  void on_nextButton_clicked();

  void on_prevButton_clicked();

private:
  using FileEntry = MOShared::FileEntry;

  Ui::ModInfoDialog *ui;

  ModInfo::Ptr m_ModInfo;

  std::vector<std::unique_ptr<ModInfoDialogTab>> m_tabs;

  QString m_RootPath;

  OrganizerCore *m_OrganizerCore;
  PluginContainer *m_PluginContainer;

  QFileSystemModel *m_FileSystemModel;
  QTreeView *m_FileTree;
  QModelIndexList m_FileSelection;

  QSettings *m_Settings;

  std::set<int> m_RequestIDs;
  bool m_RequestStarted;

  QAction *m_NewFolderAction;
  QAction *m_OpenAction;
  QAction *m_PreviewAction;
  QAction *m_RenameAction;
  QAction *m_DeleteAction;
  QAction *m_HideAction;
  QAction *m_UnhideAction;

  const MOShared::DirectoryEntry *m_Directory;
  MOShared::FilesOrigin *m_Origin;

  std::map<int, int> m_RealTabPos;


  std::vector<std::unique_ptr<ModInfoDialogTab>> createTabs();

  void refreshFiles();

  void restoreTabState(const QByteArray &state);
  QByteArray saveTabState() const;

  void changeFiletreeVisibility(bool visible);
};

#endif // MODINFODIALOG_H
