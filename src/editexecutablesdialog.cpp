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
#include "stackdata.h"
#include <QMessageBox>
#include <Shellapi.h>
#include <utility.h>


using namespace MOBase;
using namespace MOShared;

EditExecutablesDialog::EditExecutablesDialog(const ExecutablesList &executablesList, QWidget *parent)
  : TutorableDialog("EditExecutables", parent)
  , ui(new Ui::EditExecutablesDialog)
  , m_CurrentItem(nullptr)
  , m_ExecutablesList(executablesList)
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
    newList.addExecutable(m_ExecutablesList.find(ui->executablesListBox->item(i)->text()));
  }
  return newList;
}

void EditExecutablesDialog::refreshExecutablesWidget()
{
  ui->executablesListBox->clear();
  std::vector<Executable>::const_iterator current, end;
  m_ExecutablesList.getExecutables(current, end);

  for(; current != end; ++current) {
    QListWidgetItem *newItem = new QListWidgetItem(current->m_Title);
    newItem->setTextColor(current->isCustom() ? QColor(Qt::black) : QColor(Qt::darkGray));
    ui->executablesListBox->addItem(newItem);
  }

  ui->addButton->setEnabled(false);
  ui->removeButton->setEnabled(false);
}


void EditExecutablesDialog::on_binaryEdit_textChanged(const QString &name)
{
  QFileInfo fileInfo(name);
  ui->addButton->setEnabled(fileInfo.exists() && fileInfo.isFile());
}

void EditExecutablesDialog::resetInput()
{
  ui->binaryEdit->setText("");
  ui->titleEdit->setText("");
  ui->workingDirEdit->clear();
  ui->argumentsEdit->setText("");
  ui->appIDOverwriteEdit->clear();
  ui->overwriteAppIDBox->setChecked(false);
  ui->closeCheckBox->setChecked(false);
  ui->useAppIconCheckBox->setChecked(false);
  m_CurrentItem = nullptr;
}


void EditExecutablesDialog::saveExecutable()
{
  m_ExecutablesList.updateExecutable(ui->titleEdit->text(),
                                     QDir::fromNativeSeparators(ui->binaryEdit->text()),
                                     ui->argumentsEdit->text(),
                                     QDir::fromNativeSeparators(ui->workingDirEdit->text()),
                                     (ui->closeCheckBox->checkState() == Qt::Checked) ?
                                       ExecutableInfo::CloseMOStyle::DEFAULT_CLOSE
                                     : ExecutableInfo::CloseMOStyle::DEFAULT_STAY,
                                     ui->overwriteAppIDBox->isChecked() ?
                                       ui->appIDOverwriteEdit->text() : "",
                                     Executable::UseApplicationIcon | Executable::CustomExecutable,
                                     (ui->useAppIconCheckBox->isChecked() ?
                                       Executable::UseApplicationIcon : Executable::Flags())
                                     | Executable::CustomExecutable);
  }


void EditExecutablesDialog::delayedRefresh()
{
  int index = ui->executablesListBox->currentIndex().row();
  resetInput();
  refreshExecutablesWidget();
  ui->executablesListBox->setCurrentRow(index);
}


void EditExecutablesDialog::on_addButton_clicked()
{
  if (executableChanged()) {
    saveExecutable();
  }

  resetInput();
  refreshExecutablesWidget();
}

void EditExecutablesDialog::on_browseButton_clicked()
{
  QString binaryName = FileDialogMemory::getOpenFileName("editExecutableBinary", this,
            tr("Select a binary"), QString(), tr("Executable (%1)").arg("*.exe *.bat *.jar"));

  if (binaryName.endsWith(".jar", Qt::CaseInsensitive)) {
    QString binaryPath;
    { // try to find java automatically
      std::wstring binaryNameW = ToWString(binaryName);
      WCHAR buffer[MAX_PATH];
      if (::FindExecutableW(binaryNameW.c_str(), nullptr, buffer) > (HINSTANCE)32) {
        DWORD binaryType = 0UL;
        if (!::GetBinaryTypeW(binaryNameW.c_str(), &binaryType)) {
          qDebug("failed to determine binary type of \"%ls\": %lu", binaryNameW.c_str(), ::GetLastError());
        } else if (binaryType == SCS_32BIT_BINARY) {
          binaryPath = ToQString(buffer);
        }
      }
    }
    if (binaryPath.isEmpty()) {
      QSettings javaReg("HKEY_LOCAL_MACHINE\\Software\\JavaSoft\\Java Runtime Environment", QSettings::NativeFormat);
      if (javaReg.contains("CurrentVersion")) {
        QString currentVersion = javaReg.value("CurrentVersion").toString();
        binaryPath = javaReg.value(QString("%1/JavaHome").arg(currentVersion)).toString().append("\\bin\\javaw.exe");
      }
    }
    if (binaryPath.isEmpty()) {
      QMessageBox::information(this, tr("Java (32-bit) required"),
                               tr("MO requires 32-bit java to run this application. If you already have it installed, select javaw.exe "
                                  "from that installation as the binary."));
    } else {
      ui->binaryEdit->setText(binaryPath);
    }

    ui->workingDirEdit->setText(QDir::toNativeSeparators(QFileInfo(binaryName).absolutePath()));
    ui->argumentsEdit->setText("-jar \"" + QDir::toNativeSeparators(binaryName) + "\"");
  } else {
    ui->binaryEdit->setText(QDir::toNativeSeparators(binaryName));
  }
}

