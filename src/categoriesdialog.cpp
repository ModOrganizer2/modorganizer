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

#include "categoriesdialog.h"
#include "ui_categoriesdialog.h"
#include "categories.h"
#include "utility.h"
#include <QItemDelegate>
#include <QRegExpValidator>
#include <QLineEdit>
#include <QMenu>


class NewIDValidator : public QIntValidator {
public:
  NewIDValidator(const std::set<int> &ids)
    : m_UsedIDs(ids) {}
  virtual State	validate(QString &input, int &pos) const {
    State intRes = QIntValidator::validate(input, pos);
    if (intRes == Acceptable) {
      bool ok = false;
      int id = input.toInt(&ok);
      if (m_UsedIDs.find(id) != m_UsedIDs.end()) {
        return QValidator::Intermediate;
      }
    }
    return intRes;
  }
private:
  const std::set<int> &m_UsedIDs;
};


class ExistingIDValidator : public QIntValidator {
public:
  ExistingIDValidator(const std::set<int> &ids)
    : m_UsedIDs(ids) {}
  virtual State	validate(QString &input, int &pos) const {
    State intRes = QIntValidator::validate(input, pos);
    if (intRes == Acceptable) {
      bool ok = false;
      int id = input.toInt(&ok);
      if ((id == 0) || (m_UsedIDs.find(id) != m_UsedIDs.end())) {
        return QValidator::Acceptable;
      } else {
        return QValidator::Intermediate;
      }
    } else {
      return intRes;
    }
  }
private:
  const std::set<int> &m_UsedIDs;
};


class ValidatingDelegate : public QItemDelegate {

public:
  ValidatingDelegate(QObject *parent, QValidator *validator)
    : QItemDelegate(parent), m_Validator(validator) {}

  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem&, const QModelIndex&) const
  {
    QLineEdit *edit = new QLineEdit(parent);
    edit->setValidator(m_Validator);
    return edit;
  }
  virtual void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
  {
    QLineEdit *edit = qobject_cast<QLineEdit*>(editor);
    int pos = 0;
    QString editText = edit->text();
    if (m_Validator->validate(editText, pos) == QValidator::Acceptable) {
      QItemDelegate::setModelData(editor, model, index);
    }
  }
private:
  QValidator *m_Validator;
};


CategoriesDialog::CategoriesDialog(QWidget *parent)
  : TutorableDialog("Categories", parent), ui(new Ui::CategoriesDialog)
{
  ui->setupUi(this);
  fillTable();
  connect(ui->categoriesTable, SIGNAL(cellChanged(int,int)), this, SLOT(cellChanged(int,int)));
}

CategoriesDialog::~CategoriesDialog()
{
  delete ui;
}


void CategoriesDialog::cellChanged(int row, int)
{
  int currentID = ui->categoriesTable->item(row, 0)->text().toInt();
  if (currentID > m_HighestID) {
    m_HighestID = currentID;
  }
}


void CategoriesDialog::commitChanges()
{
  CategoryFactory &categories = CategoryFactory::instance();
  categories.reset();

  for (int i = 0; i < ui->categoriesTable->rowCount(); ++i) {
    int index = ui->categoriesTable->verticalHeader()->logicalIndex(i);
    QString nexusIDString = ui->categoriesTable->item(index, 2)->text();
    QStringList nexusIDStringList = nexusIDString.split(',', QString::SkipEmptyParts);
    std::vector<int> nexusIDs;
    for (QStringList::iterator iter = nexusIDStringList.begin();
         iter != nexusIDStringList.end(); ++iter) {
      nexusIDs.push_back(iter->toInt());
    }

    categories.addCategory(
          ui->categoriesTable->item(index, 0)->text().toInt(),
          ui->categoriesTable->item(index, 1)->text(),
          nexusIDs,
          ui->categoriesTable->item(index, 3)->text().toInt());
  }
  categories.setParents();

  categories.saveCategories();
}


