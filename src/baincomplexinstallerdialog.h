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

#ifndef BAINCOMPLEXINSTALLERDIALOG_H
#define BAINCOMPLEXINSTALLERDIALOG_H


#include "mytree.h"
#include "installdialog.h"
#include "tutorabledialog.h"


namespace Ui {
class BainComplexInstallerDialog;
}


/**
 * @brief Dialog to choose from options offered by a (complex) bain package
 **/
class BainComplexInstallerDialog : public MOBase::TutorableDialog
{
  Q_OBJECT
  
public:
 /**
  * @brief Constructor
  *
  * @param tree the directory tree of the archive. The caller is resonsible to verify this actually qualifies as a bain installer
  * @param modName proposed name for the mod. The dialog allows the user to change this
  * @param hasPackageTXT set to true if a package.txt is available for this archive. The file has to be unpacked to QDir::tempPath before displaying the dialog!
  * @param parent parent widget
  **/
 explicit BainComplexInstallerDialog(MOBase::DirectoryTree *tree, const QString &modName, bool hasPackageTXT, QWidget *parent);
  ~BainComplexInstallerDialog();

  /**
   * @return bool true if the user requested the manual dialog
   **/
  bool manualRequested() const { return m_Manual; }

  /**
   * @return QString the (user-modified) name to be used for the mod
   **/
  QString getName() const;

  /**
   * @brief retrieve the updated archive tree from the dialog. The caller is responsible to delete the returned tree.
   * 
   * @note This call is destructive on the input tree!
   *
   * @param tree input tree. (TODO isn't this the same as the tree passed in the constructor?)
   * @return DataTree* a new tree with only the selected options and directories arranged correctly. The caller takes custody of this pointer!
   **/
  MOBase::DirectoryTree *updateTree(MOBase::DirectoryTree *tree);

private slots:

  void on_manualBtn_clicked();

  void on_okBtn_clicked();

  void on_cancelBtn_clicked();

  void on_packageBtn_clicked();

private:

  void moveTreeUp(MOBase::DirectoryTree *target, MOBase::DirectoryTree::Node *child);

private:

  Ui::BainComplexInstallerDialog *ui;

  bool m_Manual;

};

#endif // BAINCOMPLEXINSTALLERDIALOG_H
