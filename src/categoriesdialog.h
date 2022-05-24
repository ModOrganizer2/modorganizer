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

#ifndef CATEGORIESDIALOG_H
#define CATEGORIESDIALOG_H

#include "categories.h"
#include "tutorabledialog.h"
#include <set>

namespace Ui
{
class CategoriesDialog;
}

/**
 * @brief Dialog that allows users to configure mod categories
 **/
class CategoriesDialog : public MOBase::TutorableDialog
{
  Q_OBJECT

public:
  explicit CategoriesDialog(QWidget* parent = 0);
  ~CategoriesDialog();

  // also saves and restores geometry
  //
  int exec() override;

  /**
   * @brief store changes here to the global categories store (categories.h)
   *
   **/
  void commitChanges();

public slots:
  void nxmGameInfoAvailable(QString gameName, QVariant, QVariant resultData, int);
  void nxmRequestFailed(QString, int, int, QVariant, int, int errorCode,
                        const QString& errorMessage);

private slots:
  void on_categoriesTable_customContextMenuRequested(const QPoint& pos);
  void addCategory_clicked();
  void removeCategory_clicked();
  void removeNexusMap_clicked();
  void nexusRefresh_clicked();
  void nexusImport_clicked();
  void cellChanged(int row, int column);

private:
  void refreshIDs();
  void fillTable();

private:
  Ui::CategoriesDialog* ui;
  int m_ContextRow;

  int m_HighestID;
  std::set<int> m_IDs;
  std::vector<CategoryFactory::NexusCategory> m_NexusCategories;
};

#endif  // CATEGORIESDIALOG_H
