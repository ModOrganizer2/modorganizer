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
#include "modlist.h"
#include <QMessageBox>
#include <Shellapi.h>
#include <utility.h>
#include "forcedloaddialog.h"
#include <algorithm>

using namespace MOBase;
using namespace MOShared;

EditExecutablesDialog::EditExecutablesDialog(
    const ExecutablesList &executablesList, const ModList &modList,
    Profile *profile, const IPluginGame *game, QWidget *parent)
  : TutorableDialog("EditExecutables", parent)
  , ui(new Ui::EditExecutablesDialog)
  , m_currentItem(nullptr)
  , m_executablesList(executablesList)
  , m_profile(profile)
  , m_gamePlugin(game)
  , m_dirty(false)
{
  ui->setupUi(this);
  ui->splitter->setSizes({200, 1});
  ui->splitter->setStretchFactor(0, 0);
  ui->splitter->setStretchFactor(1, 1);

  refreshExecutablesWidget();
  ui->newFilesModBox->addItems(modList.allMods());

  m_forcedLibraries = m_profile->determineForcedLibraries(ui->titleEdit->text());

  updateUI(nullptr);
}

EditExecutablesDialog::~EditExecutablesDialog() = default;

void EditExecutablesDialog::updateUI(const Executable* e)
{
  if (e) {
    setEdits(*e);
    ui->removeButton->setEnabled(e->isCustom());
  } else {
    clearEdits();
    ui->removeButton->setEnabled(false);
  }
}

void EditExecutablesDialog::clearEdits()
{
  ui->titleEdit->clear();
  ui->binaryEdit->clear();
  ui->workingDirEdit->clear();
  ui->argumentsEdit->clear();
  ui->overwriteAppIDBox->setChecked(false);
  ui->appIDOverwriteEdit->clear();
  ui->newFilesModCheckBox->setChecked(false);
  ui->newFilesModBox->setCurrentIndex(-1);
  ui->forceLoadCheckBox->setChecked(false);
  ui->useAppIconCheckBox->setChecked(false);

  ui->pluginProvidedLabel->setVisible(false);
}

void EditExecutablesDialog::setEdits(const Executable& e)
{
  ui->titleEdit->setText(e.title());
  ui->binaryEdit->setText(QDir::toNativeSeparators(e.binaryInfo().absoluteFilePath()));
  ui->workingDirEdit->setText(QDir::toNativeSeparators(e.workingDirectory()));
  ui->argumentsEdit->setText(e.arguments());
  ui->overwriteAppIDBox->setChecked(!e.steamAppID().isEmpty());
  ui->appIDOverwriteEdit->setText(e.steamAppID());
  ui->useAppIconCheckBox->setChecked(e.usesOwnIcon());

  int modIndex = -1;

  QString customOverwrite = m_profile->setting("custom_overwrites", e.title()).toString();
  if (!customOverwrite.isEmpty()) {
    modIndex = ui->newFilesModBox->findText(customOverwrite);
  }

  ui->newFilesModCheckBox->setChecked(modIndex != -1);
  ui->newFilesModBox->setCurrentIndex(modIndex);

  const bool forcedLibraries = m_profile->forcedLibrariesEnabled(e.title());
  ui->forceLoadCheckBox->setChecked(forcedLibraries);
  ui->forceLoadButton->setEnabled(forcedLibraries);

  ui->pluginProvidedLabel->setVisible(!e.isCustom());

  // only enabled for custom executables
  ui->titleEdit->setEnabled(e.isCustom());
  ui->binaryEdit->setEnabled(e.isCustom());
  ui->browseBinaryButton->setEnabled(e.isCustom());
  ui->workingDirEdit->setEnabled(e.isCustom());
  ui->browseWorkingDirButton->setEnabled(e.isCustom());
  ui->argumentsEdit->setEnabled(e.isCustom());
  ui->overwriteAppIDBox->setEnabled(e.isCustom());
  ui->appIDOverwriteEdit->setEnabled(e.isCustom());
  ui->useAppIconCheckBox->setEnabled(e.isCustom());

  // always enabled
  ui->newFilesModCheckBox->setEnabled(true);
  ui->newFilesModBox->setEnabled(true);
  ui->forceLoadCheckBox->setEnabled(true);
}





