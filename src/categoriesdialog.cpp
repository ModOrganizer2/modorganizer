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
#include "categories.h"
#include "settings.h"
#include "ui_categoriesdialog.h"
#include "utility.h"
#include "nexusinterface.h"
#include <QItemDelegate>
#include <QLineEdit>
#include <QMenu>
#include <QRegularExpressionValidator>

class NewIDValidator : public QIntValidator
{
public:
  NewIDValidator(const std::set<int>& ids) : m_UsedIDs(ids) {}
  virtual State validate(QString& input, int& pos) const
  {
    State intRes = QIntValidator::validate(input, pos);
    if (intRes == Acceptable) {
      bool ok = false;
      int id  = input.toInt(&ok);
      if (m_UsedIDs.find(id) != m_UsedIDs.end()) {
        return QValidator::Intermediate;
      }
    }
    return intRes;
  }

private:
  const std::set<int>& m_UsedIDs;
};

class ExistingIDValidator : public QIntValidator
{
public:
  ExistingIDValidator(const std::set<int>& ids) : m_UsedIDs(ids) {}
  virtual State validate(QString& input, int& pos) const
  {
    State intRes = QIntValidator::validate(input, pos);
    if (intRes == Acceptable) {
      bool ok = false;
      int id  = input.toInt(&ok);
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
  const std::set<int>& m_UsedIDs;
};

class ValidatingDelegate : public QItemDelegate
{

public:
  ValidatingDelegate(QObject* parent, QValidator* validator)
      : QItemDelegate(parent), m_Validator(validator)
  {}

  QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&,
                        const QModelIndex&) const
  {
    QLineEdit* edit = new QLineEdit(parent);
    edit->setValidator(m_Validator);
    return edit;
  }
  virtual void setModelData(QWidget* editor, QAbstractItemModel* model,
                            const QModelIndex& index) const
  {
    QLineEdit* edit  = qobject_cast<QLineEdit*>(editor);
    int pos          = 0;
    QString editText = edit->text();
    if (m_Validator->validate(editText, pos) == QValidator::Acceptable) {
      QItemDelegate::setModelData(editor, model, index);
    }
  }

private:
  QValidator* m_Validator;
};

CategoriesDialog::CategoriesDialog(PluginContainer* pluginContainer, QWidget* parent)
  : TutorableDialog("Categories", parent), ui(new Ui::CategoriesDialog), m_PluginContainer(pluginContainer)
{
  ui->setupUi(this);
  fillTable();
  connect(ui->categoriesTable, SIGNAL(cellChanged(int, int)), this,
          SLOT(cellChanged(int,int)));
  connect(ui->nexusRefresh, SIGNAL(clicked()), this, SLOT(nexusRefresh_clicked()));
  connect(ui->nexusImportButton, SIGNAL(clicked()), this, SLOT(nexusImport_clicked()));
}

CategoriesDialog::~CategoriesDialog()
{
  delete ui;
}

int CategoriesDialog::exec()
{
  GeometrySaver gs(Settings::instance(), this);
  return QDialog::exec();
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
  CategoryFactory* categories = CategoryFactory::instance();
  categories->reset();

  for (int i = 0; i < ui->categoriesTable->rowCount(); ++i) {
    int index = ui->categoriesTable->verticalHeader()->logicalIndex(i);
    QVariantList nexusData = ui->categoriesTable->item(index, 3)->data(Qt::UserRole).toList();
    std::vector<CategoryFactory::NexusCategory> nexusCats;
    for (auto nexusCat : nexusData) {
      nexusCats.push_back(CategoryFactory::NexusCategory(nexusCat.toList()[0].toString(), nexusCat.toList()[1].toInt()));
    }

    categories->addCategory(
          ui->categoriesTable->item(index, 0)->text().toInt(),
          ui->categoriesTable->item(index, 1)->text(),
          nexusCats,
          ui->categoriesTable->item(index, 2)->text().toInt());
  }
  categories->setParents();

  categories->saveCategories();
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
  CategoryFactory* categories = CategoryFactory::instance();
  QTableWidget* table = ui->categoriesTable;
  QListWidget* list = ui->nexusCategoryList;

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
  table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
  table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
  table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
  table->verticalHeader()->setSectionsMovable(true);
  table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  table->setItemDelegateForColumn(0, new ValidatingDelegate(this, new NewIDValidator(m_IDs)));
  table->setItemDelegateForColumn(2, new ValidatingDelegate(this, new ExistingIDValidator(m_IDs)));
  table->setItemDelegateForColumn(3, new ValidatingDelegate(this, new QRegExpValidator(QRegExp("([0-9]+)?(,[0-9]+)*"), this)));

  int row = 0;
  for (std::vector<CategoryFactory::Category>::const_iterator iter = categories->m_Categories.begin();
       iter != categories->m_Categories.end(); ++iter, ++row) {
    const CategoryFactory::Category& category = *iter;
    if (category.m_ID == 0) {
      --row;
      continue;
    }
    table->insertRow(row);
    //    table->setVerticalHeaderItem(row, new QTableWidgetItem("  "));

    QScopedPointer<QTableWidgetItem> idItem(new QTableWidgetItem());
    idItem->setData(Qt::DisplayRole, category.m_ID);

    QScopedPointer<QTableWidgetItem> nameItem(new QTableWidgetItem(category.m_Name));
    QScopedPointer<QTableWidgetItem> parentIDItem(new QTableWidgetItem());
    parentIDItem->setData(Qt::DisplayRole, category.m_ParentID);
    QScopedPointer<QTableWidgetItem> nexusCatItem(new QTableWidgetItem());

    table->setItem(row, 0, idItem.take());
    table->setItem(row, 1, nameItem.take());
    table->setItem(row, 2, parentIDItem.take());
    table->setItem(row, 3, nexusCatItem.take());
  }

  for (auto nexusCat : categories->m_NexusMap)
  {
    QScopedPointer<QListWidgetItem> nexusItem(new QListWidgetItem());
    nexusItem->setData(Qt::DisplayRole, nexusCat.second.m_Name);
    nexusItem->setData(Qt::UserRole, nexusCat.second.m_ID);
    list->addItem(nexusItem.take());
    auto item = table->item(categories->resolveNexusID(nexusCat.first)-1, 3);
    if (item != nullptr) {
      auto itemData = item->data(Qt::UserRole).toList();
      QVariantList newData;
      newData.append(nexusCat.second.m_Name);
      newData.append(nexusCat.second.m_ID);
      itemData.insert(itemData.length(), newData);
      QStringList names;
      for (auto cat : itemData) {
        names.append(cat.toList()[0].toString());
      }
      item->setData(Qt::UserRole, itemData);
      item->setData(Qt::DisplayRole, names.join(", "));
    }
  }

  refreshIDs();
}

void CategoriesDialog::addCategory_clicked()
{
  int row = m_ContextRow >= 0 ? m_ContextRow : 0;
  ui->categoriesTable->insertRow(row);
  ui->categoriesTable->setVerticalHeaderItem(row, new QTableWidgetItem("  "));
  ui->categoriesTable->setItem(row, 0,
                               new QTableWidgetItem(QString::number(++m_HighestID)));
  ui->categoriesTable->setItem(row, 1, new QTableWidgetItem("new"));
  ui->categoriesTable->setItem(row, 2, new QTableWidgetItem("0"));
  ui->categoriesTable->setItem(row, 3, new QTableWidgetItem(""));
}

void CategoriesDialog::removeCategory_clicked()
{
  ui->categoriesTable->removeRow(m_ContextRow);
}


void CategoriesDialog::nexusRefresh_clicked()
{
  NexusInterface *nexus = NexusInterface::instance(m_PluginContainer);
  nexus->requestGameInfo(Settings::instance().game().plugin()->gameShortName(), this, QVariant(), QString());
}


void CategoriesDialog::nexusImport_clicked()
{
  if (QMessageBox::question(nullptr, tr("Import Nexus Categories?"),
    tr("This will overwrite your existing categories with the loaded Nexus categories."),
    QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    QTableWidget* table = ui->categoriesTable;
    QListWidget* list = ui->nexusCategoryList;

    table->setRowCount(0);
    refreshIDs();
    int row = 0;
    for (int i = 0; i < list->count(); ++i) {
      row = table->rowCount();
      table->insertRow(table->rowCount());
      //    table->setVerticalHeaderItem(row, new QTableWidgetItem("  "));

      QScopedPointer<QTableWidgetItem> idItem(new QTableWidgetItem());
      idItem->setData(Qt::DisplayRole, ++m_HighestID);

      QScopedPointer<QTableWidgetItem> nameItem(new QTableWidgetItem(list->item(i)->data(Qt::DisplayRole).toString()));
      QStringList nexusLabel;
      QVariantList nexusData;
      nexusLabel.append(list->item(i)->data(Qt::DisplayRole).toString());
      QVariantList data;
      data.append(QVariant(list->item(i)->data(Qt::DisplayRole).toString()));
      data.append(QVariant(list->item(i)->data(Qt::UserRole).toInt()));
      nexusData.insert(nexusData.size(), data);
      QScopedPointer<QTableWidgetItem> nexusCatItem(new QTableWidgetItem(nexusLabel.join(", ")));
      nexusCatItem->setData(Qt::UserRole, nexusData);
      QScopedPointer<QTableWidgetItem> parentIDItem(new QTableWidgetItem());
      parentIDItem->setData(Qt::DisplayRole, 0);

      table->setItem(row, 0, idItem.take());
      table->setItem(row, 1, nameItem.take());
      table->setItem(row, 2, parentIDItem.take());
      table->setItem(row, 3, nexusCatItem.take());
    }
    refreshIDs();
  }
}


void CategoriesDialog::nxmGameInfoAvailable(QString gameName, QVariant, QVariant resultData, int)
{
  QVariantMap result = resultData.toMap();
  QVariantList categories = result["categories"].toList();
  auto catFactory = CategoryFactory::instance();
  QListWidget* list = ui->nexusCategoryList;
  list->clear();
  for (auto category : categories) {
    auto catMap = category.toMap();
    QScopedPointer<QListWidgetItem> nexusItem(new QListWidgetItem());
    nexusItem->setData(Qt::DisplayRole, catMap["name"].toString());
    nexusItem->setData(Qt::UserRole, catMap["category_id"].toInt());
    list->addItem(nexusItem.take());
  }
}


void CategoriesDialog::on_categoriesTable_customContextMenuRequested(const QPoint& pos)
{
  m_ContextRow = ui->categoriesTable->rowAt(pos.y());
  QMenu menu;
  menu.addAction(tr("Add"), this, SLOT(addCategory_clicked()));
  menu.addAction(tr("Remove"), this, SLOT(removeCategory_clicked()));

  menu.exec(ui->categoriesTable->mapToGlobal(pos));
}
