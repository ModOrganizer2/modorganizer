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

#ifndef QUESTIONBOXMEMORY_H
#define QUESTIONBOXMEMORY_H

#include <QDialog>
#include <QSettings>
#include <QDialogButtonBox>


namespace Ui {
  class QuestionBoxMemory;
}


class QuestionBoxMemory : public QDialog
{
  Q_OBJECT
  
public:

  virtual ~QuestionBoxMemory();
  static QDialogButtonBox::StandardButton query(QWidget *parent,
                                                QSettings &settings, const QString &name,
                                                const QString &title, const QString &text,
                                                QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Yes | QDialogButtonBox::No,
                                                QDialogButtonBox::StandardButton defaultButton = QDialogButtonBox::NoButton);
private slots:

  void buttonClicked(QAbstractButton *button);

private:

  explicit QuestionBoxMemory(QWidget *parent, const QString &title, const QString &text, const QDialogButtonBox::StandardButtons buttons,
                             QDialogButtonBox::StandardButton defaultButton);

private:

  Ui::QuestionBoxMemory *ui;
  QDialogButtonBox::StandardButton m_Button;

};

#endif // QUESTIONBOXMEMORY_H


