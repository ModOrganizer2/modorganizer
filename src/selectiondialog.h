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

#ifndef SELECTIONDIALOG_H
#define SELECTIONDIALOG_H

#include <QAbstractButton>
#include <QDialog>

namespace Ui
{
class SelectionDialog;
}

class SelectionDialog : public QDialog
{
  Q_OBJECT

public:
  explicit SelectionDialog(const QString& description, QWidget* parent = 0,
                           const QSize& iconSize = QSize());

  ~SelectionDialog();

  /**
   * @brief add a choice to the dialog
   * @param buttonText the text to be displayed on the button
   * @param description the description that shows up under in small letters inside the
   * button
   * @param data data to be stored with the button. Please note that as soon as one
   * choice has data associated with it (non-invalid QVariant) all buttons that contain
   * no data will be treated as "cancel" buttons
   */
  void addChoice(const QString& buttonText, const QString& description,
                 const QVariant& data);

  void addChoice(const QIcon& icon, const QString& buttonText,
                 const QString& description, const QVariant& data);

  int numChoices() const;

  QVariant getChoiceData();
  QString getChoiceString();
  QString getChoiceDescription();

  void disableCancel();

private slots:

  void on_buttonBox_clicked(QAbstractButton* button);

  void on_cancelButton_clicked();

private:
  Ui::SelectionDialog* ui;
  QAbstractButton* m_Choice;
  bool m_ValidateByData;
  QSize m_IconSize;
};

#endif  // SELECTIONDIALOG_H
