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

#include <QDialog>
#include <QSignalMapper>
#include <QSettings>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QModelIndex>
#include <QAction>
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

/**
 * this is a larger dialog used to visualise information abount the mod.
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
 explicit ModInfoDialog(const ModInfo::Ptr& modInfo, const MOShared::DirectoryEntry *directory, bool unmanaged, OrganizerCore *organizerCore, PluginContainer *pluginContainer, QWidget *parent = 0);
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

  void restoreTabState(const QByteArray &state);

  QByteArray saveTabState() const;

signals:

  void thumbnailClickedSignal(const QString &filename);
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

  void initFiletree();
  void initINITweaks();

  void refreshLists();

  void addCategories(const CategoryFactory &factory, const std::set<int> &enabledCategories, QTreeWidgetItem *root, int rootLevel);

  void updateVersionColor();

  void refreshNexusData(int modID);
  void activateNexusTab();
  QString getFileCategory(int categoryID);
  bool recursiveDelete(const QModelIndex &index);
  void deleteFile(const QModelIndex &index);
  void openFile(const QModelIndex &index);
  void saveIniTweaks();
  void saveCategories(QTreeWidgetItem *currentNode);
  void saveCurrentTextFile();
  void saveCurrentIniFile();
  void openTextFile(const QString &fileName);
  void openIniFile(const QString &fileName);
  bool allowNavigateFromTXT();
  bool allowNavigateFromINI();
  bool hideFile(const QString &oldName);
  bool unhideFile(const QString &oldName);
  void addCheckedCategories(QTreeWidgetItem *tree);
  void refreshPrimaryCategoriesBox();

  int tabIndex(const QString &tabId);

private slots:

  void hideConflictFile();
  void unhideConflictFile();
  int getBinaryExecuteInfo(const QFileInfo &targetInfo, QFileInfo &binaryInfo, QString &arguments);
  void previewDataFile();
  void openDataFile();


  void thumbnailClicked(const QString &fileName);
  void linkClicked(const QUrl &url);
  void linkClicked(QString url);

  void delete_activated();

  void deleteTriggered();
  void renameTriggered();
  void openTriggered();
  void createDirectoryTriggered();
  void hideTriggered();
  void unhideTriggered();

  void on_openInExplorerButton_clicked();
  void on_closeButton_clicked();
  void on_saveButton_clicked();
  void on_activateESP_clicked();
  void on_deactivateESP_clicked();
  void on_saveTXTButton_clicked();
  void on_visitNexusLabel_linkActivated(const QString &link);
  void on_modIDEdit_editingFinished();
  void on_sourceGameEdit_currentIndexChanged(int);
  void on_versionEdit_editingFinished();
  void on_iniFileView_textChanged();
  void on_textFileView_textChanged();
  void on_tabWidget_currentChanged(int index);
  void on_primaryCategoryBox_currentIndexChanged(int index);
  void on_categoriesTree_itemChanged(QTreeWidgetItem *item, int column);
  void on_textFileList_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
  void on_iniFileList_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
  void on_iniTweaksList_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
  void on_overwriteTree_itemDoubleClicked(QTreeWidgetItem *item, int column);
  void on_overwrittenTree_itemDoubleClicked(QTreeWidgetItem *item, int column);
  void on_overwriteTree_customContextMenuRequested(const QPoint &pos);
  void on_overwrittenTree_customContextMenuRequested(const QPoint &pos);
  void on_fileTree_customContextMenuRequested(const QPoint &pos);

  void on_refreshButton_clicked();

  void on_endorseBtn_clicked();

  void on_nextButton_clicked();

  void on_prevButton_clicked();

  void on_iniTweaksList_customContextMenuRequested(const QPoint &pos);

  void createTweak();
private:

  Ui::ModInfoDialog *ui;

  ModInfo::Ptr m_ModInfo;
  int m_OriginID{};

  QSignalMapper m_ThumbnailMapper;
  QString m_RootPath;

  OrganizerCore *m_OrganizerCore;
  PluginContainer *m_PluginContainer;

  QFileSystemModel *m_FileSystemModel{};
  QTreeView *m_FileTree{};
  QModelIndexList m_FileSelection;

  QSettings *m_Settings;

  std::set<int> m_RequestIDs;
  bool m_RequestStarted;

  QAction *m_DeleteAction;
  QAction *m_RenameAction;
  QAction *m_OpenAction;
  QAction *m_NewFolderAction{};
  QAction *m_HideAction{};
  QAction *m_UnhideAction{};

  QTreeWidgetItem *m_ConflictsContextItem{};

  const MOShared::DirectoryEntry *m_Directory;
  MOShared::FilesOrigin *m_Origin;

  std::map<int, int> m_RealTabPos;

};

#endif // MODINFODIALOG_H
