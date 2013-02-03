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

#include "questionboxmemory.h"
#include "ui_questionboxmemory.h"

#include <QMessageBox>
#include <QPushButton>

QuestionBoxMemory::QuestionBoxMemory(QWidget *parent, const QString &title, const QString &text,
                                     const QDialogButtonBox::StandardButtons buttons, QDialogButtonBox::StandardButton defaultButton)
  : QDialog(parent), ui(new Ui::QuestionBoxMemory)
{
  ui->setupUi(this);

  this->setWindowTitle(title);

  QIcon icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxQuestion);
  ui->iconLabel->setPixmap(icon.pixmap(128));
  ui->messageLabel->setText(text);
  ui->buttonBox->setStandardButtons(buttons);
  if (defaultButton != QDialogButtonBox::NoButton) {
    ui->buttonBox->button(defaultButton)->setDefault(true);
  }
  connect(ui->buttonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(buttonClicked(QAbstractButton*)));
}


QuestionBoxMemory::~QuestionBoxMemory()
{
  delete ui;
}


void QuestionBoxMemory::buttonClicked(QAbstractButton *button)
{
  m_Button = ui->buttonBox->standardButton(button);
}


QDialogButtonBox::StandardButton QuestionBoxMemory::query(QWidget *parent,
    QSettings &settings, const QString &name,
    const QString &title, const QString &text, QDialogButtonBox::StandardButtons buttons,
    QDialogButtonBox::StandardButton defaultButton)
{
  if (settings.contains(QString("DialogChoices/") + name)) {
    return static_cast<QDialogButtonBox::StandardButton>(settings.value(QString("DialogChoices/") + name).toInt());
  } else {
    QuestionBoxMemory dialog(parent, title, text, buttons, defaultButton);
    dialog.exec();
    if (dialog.ui->rememberCheckBox->isChecked()) {
      settings.setValue(QString("DialogChoices/") + name, dialog.m_Button);
    }
    return dialog.m_Button;
  }
}
