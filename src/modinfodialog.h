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
* Renames individual files and handles dialog boxes to confirm replacements and
* failures with the user
**/
class FileRenamer
{
public:
  /**
  * controls appearance and replacement behaviour; if RENAME_REPLACE_ALL and
  * RENAME_REPLACE_NONE are not provided, the user will have the option to
  * choose on the first replacement
  **/
  enum RenameFlags
  {
    /**
    * this renamer will be used on multiple files, so display additional
    * buttons to replace all and for canceling
    **/
    MULTIPLE      = 0x01,

    /**
    * customizes some of the text shown on dialog to mention that files are
    * being hidden
    **/
    HIDE          = 0x02,

    /**
    * customizes some of the text shown on dialog to mention that files are
    * being unhidden
    **/
    UNHIDE        = 0x04,

    /**
    * silently replaces all existing files
    **/
    REPLACE_ALL   = 0x08,

    /**
    * silently skips all existing files
    **/
    REPLACE_NONE  = 0x10,
  };


  /** result of a single rename
  *
  **/
  enum RenameResults
  {
    /**
    * the user skipped this file
    */
    RESULT_SKIP,

    /**
    * the file was successfully renamed
    */
    RESULT_OK,

    /**
    * the user wants to cancel
    */
    RESULT_CANCEL
  };


  /**
  * @param parent Parent widget for dialog boxes
  **/
  FileRenamer(QWidget* parent, QFlags<RenameFlags> flags);

  /**
  * renames the given file
  * @param oldName current filename
  * @param newName new filename
  * @return whether the file was renamed, skipped or the user wants to cancel
  **/
  RenameResults rename(const QString& oldName, const QString& newName);

private:
  /**
  *user's decision when replacing
  **/
  enum RenameDecision
  {
    /**
    * replace the file
    **/
    DECISION_REPLACE,

    /**
    * skip the file
    **/
    DECISION_SKIP,

    /**
    * cancel the whole thing
    **/
    DECISION_CANCEL
  };

  /**
  * parent widget for dialog boxes
  **/
  QWidget* m_parent;

  /**
  * flags
  **/
  QFlags<RenameFlags> m_flags;

  /**
  * asks the user to replace an existing file, may return early if the user
  * has already selected to replace all/none
  * @return whether to replace, skip or cancel
  **/
  RenameDecision confirmReplace(const QString& newName);

  /**
  * removal of a file failed, ask the user to continue or cancel
  * @param name The name of the file that failed to be removed
  * @return true to continue, false to stop
  **/
  bool removeFailed(const QString& name);

  /**
  * renaming a file failed, ask the user to continue or cancel
  * @param oldName current filename
  * @param newName new filename
  * @return true to continue, false to stop
  **/
  bool renameFailed(const QString& oldName, const QString& newName);
};


/* Takes a QToolButton and a widget and creates an expandable widget.
 **/
class ExpanderWidget
{
public:
  /** empty expander, use set()
   **/
  ExpanderWidget();

  /** see set()
   **/
  ExpanderWidget(QToolButton* button, QWidget* content);

  /** @brief sets the button and content widgets to use
    * the button will be given an arrow icon, clicking it will toggle the
    * visibility of the given widget
    * @param button the button that toggles the content
    * @param content the widget that will be shown or hidden
    * @param opened initial state, defaults to closed
    **/
  void set(QToolButton* button, QWidget* content, bool opened=false);

  /** either opens or closes the expander depending on the current state
   **/
  void toggle();

  /** sets the current state of the expander
   **/
  void toggle(bool b);

