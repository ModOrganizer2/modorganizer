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

#include "selectiondialog.h"
#include "ui_selectiondialog.h"

#include <QCommandLinkButton>

SelectionDialog::SelectionDialog(const QString& description, QWidget* parent,
                                 const QSize& iconSize)
    : QDialog(parent), ui(new Ui::SelectionDialog), m_Choice(nullptr),
      m_ValidateByData(false), m_IconSize(iconSize)
{
  ui->setupUi(this);

  ui->descriptionLabel->setText(description);
}

SelectionDialog::~SelectionDialog()
{
  delete ui;
}

void SelectionDialog::addChoice(const QString& buttonText, const QString& description,
                                const QVariant& data)
{
  QAbstractButton* button =
      new QCommandLinkButton(buttonText, description, ui->buttonBox);
  if (m_IconSize.isValid()) {
    button->setIconSize(m_IconSize);
  }
  button->setProperty("data", data);
  ui->buttonBox->addButton(button, QDialogButtonBox::AcceptRole);
  if (data.isValid())
    m_ValidateByData = true;
}

void SelectionDialog::addChoice(const QIcon& icon, const QString& buttonText,
                                const QString& description, const QVariant& data)
{
  QAbstractButton* button =
      new QCommandLinkButton(buttonText, description, ui->buttonBox);
  if (m_IconSize.isValid()) {
    button->setIconSize(m_IconSize);
  }
  button->setIcon(icon);
  button->setProperty("data", data);
  ui->buttonBox->addButton(button, QDialogButtonBox::AcceptRole);
  if (data.isValid())
    m_ValidateByData = true;
}

int SelectionDialog::numChoices() const
{
  return ui->buttonBox->findChildren<QCommandLinkButton*>(QString()).count();
}

QVariant SelectionDialog::getChoiceData()
{
  return m_Choice->property("data");
}

QString SelectionDialog::getChoiceString()
{
  if ((m_Choice == nullptr) ||
      (m_ValidateByData && !m_Choice->property("data").isValid())) {
    return QString();
  } else {
    return m_Choice->text();
  }
}

QString SelectionDialog::getChoiceDescription()
{
  if (m_Choice == nullptr)
    return QString();
  else
    return m_Choice->accessibleDescription();
}

void SelectionDialog::disableCancel()
{
  ui->cancelButton->setEnabled(false);
  ui->cancelButton->setHidden(true);
}

void SelectionDialog::on_buttonBox_clicked(QAbstractButton* button)
{
  m_Choice = button;
  if (!m_ValidateByData || m_Choice->property("data").isValid()) {
    this->accept();
  } else {
    this->reject();
  }
}

void SelectionDialog::on_cancelButton_clicked()
{
  this->reject();
}
