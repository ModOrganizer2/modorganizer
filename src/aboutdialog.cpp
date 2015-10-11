/*
Copyright (C) 2014 Sebastian Herbord. All rights reserved.

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


#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include <utility.h>


AboutDialog::AboutDialog(const QString &version, QWidget *parent)
  : QDialog(parent)
  , ui(new Ui::AboutDialog)
{
  ui->setupUi(this);

  m_LicenseFiles[LICENSE_LGPL3] = "lgpl-3.0.txt";
  m_LicenseFiles[LICENSE_GPL3] = "gpl-3.0.txt";
  m_LicenseFiles[LICENSE_BSD3] = "bsd3.txt";
  m_LicenseFiles[LICENSE_BOOST] = "boost.txt";
  m_LicenseFiles[LICENSE_CCBY3] = "by-sa3.txt";
  m_LicenseFiles[LICENSE_ZLIB] = "zlib.txt";
  m_LicenseFiles[LICENSE_APACHE2] = "apache-license-2.0.txt";

  addLicense("Qt", LICENSE_LGPL3);
  addLicense("Qt Json", LICENSE_GPL3);
  addLicense("Boost Library", LICENSE_BOOST);
  addLicense("7-zip", LICENSE_LGPL3);
  addLicense("ZLib", LICENSE_ZLIB);
  addLicense("Tango Icon Theme", LICENSE_NONE);
  addLicense("RRZE Icon Set", LICENSE_CCBY3);
  addLicense("Icons by Lorc, Delapouite and sbed available on http://game-icons.net", LICENSE_CCBY3);
  addLicense("Castle Core", LICENSE_APACHE2);

  ui->nameLabel->setText(QString("<span style=\"font-size:12pt; font-weight:600;\">%1 %2</span>").arg(ui->nameLabel->text()).arg(version));
#ifdef HGID
  ui->revisionLabel->setText(ui->revisionLabel->text() + " " + HGID);
#else
  ui->revisionLabel->setText(ui->revisionLabel->text() + " unknown");
#endif
}


AboutDialog::~AboutDialog()
{
  delete ui;
}


void AboutDialog::addLicense(const QString &name, Licenses license)
{
  QListWidgetItem *item = new QListWidgetItem(name);
  item->setData(Qt::UserRole, license);
  ui->creditsList->addItem(item);
}


void AboutDialog::on_creditsList_currentItemChanged(QListWidgetItem *current, QListWidgetItem*)
{
  auto iter = m_LicenseFiles.find(current->data(Qt::UserRole).toInt());
  if (iter != m_LicenseFiles.end()) {
    QString filePath = qApp->applicationDirPath() + "/license/" + iter->second;
    QString text = MOBase::readFileText(filePath);
    ui->licenseText->setText(text);
  } else {
    ui->licenseText->setText(tr("No license"));
  }
}
