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

#include "baincomplexinstallerdialog.h"
#include "textviewer.h"
#include "ui_baincomplexinstallerdialog.h"

#include <QDir>

BainComplexInstallerDialog::BainComplexInstallerDialog(DirectoryTree *tree, const QString &modName, bool hasPackageTXT, QWidget *parent)
  : TutorableDialog("BainInstaller", parent), ui(new Ui::BainComplexInstallerDialog), m_Manual(false)
{
  ui->setupUi(this);

  ui->nameEdit->setText(modName);

  for (DirectoryTree::const_node_iterator iter = tree->nodesBegin(); iter != tree->nodesEnd(); ++iter) {
    const QString &dirName = (*iter)->getData().name;
    if ((dirName.compare("fomod", Qt::CaseInsensitive) == 0) ||
        (dirName.startsWith("--"))) {
      continue;
    }

    QListWidgetItem *item = new QListWidgetItem(ui->optionsList);
    item->setText((*iter)->getData().name);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(item->text().mid(0, 2) == "00" ? Qt::Checked : Qt::Unchecked);
    item->setData(Qt::UserRole, qVariantFromValue((void*)(*iter)));
    ui->optionsList->addItem(item);
  }

  ui->packageBtn->setEnabled(hasPackageTXT);
}


BainComplexInstallerDialog::~BainComplexInstallerDialog()
{
  delete ui;
}


QString BainComplexInstallerDialog::getName() const
{
  return ui->nameEdit->text();
}


void BainComplexInstallerDialog::moveTreeUp(DirectoryTree *target, DirectoryTree::Node *child)
{
  for (DirectoryTree::const_node_iterator iter = child->nodesBegin();
       iter != child->nodesEnd();) {
    target->addNode(*iter, true);
    iter = child->detach(iter);
  }

  for (DirectoryTree::const_leaf_reverse_iterator iter = child->leafsRBegin();
       iter != child->leafsREnd(); ++iter) {
    target->addLeaf(*iter);
  }
}


DirectoryTree *BainComplexInstallerDialog::updateTree(DirectoryTree *tree)
{
  DirectoryTree *newTree = new DirectoryTree;

  for (DirectoryTree::const_node_reverse_iterator iter = tree->nodesRBegin();
       iter != tree->nodesREnd();) {
    QList<QListWidgetItem*> items = ui->optionsList->findItems((*iter)->getData().name, Qt::MatchFixedString);
    if ((items.count() == 1) && (items.at(0)->checkState() == Qt::Checked)) {
      moveTreeUp(newTree, *iter);
    }
    iter = tree->erase(iter);
  }

  return newTree;
}


void BainComplexInstallerDialog::on_okBtn_clicked()
{
  this->accept();
}


void BainComplexInstallerDialog::on_cancelBtn_clicked()
{
  this->reject();
}


void BainComplexInstallerDialog::on_manualBtn_clicked()
{
  m_Manual = true;
  this->reject();
}

void BainComplexInstallerDialog::on_packageBtn_clicked()
{
  TextViewer viewer(this);
  viewer.setDescription("");
  viewer.addFile(QDir::tempPath().append("/package.txt"), false);
  viewer.exec();
}
