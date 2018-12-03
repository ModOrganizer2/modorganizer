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
#include "ui_listdialog.h"

ListDialog::ListDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ListDialog)
{
    ui->setupUi(this);
}

ListDialog::~ListDialog()
{
    delete ui;
}

void ListDialog::setChoices(QStringList choices)
{
  ui->listWidget->clear();
  ui->listWidget->addItems(choices);
}

QString ListDialog::getChoice() const
{
  if (ui->listWidget->selectedItems().length()) {
    return ui->listWidget->currentItem()->text();
  } else {
    return "";
  }
}
