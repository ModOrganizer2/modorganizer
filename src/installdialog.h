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

#ifndef INSTALLDIALOG_H
#define INSTALLDIALOG_H

#include "mytree.h"
#include "archivetree.h"
#include "tutorabledialog.h"
#include <QDialog>
#include <QUuid>
#include <QTreeWidgetItem>
#include <QProgressDialog>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <archive.h>
#include <directorytree.h>


namespace Ui {
    class InstallDialog;
}


/**
 * a dialog presented to manually define how a mod is to be installed. It provides
 * a tree view of the file contents that can modified directly
 **/
class InstallDialog : public MOBase::TutorableDialog
{
    Q_OBJECT

public:
  /**
   * @brief constructor
   *
   * @param tree tree structure describing the vanilla archive structure. The InstallDialog does NOT take custody of this pointer!
   * @param modName name of the mod. The name can be modified through the dialog
   * @param parent parent widget
   **/
  explicit InstallDialog(MOBase::DirectoryTree *tree, const QString &modName, QWidget *parent = 0);
  ~InstallDialog();

  /**
   * @brief retrieve the (modified) mod name
   *
   * @return updated mod name
   **/
  QString getModName() const;

  /**
   * @brief retrieve the user-modified directory structure
   *
   * @return modified data structure. This is a NEW datatree object for which the caller takes custody
   **/
  MOBase::DirectoryTree *getDataTree() const;

signals:

  void openFile(const QString fileName);

private:

  void updatePreview();
  bool testForProblem();
  void updateProblems();

  void setDataRoot(QTreeWidgetItem* root);

  void updateFileList(QTreeWidgetItem *item, QString targetName, FileData* const *fileData, size_t size) const;

  void updateCheckState(QTreeWidgetItem *item);
//  void recursiveCheck(QTreeWidgetItem *item);
//  void recursiveUncheck(QTreeWidgetItem *item);

  void addDataToTree(MOBase::DirectoryTree::Node *node, QTreeWidgetItem *treeItem);

  void mapDataNode(MOBase::DirectoryTree::Node *node, QTreeWidgetItem *baseItem) const;

  static QString getFullPath(const MOBase::DirectoryTree::Node *node);

private slots:

  void on_treeContent_customContextMenuRequested(QPoint pos);

  void unset_data();
  void use_as_data();
  void create_directory();
  void open_file();

  void treeChanged();

  void on_cancelButton_clicked();

  void on_okButton_clicked();

private:
  Ui::InstallDialog *ui;

  MOBase::DirectoryTree *m_DataTree;

  ArchiveTree *m_Tree;
  QLabel *m_ProblemLabel;
  QTreeWidgetItem *m_TreeRoot;
  QTreeWidgetItem *m_DataRoot;
  QTreeWidgetItem *m_TreeSelection;
  bool m_Updating;

};

#endif // INSTALLDIALOG_H
