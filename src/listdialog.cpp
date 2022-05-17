/*
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

#include "listdialog.h"
#include "settings.h"
#include "ui_listdialog.h"

ListDialog::ListDialog(QWidget* parent)
    : QDialog(parent), ui(new Ui::ListDialog), m_Choices()
{
  ui->setupUi(this);
  ui->filterEdit->setFocus();
  connect(ui->choiceList, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
}

ListDialog::~ListDialog()
{
  delete ui;
}

int ListDialog::exec()
{
  GeometrySaver gs(Settings::instance(), this);
  return QDialog::exec();
}

void ListDialog::setChoices(QStringList choices)
{
  m_Choices = choices;
  ui->choiceList->clear();
  ui->choiceList->addItems(m_Choices);
}

QString ListDialog::getChoice() const
{
  if (ui->choiceList->selectedItems().length()) {
    return ui->choiceList->currentItem()->text();
  } else {
    return "";
  }
}

void ListDialog::on_filterEdit_textChanged(QString filter)
{
  QStringList newChoices;
  for (auto choice : m_Choices) {
    if (choice.contains(filter, Qt::CaseInsensitive)) {
      newChoices << choice;
    }
  }
  ui->choiceList->clear();
  ui->choiceList->addItems(newChoices);

  if (newChoices.length() == 1) {
    QListWidgetItem* item = ui->choiceList->item(0);
    item->setSelected(true);
    ui->choiceList->setCurrentItem(item);
  }

  if (!filter.isEmpty()) {
    ui->choiceList->setStyleSheet("QListWidget { border: 2px ridge #f00; }");
  } else {
    ui->choiceList->setStyleSheet("");
  }
}
