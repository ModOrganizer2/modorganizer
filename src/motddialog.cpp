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

#include "motddialog.h"
#include "bbcode.h"
#include "organizercore.h"
#include "ui_motddialog.h"
#include "utility.h"
#include <Shlwapi.h>
#include <utility.h>

using namespace MOBase;

MotDDialog::MotDDialog(const QString& message, QWidget* parent)
    : QDialog(parent), ui(new Ui::MotDDialog)
{
  ui->setupUi(this);
  ui->motdView->setHtml(BBCode::convertToHTML(message));
  connect(ui->motdView, SIGNAL(anchorClicked(QUrl)), this, SLOT(linkClicked(QUrl)));
}

MotDDialog::~MotDDialog()
{
  delete ui;
}

void MotDDialog::on_okButton_clicked()
{
  this->close();
}

void MotDDialog::linkClicked(const QUrl& url)
{
  shell::Open(url);
}
