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

#include "editexecutablesdialog.h"
#include "ui_editexecutablesdialog.h"
#include "filedialogmemory.h"
#include <QMessageBox>


EditExecutablesDialog::EditExecutablesDialog(const ExecutablesList &executablesList, QWidget *parent)
  : TutorableDialog("EditExecutables", parent),
  ui(new Ui::EditExecutablesDialog), m_ExecutablesList(executablesList)
{
  ui->setupUi(this);

  refreshExecutablesWidget();
}

EditExecutablesDialog::~EditExecutablesDialog()
{
  delete ui;
}

ExecutablesList EditExecutablesDialog::getExecutablesList() const
{
  ExecutablesList newList;
  for (int i = 0; i < ui->executablesListBox->count(); ++i) {
    newList.addExecutable(ui->executablesListBox->item(i)->data(Qt::UserRole).value<Executable>());
  }
  return newList;
}

void EditExecutablesDialog::refreshExecutablesWidget()
{
  QListWidget *executablesWidget = findChild<QListWidget*>("executablesListBox");

  executablesWidget->clear();
  std::vector<Executable>::const_iterator current, end;
  m_ExecutablesList.getExecutables(current, end);

  for(; current != end; ++current) {
    QListWidgetItem *newItem = new QListWidgetItem(current->m_Title);
    QVariant temp;
    temp.setValue(*current);
    newItem->setData(Qt::UserRole, temp);
    newItem->setTextColor(current->m_Custom ? QColor(Qt::black) : QColor(Qt::darkGray));
    executablesWidget->addItem(newItem);
  }

  QPushButton *addButton = findChild<QPushButton*>("addButton");
  QPushButton *removeButton = findChild<QPushButton*>("removeButton");

  addButton->setEnabled(false);
  removeButton->setEnabled(false);
}


void EditExecutablesDialog::on_binaryEdit_textChanged(const QString &arg1)
{
  QPushButton *addButton = findChild<QPushButton*>("addButton");
//  QPushButton *removeButton = findChild<QPushButton*>("removeButton");

  QFileInfo fileInfo(arg1);
  addButton->setEnabled(fileInfo.exists() && fileInfo.isFile());
}

void EditExecutablesDialog::resetInput()
{
  ui->binaryEdit->setText("");
  ui->titleEdit->setText("");
  ui->argumentsEdit->setText("");
  ui->closeCheckBox->setChecked(false);
}


void EditExecutablesDialog::on_addButton_clicked()
{
  QLineEdit *titleEdit = findChild<QLineEdit*>("titleEdit");
  QLineEdit *binaryEdit = findChild<QLineEdit*>("binaryEdit");
  QLineEdit *argumentsEdit = findChild<QLineEdit*>("argumentsEdit");
  QLineEdit *workingDirEdit = findChild<QLineEdit*>("workingDirEdit");
  QCheckBox *closeCheckBox = findChild<QCheckBox*>("closeCheckBox");

  m_ExecutablesList.addExecutable(titleEdit->text(), QDir::fromNativeSeparators(binaryEdit->text()),
        argumentsEdit->text(), QDir::fromNativeSeparators(workingDirEdit->text()),
        (closeCheckBox->checkState() == Qt::Checked) ? DEFAULT_CLOSE : DEFAULT_STAY,
        ui->overwriteAppIDBox->isChecked() ? ui->appIDOverwriteEdit->text() : "");

  resetInput();
  refreshExecutablesWidget();
}

void EditExecutablesDialog::on_browseButton_clicked()
{
  QString binaryName = FileDialogMemory::getOpenFileName("editExecutableBinary", this,
            tr("Select a binary"), QString(), tr("Executable (%1)").arg("*.exe *.bat"));

  QLineEdit *binaryEdit = findChild<QLineEdit*>("binaryEdit");

  binaryEdit->setText(QDir::toNativeSeparators(binaryName));
}

void EditExecutablesDialog::on_browseDirButton_clicked()
{
  QString dirName = FileDialogMemory::getExistingDirectory("editExecutableDirectory", this,
                                                           tr("Select a directory"));

  ui->workingDirEdit->setText(dirName);
}

void EditExecutablesDialog::on_removeButton_clicked()
{
//  QLineEdit *binaryEdit = findChild<QLineEdit*>("binaryEdit");

  if (QMessageBox::question(this, tr("Confirm"), tr("Really remove \"%1\" from executables?").arg(ui->titleEdit->text()),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    m_ExecutablesList.remove(ui->titleEdit->text());
  }

  resetInput();
  refreshExecutablesWidget();
}

void EditExecutablesDialog::on_titleEdit_textChanged(const QString &arg1)
{
  QPushButton *addButton = findChild<QPushButton*>("addButton");
  QPushButton *removeButton = findChild<QPushButton*>("removeButton");

  QListWidget *executablesWidget = findChild<QListWidget*>("executablesListBox");

  QList<QListWidgetItem*> existingItems = executablesWidget->findItems(arg1, Qt::MatchFixedString);

  addButton->setEnabled(arg1.length() != 0);

  if (existingItems.count() == 0) {
    addButton->setText(tr("Add"));
    removeButton->setEnabled(false);
  } else {
    // existing item. is it a custom one?
    addButton->setText(tr("Modify"));
    removeButton->setEnabled(true);
  }
}

void EditExecutablesDialog::on_executablesListBox_itemClicked(QListWidgetItem *item)
{
  QLineEdit *titleEdit = findChild<QLineEdit*>("titleEdit");
  QLineEdit *binaryEdit = findChild<QLineEdit*>("binaryEdit");
  QLineEdit *argumentsEdit = findChild<QLineEdit*>("argumentsEdit");
  QLineEdit *workingDirEdit = findChild<QLineEdit*>("workingDirEdit");
  QPushButton *removeButton = findChild<QPushButton*>("removeButton");
  QCheckBox *closeCheckBox = findChild<QCheckBox*>("closeCheckBox");

  const Executable &selectedExecutable = item->data(Qt::UserRole).value<Executable>();

  titleEdit->setText(selectedExecutable.m_Title);
  binaryEdit->setText(QDir::toNativeSeparators(selectedExecutable.m_BinaryInfo.absoluteFilePath()));
  argumentsEdit->setText(selectedExecutable.m_Arguments);
  workingDirEdit->setText(QDir::toNativeSeparators(selectedExecutable.m_WorkingDirectory));
  closeCheckBox->setChecked(selectedExecutable.m_CloseMO == DEFAULT_CLOSE);
  if (selectedExecutable.m_CloseMO == NEVER_CLOSE) {
    closeCheckBox->setEnabled(false);
    closeCheckBox->setToolTip(tr("MO must be kept running or this application will not work correctly."));
  } else {
    closeCheckBox->setEnabled(true);
    closeCheckBox->setToolTip(tr("If checked, MO will be closed once the specified executable is run."));
  }
  removeButton->setEnabled(selectedExecutable.m_Custom);
  ui->overwriteAppIDBox->setChecked(selectedExecutable.m_SteamAppID != 0);
  if (selectedExecutable.m_SteamAppID != 0) {
    ui->appIDOverwriteEdit->setText(selectedExecutable.m_SteamAppID);
  }
}

void EditExecutablesDialog::on_overwriteAppIDBox_toggled(bool checked)
{
  ui->appIDOverwriteEdit->setEnabled(checked);
}