  /** returns whether the expander is currently opened
   **/
  bool opened() const;

private:
  QToolButton* m_button;
  QWidget* m_content;
  bool opened_;
};


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

  void saveState(Settings& s) const;
  void restoreState(const Settings& s);

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

  void initFiletree(ModInfo::Ptr modInfo);
  void initINITweaks();

  void refreshLists();

  void addCategories(const CategoryFactory &factory, const std::set<int> &enabledCategories, QTreeWidgetItem *root, int rootLevel);

  void updateVersionColor();

  void refreshNexusData(int modID);
  void activateNexusTab();
  QString getFileCategory(int categoryID);
  bool recursiveDelete(const QModelIndex &index);
  void deleteFile(const QModelIndex &index);
  void saveIniTweaks();
  void saveCategories(QTreeWidgetItem *currentNode);
  void saveCurrentTextFile();
  void saveCurrentIniFile();
  void openTextFile(const QString &fileName);
  void openIniFile(const QString &fileName);
  bool allowNavigateFromTXT();
  bool allowNavigateFromINI();
  FileRenamer::RenameResults hideFile(FileRenamer& renamer, const QString &oldName);
  FileRenamer::RenameResults unhideFile(FileRenamer& renamer, const QString &oldName);
  void addCheckedCategories(QTreeWidgetItem *tree);
  void refreshPrimaryCategoriesBox();

  int tabIndex(const QString &tabId);

private slots:
  void thumbnailClicked(const QString &fileName);
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
  void on_saveButton_clicked();
  void on_activateESP_clicked();
  void on_deactivateESP_clicked();
  void on_saveTXTButton_clicked();
  void on_visitNexusLabel_linkActivated(const QString &link);
  void on_modIDEdit_editingFinished();
  void on_sourceGameEdit_currentIndexChanged(int);
  void on_versionEdit_editingFinished();
  void on_customUrlLineEdit_editingFinished();
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
  void on_noConflictTree_customContextMenuRequested(const QPoint &pos);
  void on_fileTree_customContextMenuRequested(const QPoint &pos);

  void on_refreshButton_clicked();

  void on_endorseBtn_clicked();

  void on_nextButton_clicked();

  void on_prevButton_clicked();

  void on_iniTweaksList_customContextMenuRequested(const QPoint &pos);

  void createTweak();
private:
  struct ConflictActions
  {
    QAction* hide;
    QAction* unhide;
    QAction* open;
    QAction* preview;

    ConflictActions()
      : hide(nullptr), unhide(nullptr), open(nullptr), preview(nullptr)
    {
    }
  };

  Ui::ModInfoDialog *ui;

  ModInfo::Ptr m_ModInfo;

  QSignalMapper m_ThumbnailMapper;
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

  ExpanderWidget m_overwriteExpander, m_overwrittenExpander, m_nonconflictExpander;
  FilterWidget m_advancedConflictFilter;


  void refreshConflictLists(bool refreshGeneral, bool refreshAdvanced);
  void refreshFiles();

  QTreeWidgetItem* createOverwriteItem(
    bool archive, const QString& fileName, const QString& relativeName,
    const MOShared::FileEntry::AlternativesVector& alternatives);

  QTreeWidgetItem* createNoConflictItem(
    bool archive, const QString& fileName, const QString& relativeName);

  QTreeWidgetItem* createOverwrittenItem(
    int fileOrigin, bool archive,
    const QString& fileName, const QString& relativeName);

  QTreeWidgetItem* createAdvancedConflictItem(
    int fileOrigin, bool archive,
    const QString& fileName, const QString& relativeName,
    const MOShared::FileEntry::AlternativesVector& alternatives);


  void restoreTabState(const QByteArray &state);
  void restoreConflictExpandersState(const QByteArray &state);

  QByteArray saveTabState() const;
  QByteArray saveConflictExpandersState() const;

  bool canHideConflictItem(const QTreeWidgetItem* item) const;
  bool canUnhideConflictItem(const QTreeWidgetItem* item) const;
  bool canPreviewConflictItem(const QTreeWidgetItem* item) const;

  void openDataFile(const QTreeWidgetItem* item);
  void previewDataFile(const QTreeWidgetItem* item);
  void changeFiletreeVisibility(bool visible);

  void openConflictItems(const QList<QTreeWidgetItem*>& items);
  void previewConflictItems(const QList<QTreeWidgetItem*>& items);
  void changeConflictItemsVisibility(
    const QList<QTreeWidgetItem*>& items, bool visible);

  bool canPreviewFile(bool isArchive, const QString& filename) const;
  bool canHideFile(bool isArchive, const QString& filename) const;
  bool canUnhideFile(bool isArchive, const QString& filename) const;

  void showConflictMenu(const QPoint &pos, QTreeWidget* tree);

  ConflictActions createConflictMenuActions(
    const QList<QTreeWidgetItem*> selection);
};

#endif // MODINFODIALOG_H
