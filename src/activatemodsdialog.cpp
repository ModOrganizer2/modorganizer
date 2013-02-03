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

#include "activatemodsdialog.h"
#include "ui_activatemodsdialog.h"

#include <QComboBox>
#include <QLabel>

ActivateModsDialog::ActivateModsDialog(const std::map<QString, std::vector<QString> > &missingPlugins, QWidget *parent)
  : TutorableDialog("ActivateMods", parent), ui(new Ui::ActivateModsDialog)
{
  ui->setupUi(this);

  QTableWidget *modsTable = findChild<QTableWidget*>("modsTable");
  QHeaderView *headerView = modsTable->horizontalHeader();
#if QT_VERSION >= 0x050000
  headerView->setSectionResizeMode(0, QHeaderView::Stretch);
  headerView->setSectionResizeMode(1, QHeaderView::Interactive);
#else
  headerView->setResizeMode(0, QHeaderView::Stretch);
  headerView->setResizeMode(1, QHeaderView::Interactive);
#endif

  int row = 0;

  modsTable->setRowCount(missingPlugins.size());

  for (std::map<QString, std::vector<QString> >::const_iterator espIter = missingPlugins.begin();
       espIter != missingPlugins.end(); ++espIter, ++row) {
    modsTable->setCellWidget(row, 0, new QLabel(espIter->first));
    if (espIter->second.size() == 0) {
      modsTable->setCellWidget(row, 1, new QLabel(tr("not found")));
    } else {
      QComboBox* combo = new QComboBox();
      for (std::vector<QString>::const_iterator modIter = espIter->second.begin();
           modIter != espIter->second.end(); ++modIter) {
        combo->addItem(*modIter);
      }
      modsTable->setCellWidget(row, 1, combo);
    }
  }
}


ActivateModsDialog::~ActivateModsDialog()
{
  delete ui;
}


void ActivateModsDialog::on_buttonBox_accepted()
{
}


std::set<QString> ActivateModsDialog::getModsToActivate()
{
  std::set<QString> result;
  QTableWidget *modsTable = findChild<QTableWidget*>("modsTable");

  for (int row = 0; row < modsTable->rowCount(); ++row) {
    QComboBox *comboBox = dynamic_cast<QComboBox*>(modsTable->cellWidget(row, 1));
    if (comboBox != NULL) {
      result.insert(comboBox->currentText());
    }
  }
  return result;
}


std::set<QString> ActivateModsDialog::getESPsToActivate()
{
  std::set<QString> result;
  QTableWidget *modsTable = findChild<QTableWidget*>("modsTable");

  for (int row = 0; row < modsTable->rowCount(); ++row) {
    QComboBox *comboBox = dynamic_cast<QComboBox*>(modsTable->cellWidget(row, 1));
    if (comboBox != NULL) {
      QLabel *espName = dynamic_cast<QLabel*>(modsTable->cellWidget(row, 0));

      result.insert(espName->text());
    }
  }
  return result;
}
