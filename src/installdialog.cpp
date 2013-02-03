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

#include "installdialog.h"
#include "ui_installdialog.h"

#include "report.h"
#include "utility.h"
#include "installationtester.h"

#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>


InstallDialog::InstallDialog(DirectoryTree *tree, const QString &modName, QWidget *parent)
  : TutorableDialog("InstallDialog", parent), ui(new Ui::InstallDialog),
    m_DataTree(tree), m_TreeRoot(NULL), m_DataRoot(NULL), m_TreeSelection(NULL),
    m_Updating(false)
{
  ui->setupUi(this);

  QLineEdit *editName = findChild<QLineEdit*>("editName");
  editName->setText(modName);

  m_Tree = findChild<ArchiveTree*>("treeContent");

  m_ProblemLabel = findChild<QLabel*>("problemLabel");

  connect(m_Tree, SIGNAL(changed()), this, SLOT(treeChanged()));

  updatePreview();
}

InstallDialog::~InstallDialog()
{
  delete ui;
}


QString InstallDialog::getModName() const
{
  QLineEdit *editName = findChild<QLineEdit*>("editName");
  return editName->text();
}


void InstallDialog::mapDataNode(DirectoryTree::Node *node, QTreeWidgetItem *baseItem) const
{
  for (int i = 0; i < baseItem->childCount(); ++i) {
    QTreeWidgetItem *currentItem = baseItem->child(i);

    if (currentItem->checkState(0) != Qt::Unchecked) {
      if (currentItem->data(0, Qt::UserRole).isNull()) {
        DirectoryTree::Node *newNode = new DirectoryTree::Node;
        newNode->setData(currentItem->text(0));
        mapDataNode(newNode, currentItem);
        node->addNode(newNode, true);
      } else {
        node->addLeaf(FileTreeInformation(currentItem->text(0), currentItem->data(0, Qt::UserRole).toInt()));
      }
    }
  }
}


DirectoryTree *InstallDialog::getDataTree() const
{
  DirectoryTree *base = new DirectoryTree;

  mapDataNode(base, m_Tree->topLevelItem(0));
  return base;
}


QString InstallDialog::getFullPath(const DirectoryTree::Node *node)
{
  QString result(node->getData().name);
  const DirectoryTree::Node *parent = node->getParent();
  while (parent != NULL) {
    if (parent->getParent() != NULL) {
      result.prepend("\\");
    }
    result.prepend(parent->getData().name);
    parent = parent->getParent();
  }
  return result;
}


void InstallDialog::addDataToTree(DirectoryTree::Node *node, QTreeWidgetItem *treeItem)
{
  QString path = getFullPath(node);

  // add directory elements
  for (DirectoryTree::node_iterator iter = node->nodesBegin(); iter != node->nodesEnd(); ++iter) {
    QStringList fields((*iter)->getData().name);
    QTreeWidgetItem *newNodeItem = new QTreeWidgetItem(treeItem, fields);
    newNodeItem->setFlags(newNodeItem->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsTristate);
    newNodeItem->setCheckState(0, Qt::Checked);
    addDataToTree(*iter, newNodeItem);
    treeItem->addChild(newNodeItem);
  }

  // add file elements
  for (DirectoryTree::leaf_iterator iter = node->leafsBegin(); iter != node->leafsEnd(); ++iter) {
    QStringList fields(iter->getName());

    QTreeWidgetItem *newLeafItem = new QTreeWidgetItem(treeItem, fields);
    newLeafItem->setFlags(newLeafItem->flags() | Qt::ItemIsUserCheckable);
    newLeafItem->setCheckState(0, Qt::Checked);
    if (path.size() != 0) {
      newLeafItem->setToolTip(0, path.mid(0).append("\\").append(iter->getName()));
    } else {
      newLeafItem->setToolTip(0, iter->getName());
    }
    newLeafItem->setData(0, Qt::UserRole, iter->getIndex());

    treeItem->addChild(newLeafItem);
  }
}


void InstallDialog::updatePreview()
{
  m_Updating = true;
  m_Tree->clear();
  delete m_TreeRoot;

  m_TreeRoot = new QTreeWidgetItem(m_Tree, QStringList("<data>"));

  addDataToTree(m_DataTree, m_TreeRoot);

  setDataRoot(m_TreeRoot);
  m_Updating = false;
  updateProblems();
}


