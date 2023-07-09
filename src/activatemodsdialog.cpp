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
#include <QHeaderView>
#include <QLabel>
#include <QString>
#include <QTableWidget>

#include <QtGlobal>

ActivateModsDialog::ActivateModsDialog(SaveGameInfo::MissingAssets const& missingAssets,
                                       QWidget* parent)
    : TutorableDialog("ActivateMods", parent), ui(new Ui::ActivateModsDialog)
{
  ui->setupUi(this);

  QTableWidget* modsTable = findChild<QTableWidget*>("modsTable");
  QHeaderView* headerView = modsTable->horizontalHeader();
  headerView->setSectionResizeMode(0, QHeaderView::Stretch);
  headerView->setSectionResizeMode(1, QHeaderView::Interactive);

  int row = 0;

  modsTable->setRowCount(missingAssets.size());

  for (SaveGameInfo::MissingAssets::const_iterator espIter = missingAssets.begin();
       espIter != missingAssets.end(); ++espIter, ++row) {
    modsTable->setCellWidget(row, 0, new QLabel(espIter.key()));
    if (espIter->size() == 0) {
      modsTable->setCellWidget(row, 1, new QLabel(tr("not found")));
    } else {
      QComboBox* combo = new QComboBox();
      for (QString const& mod : espIter.value()) {
        combo->addItem(mod);
      }
      modsTable->setCellWidget(row, 1, combo);
    }
  }
}

ActivateModsDialog::~ActivateModsDialog()
{
  delete ui;
}

std::set<QString> ActivateModsDialog::getModsToActivate()
{
  std::set<QString> result;
  QTableWidget* modsTable = findChild<QTableWidget*>("modsTable");

  for (int row = 0; row < modsTable->rowCount(); ++row) {
    QComboBox* comboBox = dynamic_cast<QComboBox*>(modsTable->cellWidget(row, 1));
    if (comboBox != nullptr) {
      result.insert(comboBox->currentText());
    }
  }
  return result;
}

std::set<QString> ActivateModsDialog::getESPsToActivate()
{
  std::set<QString> result;
  QTableWidget* modsTable = findChild<QTableWidget*>("modsTable");

  for (int row = 0; row < modsTable->rowCount(); ++row) {
    QComboBox* comboBox = dynamic_cast<QComboBox*>(modsTable->cellWidget(row, 1));
    if (comboBox != nullptr) {
      QLabel* espName = dynamic_cast<QLabel*>(modsTable->cellWidget(row, 0));

      result.insert(espName->text());
    }
  }
  return result;
}