void EditExecutablesDialog::on_browseDirButton_clicked()
{
  QString dirName = FileDialogMemory::getExistingDirectory("editExecutableDirectory", this,
                                                           tr("Select a directory"));

  ui->workingDirEdit->setText(dirName);
}

void EditExecutablesDialog::on_removeButton_clicked()
{
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


bool EditExecutablesDialog::executableChanged()
{
  if (m_CurrentItem != nullptr) {
    Executable const &selectedExecutable(m_ExecutablesList.find(m_CurrentItem->text()));
    return selectedExecutable.m_Arguments != ui->argumentsEdit->text()
        || selectedExecutable.m_SteamAppID != ui->appIDOverwriteEdit->text()
        || selectedExecutable.m_WorkingDirectory != QDir::fromNativeSeparators(ui->workingDirEdit->text())
        || selectedExecutable.m_BinaryInfo.absoluteFilePath() != QDir::fromNativeSeparators(ui->binaryEdit->text())
        || (selectedExecutable.m_CloseMO == ExecutableInfo::CloseMOStyle::DEFAULT_CLOSE) != ui->closeCheckBox->isChecked()
        || selectedExecutable.usesOwnIcon() != ui->useAppIconCheckBox->isChecked();
  } else {
    QFileInfo fileInfo(ui->binaryEdit->text());
    return !ui->binaryEdit->text().isEmpty()
        && !ui->titleEdit->text().isEmpty()
        && fileInfo.exists()
        && fileInfo.isFile();
  }
}

void EditExecutablesDialog::on_executablesListBox_itemSelectionChanged()
{
  if (ui->executablesListBox->selectedItems().size() == 0) {
    // deselected
    resetInput();
  }
}

void EditExecutablesDialog::on_overwriteAppIDBox_toggled(bool checked)
{
  ui->appIDOverwriteEdit->setEnabled(checked);
}

void EditExecutablesDialog::on_closeButton_clicked()
{
  if (executableChanged()) {
    QMessageBox::StandardButton res = QMessageBox::question(this, tr("Save Changes?"),
        tr("You made changes to the current executable, do you want to save them?"),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (res == QMessageBox::Cancel) {
      return;
    } else if (res == QMessageBox::Yes) {
      saveExecutable();
      // the executable list returned to callers is generated from the user data in the widgets,
      // NOT the list we just saved
      refreshExecutablesWidget();
    }
  }
  this->accept();
}

void EditExecutablesDialog::on_executablesListBox_clicked(const QModelIndex &current)
{
  if (current.isValid()) {
    ui->executablesListBox->selectionModel()->clearSelection();
    ui->executablesListBox->selectionModel()->select(current, QItemSelectionModel::SelectCurrent);

    if (executableChanged()) {
      QMessageBox::StandardButton res = QMessageBox::question(this, tr("Save Changes?"),
                                                              tr("You made changes to the current executable, do you want to save them?"),
                                                              QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
      if (res == QMessageBox::Cancel) {
        return;
      } else if (res == QMessageBox::Yes) {
        // this invalidates the item passed as a a parameter
        saveExecutable();

        QTimer::singleShot(50, this, SLOT(delayedRefresh()));
        return;
      }
    }

    m_CurrentItem = ui->executablesListBox->item(current.row());

    Executable const &selectedExecutable(m_ExecutablesList.find(m_CurrentItem->text()));

    ui->titleEdit->setText(selectedExecutable.m_Title);
    ui->binaryEdit->setText(QDir::toNativeSeparators(selectedExecutable.m_BinaryInfo.absoluteFilePath()));
    ui->argumentsEdit->setText(selectedExecutable.m_Arguments);
    ui->workingDirEdit->setText(QDir::toNativeSeparators(selectedExecutable.m_WorkingDirectory));
    ui->closeCheckBox->setChecked(selectedExecutable.m_CloseMO == ExecutableInfo::CloseMOStyle::DEFAULT_CLOSE);
    if (selectedExecutable.m_CloseMO == ExecutableInfo::CloseMOStyle::NEVER_CLOSE) {
      ui->closeCheckBox->setEnabled(false);
      ui->closeCheckBox->setToolTip(tr("MO must be kept running or this application will not work correctly."));
    } else {
      ui->closeCheckBox->setEnabled(true);
      ui->closeCheckBox->setToolTip(tr("If checked, MO will be closed once the specified executable is run."));
    }
    ui->removeButton->setEnabled(selectedExecutable.isCustom());
    ui->overwriteAppIDBox->setChecked(!selectedExecutable.m_SteamAppID.isEmpty());
    if (!selectedExecutable.m_SteamAppID.isEmpty()) {
      ui->appIDOverwriteEdit->setText(selectedExecutable.m_SteamAppID);
    } else {
      ui->appIDOverwriteEdit->clear();
    }
    ui->useAppIconCheckBox->setChecked(selectedExecutable.usesOwnIcon());
  }
}