bool InstallDialog::testForProblem()
{
  bool ok = false;
  QTreeWidgetItem *tlWidget = m_Tree->topLevelItem(0);
  for (int i = 0; i < tlWidget->childCount(); ++i) {
    QTreeWidgetItem *widget = tlWidget->child(i);
    if (widget->checkState(0) == Qt::Unchecked) {
      continue;
    }

    if (widget->data(0, Qt::UserRole).isValid()) {
      // file
      if (InstallationTester::isTopLevelSuffix(widget->text(0))) {
        ok = true;
        break;
      }
    } else {
      // directory
      if (InstallationTester::isTopLevelDirectory(widget->text(0))) {
        ok = true;
        break;
      }
    }
  }
  return ok;
}


void InstallDialog::updateProblems()
{

  if (testForProblem()) {
    m_ProblemLabel->setText(tr("Looks good"));
    m_ProblemLabel->setToolTip(tr("No problem detected"));
    m_ProblemLabel->setStyleSheet("color: darkGreen;");
  } else {
    m_ProblemLabel->setText(tr("No game data on top level"));
    m_ProblemLabel->setToolTip(tr("There is no esp/esm file or asset directory (textures, meshes, interface, ...) "
                                  "on the top level."));
    m_ProblemLabel->setStyleSheet("color: red;");
  }
}


void InstallDialog::setDataRoot(QTreeWidgetItem* root)
{
  if (root != NULL) {
    m_DataRoot = root;

    m_Tree->takeTopLevelItem(0);
    QTreeWidgetItem *temp = root->clone();
//    temp->setCheckState(0, Qt::Checked);
    temp->setFlags(temp->flags() & ~(Qt::ItemIsUserCheckable | Qt::ItemIsTristate));
    temp->setText(0, "<data>");
    temp->setData(0, Qt::CheckStateRole, QVariant());
    m_Tree->addTopLevelItem(temp);
    temp->setExpanded(true);
  } else {
    m_Tree->takeTopLevelItem(0);
  }
  updateProblems();
}


void InstallDialog::use_as_data()
{
  if (m_TreeSelection != NULL) {
    setDataRoot(m_TreeSelection);
    m_TreeSelection = NULL;
  }
  updateProblems();
}


void InstallDialog::unset_data()
{
  m_TreeSelection = NULL;

  setDataRoot(m_TreeRoot);
  updateProblems();
}


void InstallDialog::create_directory()
{
  bool ok = false;
  QString result = QInputDialog::getText(this, tr("Enter a directory name"), tr("Name"),
                                         QLineEdit::Normal, QString(), &ok);
  if (ok && !result.isEmpty()) {
    for (int i = 0; i < m_TreeSelection->childCount(); ++i) {
      if (m_TreeSelection->child(i)->text(0) == result) {
        reportError(tr("A directory with that name exists"));
        return;
      }
    }
    QStringList fields(result);
    QTreeWidgetItem *newItem = new QTreeWidgetItem(m_TreeSelection, fields);
    newItem->setFlags(newItem->flags() | Qt::ItemIsUserCheckable);
    newItem->setCheckState(0, Qt::Checked);
    m_TreeSelection->addChild(newItem);
    updateProblems();
  }
}

void InstallDialog::open_file()
{
  emit openFile(m_TreeSelection->toolTip(0));
}


void InstallDialog::on_treeContent_customContextMenuRequested(QPoint pos)
{
  m_TreeSelection = m_Tree->itemAt(pos);
  if (m_TreeSelection == 0) {
    return;
  }

  QMenu menu;
  menu.addAction(tr("Set data directory"), this, SLOT(use_as_data()));
  menu.addAction(tr("Unset data directory"), this, SLOT(unset_data()));
  if (m_TreeSelection->data(0, Qt::UserRole).isNull()) {
    menu.addAction(tr("Create directory..."), this, SLOT(create_directory()));
  } else {
    menu.addAction(tr("&Open"), this, SLOT(open_file()));
  }
  menu.exec(m_Tree->mapToGlobal(pos));
}


void InstallDialog::treeChanged()
{
  updateProblems();
}


void InstallDialog::on_okButton_clicked()
{
  if (!testForProblem()) {
    if (QMessageBox::question(this, tr("Continue?"), tr("This mod was probably NOT set up correctly, most likely it will NOT work. Really continue?"),
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::No) {
      return;
    }
  }
  this->accept();
}

void InstallDialog::on_cancelButton_clicked()
{
  this->reject();
}