void CategoriesDialog::refreshIDs()
{
  m_HighestID = 0;
  for (int i = 0; i < ui->categoriesTable->rowCount(); ++i) {
    int id = ui->categoriesTable->item(i, 0)->text().toInt();
    if (id > m_HighestID) {
      m_HighestID = id;
    }
    m_IDs.insert(id);
  }
}


void CategoriesDialog::fillTable()
{
  CategoryFactory &categories = CategoryFactory::instance();
  QTableWidget *table = ui->categoriesTable;

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
  table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
  table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
  table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
  table->verticalHeader()->setSectionsMovable(true);
  table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
#else
  table->horizontalHeader()->setResizeMode(0, QHeaderView::Fixed);
  table->horizontalHeader()->setResizeMode(1, QHeaderView::Stretch);
  table->horizontalHeader()->setResizeMode(2, QHeaderView::Fixed);
  table->horizontalHeader()->setResizeMode(3, QHeaderView::Fixed);
  table->verticalHeader()->setMovable(true);
  table->verticalHeader()->setResizeMode(QHeaderView::Fixed);
#endif


  table->setItemDelegateForColumn(0, new ValidatingDelegate(this, new NewIDValidator(m_IDs)));
  table->setItemDelegateForColumn(2, new ValidatingDelegate(this, new QRegExpValidator(QRegExp("([0-9]+)?(,[0-9]+)*"), this)));
  table->setItemDelegateForColumn(3, new ValidatingDelegate(this, new ExistingIDValidator(m_IDs)));

  int row = 0;
  for (std::vector<CategoryFactory::Category>::const_iterator iter = categories.m_Categories.begin();
       iter != categories.m_Categories.end(); ++iter, ++row) {
    const CategoryFactory::Category &category = *iter;
    if (category.m_ID == 0) {
      --row;
      continue;
    }
    table->insertRow(row);
//    table->setVerticalHeaderItem(row, new QTableWidgetItem("  "));

    QScopedPointer<QTableWidgetItem> idItem(new QTableWidgetItem());
    idItem->setData(Qt::DisplayRole, category.m_ID);

    QScopedPointer<QTableWidgetItem> nameItem(new QTableWidgetItem(category.m_Name));
    QScopedPointer<QTableWidgetItem> nexusIDItem(new QTableWidgetItem(MOBase::VectorJoin(category.m_NexusIDs, ",")));
    QScopedPointer<QTableWidgetItem> parentIDItem(new QTableWidgetItem());
    parentIDItem->setData(Qt::DisplayRole, category.m_ParentID);

    table->setItem(row, 0, idItem.take());
    table->setItem(row, 1, nameItem.take());
    table->setItem(row, 2, nexusIDItem.take());
    table->setItem(row, 3, parentIDItem.take());
  }

  refreshIDs();
}


void CategoriesDialog::addCategory_clicked()
{
  int row = m_ContextRow >= 0 ? m_ContextRow : 0;
  ui->categoriesTable->insertRow(row);
  ui->categoriesTable->setVerticalHeaderItem(row, new QTableWidgetItem("  "));
  ui->categoriesTable->setItem(row, 0, new QTableWidgetItem(QString::number(++m_HighestID)));
  ui->categoriesTable->setItem(row, 1, new QTableWidgetItem("new"));
  ui->categoriesTable->setItem(row, 2, new QTableWidgetItem(""));
  ui->categoriesTable->setItem(row, 3, new QTableWidgetItem("0"));
}


void CategoriesDialog::removeCategory_clicked()
{
  ui->categoriesTable->removeRow(m_ContextRow);
}


void CategoriesDialog::on_categoriesTable_customContextMenuRequested(const QPoint &pos)
{
  m_ContextRow = ui->categoriesTable->rowAt(pos.y());
  QMenu menu;
  menu.addAction(tr("Add"), this, SLOT(addCategory_clicked()));
  menu.addAction(tr("Remove"), this, SLOT(removeCategory_clicked()));

  menu.exec(ui->categoriesTable->mapToGlobal(pos));
}