void EditExecutablesDialog::resetInput()
{
  ui->binaryEdit->setText("");
  ui->titleEdit->setText("");
  ui->workingDirEdit->clear();
  ui->argumentsEdit->setText("");
  ui->appIDOverwriteEdit->clear();
  ui->overwriteAppIDBox->setChecked(false);
  ui->useAppIconCheckBox->setChecked(false);
  ui->newFilesModCheckBox->setChecked(false);
  ui->forceLoadCheckBox->setChecked(false);
  m_currentItem = nullptr;
}

ExecutablesList EditExecutablesDialog::getExecutablesList() const
{
  ExecutablesList newList;
  for (int i = 0; i < ui->executablesListBox->count(); ++i) {
    const auto& title = ui->executablesListBox->item(i)->text();
    auto itor = m_executablesList.find(title);

    if (itor == m_executablesList.end()) {
      qWarning().nospace()
        << "getExecutablesList(): executable '" << title << "' not found";

      continue;
    }

    newList.setExecutable(*itor);
  }

  return newList;
}

void EditExecutablesDialog::refreshExecutablesWidget()
{
  ui->executablesListBox->clear();

  for(const auto& exe : m_executablesList) {
    QListWidgetItem *newItem = new QListWidgetItem(exe.title());

    if (!exe.isCustom()) {
      auto f = newItem->font();
      f.setItalic(true);

      newItem->setFont(f);
    }

    ui->executablesListBox->addItem(newItem);
  }

  //ui->addButton->setEnabled(false);
  //ui->removeButton->setEnabled(false);
}


void EditExecutablesDialog::on_binaryEdit_textChanged(const QString &name)
{
  updateButtonStates();
}

void EditExecutablesDialog::on_workingDirEdit_textChanged(const QString &dir)
{
  updateButtonStates();
}

void EditExecutablesDialog::updateButtonStates()
{
  bool enabled = true;

  QString filePath(ui->binaryEdit->text());
  QFileInfo fileInfo(filePath);
  if (!fileInfo.exists())
    enabled = false;
  if (!fileInfo.isFile())
    enabled = false;

  QString dirPath(ui->workingDirEdit->text());
  if (!dirPath.isEmpty()) {
    QDir dirInfo(dirPath);
    if (!dirInfo.exists())
      enabled = false;
  }

  //ui->addButton->setEnabled(enabled);
}


void EditExecutablesDialog::saveExecutable()
{
  Executable::Flags flags = Executable::CustomExecutable;
  if (ui->useAppIconCheckBox->isChecked())
    flags |= Executable::UseApplicationIcon;

  m_executablesList.setExecutable(Executable()
    .title(ui->titleEdit->text())
    .binaryInfo(QDir::fromNativeSeparators(ui->binaryEdit->text()))
    .arguments(ui->argumentsEdit->text())
    .steamAppID(ui->overwriteAppIDBox->isChecked() ? ui->appIDOverwriteEdit->text() : "")
    .workingDirectory(QDir::fromNativeSeparators(ui->workingDirEdit->text()))
    .flags(flags));

  if (ui->newFilesModCheckBox->isChecked()) {
    m_profile->storeSetting("custom_overwrites", ui->titleEdit->text(),
                            ui->newFilesModBox->currentText());
  }
  else {
	  m_profile->removeSetting("custom_overwrites", ui->titleEdit->text());
  }

  m_profile->removeForcedLibraries(ui->titleEdit->text());
  m_profile->storeForcedLibraries(ui->titleEdit->text(), m_forcedLibraries);
  m_profile->setForcedLibrariesEnabled(ui->titleEdit->text(), ui->forceLoadCheckBox->isChecked());
}


void EditExecutablesDialog::delayedRefresh()
{
  QModelIndex index = ui->executablesListBox->currentIndex();
  resetInput();
  refreshExecutablesWidget();
  on_executablesListBox_clicked(index);
}


void EditExecutablesDialog::on_forceLoadButton_clicked()
{
  ForcedLoadDialog dialog(m_gamePlugin, this);
  dialog.setValues(m_forcedLibraries);
  if (dialog.exec() == QDialog::Accepted) {
    m_forcedLibraries = dialog.values();
  }
}

