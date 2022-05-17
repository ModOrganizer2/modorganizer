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

#include "queryoverwritedialog.h"
#include "ui_queryoverwritedialog.h"

#include <QStyle>

QueryOverwriteDialog::QueryOverwriteDialog(QWidget* parent, Backup b)
    : QDialog(parent), ui(new Ui::QueryOverwriteDialog), m_Action(ACT_NONE)
{
  ui->setupUi(this);
  ui->backupBox->setChecked(b == BACKUP_YES);
  QIcon icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxQuestion);
  ui->iconLabel->setPixmap(icon.pixmap(128));
}

QueryOverwriteDialog::~QueryOverwriteDialog()
{
  delete ui;
}

bool QueryOverwriteDialog::backup() const
{
  return ui->backupBox->isChecked();
}

void QueryOverwriteDialog::on_mergeBtn_clicked()
{
  this->m_Action = ACT_MERGE;
  this->accept();
}

void QueryOverwriteDialog::on_replaceBtn_clicked()
{
  this->m_Action = ACT_REPLACE;
  this->accept();
}

void QueryOverwriteDialog::on_renameBtn_clicked()
{
  this->m_Action = ACT_RENAME;
  this->accept();
}

void QueryOverwriteDialog::on_cancelBtn_clicked()
{
  this->reject();
}