void EditExecutablesDialog::on_forceLoadCheckBox_toggled()
{
  ui->forceLoadButton->setEnabled(ui->forceLoadCheckBox->isChecked());
}


void EditExecutablesDialog::on_addButton_clicked()
{
  if (executableChanged()) {
    saveExecutable();
  }

  resetInput();
  refreshExecutablesWidget();
}

void EditExecutablesDialog::on_browseBinaryButton_clicked()
{
  QString binaryName = FileDialogMemory::getOpenFileName(
      "editExecutableBinary", this, tr("Select a binary"), QString(),
      tr("Executable (%1)").arg("*.exe *.bat *.jar"));

  if (binaryName.isNull()) {
    // canceled
    return;
  }

  if (binaryName.endsWith(".jar", Qt::CaseInsensitive)) {
    QString binaryPath;
    { // try to find java automatically
      std::wstring binaryNameW = ToWString(binaryName);
      WCHAR buffer[MAX_PATH];
      if (::FindExecutableW(binaryNameW.c_str(), nullptr, buffer)
          > reinterpret_cast<HINSTANCE>(32)) {
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

  if (ui->titleEdit->text().isEmpty()) {
    ui->titleEdit->setText(QFileInfo(binaryName).baseName());
  }
}

void EditExecutablesDialog::on_browseWorkingDirButton_clicked()
{
  QString dirName = FileDialogMemory::getExistingDirectory("editExecutableDirectory", this,
                                                           tr("Select a directory"));

  if (dirName.isNull()) {
    // canceled
    return;
  }

  ui->workingDirEdit->setText(dirName);
}

void EditExecutablesDialog::on_removeButton_clicked()
{
  if (QMessageBox::question(this, tr("Confirm"), tr("Really remove \"%1\" from executables?").arg(ui->titleEdit->text()),
                            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    m_profile->removeSetting("custom_overwrites", ui->titleEdit->text());
    m_profile->removeForcedLibraries(ui->titleEdit->text());
    m_executablesList.remove(ui->titleEdit->text());
  }

  resetInput();
  refreshExecutablesWidget();
}

void EditExecutablesDialog::on_titleEdit_textChanged(const QString &arg1)
{
  /*QPushButton *addButton = findChild<QPushButton*>("addButton");
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
  }*/
}


bool EditExecutablesDialog::executableChanged()
{
  if (m_currentItem != nullptr) {
    const auto& title = m_currentItem->text();
    auto itor = m_executablesList.find(title);

    if (itor == m_executablesList.end()) {
      qWarning().nospace()
        << "executableChanged(): title '" << title << "' not found";

      return false;
    }

    const Executable& selectedExecutable = *itor;

    QString storedCustomOverwrite = m_profile->setting("custom_overwrites", selectedExecutable.title()).toString();

    bool forcedLibrariesDirty = false;
    auto forcedLibaries = m_profile->determineForcedLibraries(selectedExecutable.title());
    forcedLibrariesDirty |= !std::equal(forcedLibaries.begin(), forcedLibaries.end(),
                                        m_forcedLibraries.begin(), m_forcedLibraries.end(),
                                        [](const ExecutableForcedLoadSetting &lhs, const ExecutableForcedLoadSetting &rhs)
                                        {
                                          return lhs.enabled() == rhs.enabled() &&
                                                 lhs.forced() == rhs.forced() &&
                                                 lhs.library() == rhs.library() &&
                                                 lhs.process() == rhs.process();
                                        });
    forcedLibrariesDirty |= m_profile->setting("forced_libraries", ui->titleEdit->text() + "/enabled", false).toBool() !=
                            ui->forceLoadCheckBox->isChecked();

    return selectedExecutable.title() != ui->titleEdit->text()
        || selectedExecutable.arguments() != ui->argumentsEdit->text()
        || selectedExecutable.steamAppID() != ui->appIDOverwriteEdit->text()
        || !storedCustomOverwrite.isEmpty() != ui->newFilesModCheckBox->isChecked()
        || !storedCustomOverwrite.isEmpty() && (storedCustomOverwrite != ui->newFilesModBox->currentText())
        || selectedExecutable.workingDirectory() != QDir::fromNativeSeparators(ui->workingDirEdit->text())
        || selectedExecutable.binaryInfo().absoluteFilePath() != QDir::fromNativeSeparators(ui->binaryEdit->text())
        || selectedExecutable.usesOwnIcon() != ui->useAppIconCheckBox->isChecked()
        || forcedLibrariesDirty
      ;
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
  const auto selection = ui->executablesListBox->selectedItems();

  if (selection.empty()) {
    updateUI(nullptr);
    return;
  }

  auto* item = selection[0];
  if (!item) {
    return;
  }

  const auto& title = item->text();
  auto itor = m_executablesList.find(title);

  if (itor == m_executablesList.end()) {
    qWarning().nospace() << "selection: executable '" << title << "' not found";
    return;
  }

  updateUI(&*itor);
}

void EditExecutablesDialog::on_overwriteAppIDBox_toggled(bool checked)
{
  ui->appIDOverwriteEdit->setEnabled(checked);
}

void EditExecutablesDialog::on_buttonBox_accepted()
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

  accept();
}

void EditExecutablesDialog::on_buttonBox_rejected()
{
  reject();
}

void EditExecutablesDialog::on_executablesListBox_clicked(const QModelIndex &current)
{/*
  if (current.isValid()) {

    if (executableChanged()) {
      QMessageBox::StandardButton res = QMessageBox::question(this, tr("Save Changes?"),
                                                              tr("You made changes to the current executable, do you want to save them?"),
                                                              QMessageBox::Yes | QMessageBox::No);
      if (res == QMessageBox::Yes) {
        saveExecutable();

        //This is necessary if we're adding a new item, but it doesn't look very nice.
        //Ideally we'd end up with the correct row displayed
        ui->executablesListBox->selectionModel()->clearSelection();
        ui->executablesListBox->selectionModel()->select(current, QItemSelectionModel::SelectCurrent);
        QTimer::singleShot(50, this, SLOT(delayedRefresh()));
        return;
      }
    }

    ui->executablesListBox->selectionModel()->clearSelection();
    ui->executablesListBox->selectionModel()->select(current, QItemSelectionModel::SelectCurrent);

    m_CurrentItem = ui->executablesListBox->item(current.row());

    const auto& title = m_CurrentItem->text();
    auto itor = m_ExecutablesList.find(title);

    if (itor == m_ExecutablesList.end()) {
      qWarning().nospace() << "selection: executable '" << title << "' not found";
      return;
    }

    const Executable& selectedExecutable = *itor;

    ui->titleEdit->setText(selectedExecutable.title());
    ui->binaryEdit->setText(QDir::toNativeSeparators(selectedExecutable.binaryInfo().absoluteFilePath()));
    ui->argumentsEdit->setText(selectedExecutable.arguments());
    ui->workingDirEdit->setText(QDir::toNativeSeparators(selectedExecutable.workingDirectory()));
    ui->removeButton->setEnabled(selectedExecutable.isCustom());
    ui->overwriteAppIDBox->setChecked(!selectedExecutable.steamAppID().isEmpty());
    if (!selectedExecutable.steamAppID().isEmpty()) {
      ui->appIDOverwriteEdit->setText(selectedExecutable.steamAppID());
    } else {
      ui->appIDOverwriteEdit->clear();
    }
    ui->useAppIconCheckBox->setChecked(selectedExecutable.usesOwnIcon());

    int index = -1;

    QString customOverwrite = m_Profile->setting("custom_overwrites", selectedExecutable.title()).toString();
    if (!customOverwrite.isEmpty()) {
      index = ui->newFilesModBox->findText(customOverwrite);
      qDebug("find %s -> %d", qUtf8Printable(customOverwrite), index);
    }

    ui->newFilesModCheckBox->setChecked(index != -1);
    if (index != -1) {
      ui->newFilesModBox->setCurrentIndex(index);
    }

    m_ForcedLibraries = m_Profile->determineForcedLibraries(ui->titleEdit->text());
    bool forcedLibraries = m_Profile->forcedLibrariesEnabled(ui->titleEdit->text());
    ui->forceLoadButton->setEnabled(forcedLibraries);
    ui->forceLoadCheckBox->setChecked(forcedLibraries);
  }*/
}

void EditExecutablesDialog::on_newFilesModCheckBox_toggled(bool checked)
{
  ui->newFilesModBox->setEnabled(checked);
}
