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
#include "forcedloaddialog.h"
#include "organizercore.h"

#include <QMessageBox>
#include <Shellapi.h>
#include <utility.h>
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
  , m_settingUI(false)
{
  ui->setupUi(this);
  ui->splitter->setSizes({200, 1});
  ui->splitter->setStretchFactor(0, 0);
  ui->splitter->setStretchFactor(1, 1);

  for (const auto& e : m_executablesList) {
    // custom overwrites
    QString customOverwrite = m_profile->setting("custom_overwrites", e.title()).toString();
    if (!customOverwrite.isEmpty()) {
      m_customOverwrites[e.title()] = customOverwrite;
    }

    // forced libraries
    if (m_profile->forcedLibrariesEnabled(e.title())) {
      m_forcedLibraries[e.title()] = m_profile->determineForcedLibraries(e.title());
    }
  }


  fillExecutableList();
  ui->mods->addItems(modList.allMods());


  // some widgets need to do more than just save() and have their own handler

  connect(ui->binary, &QLineEdit::textChanged, [&]{ save(); });
  connect(ui->workingDirectory, &QLineEdit::textChanged, [&]{ save(); });
  connect(ui->arguments, &QLineEdit::textChanged, [&]{ save(); });
  connect(ui->steamAppID, &QLineEdit::textChanged, [&]{ save(); });
  connect(ui->mods, &QComboBox::currentTextChanged, [&]{ save(); });
  connect(ui->useApplicationIcon, &QCheckBox::toggled, [&]{ save(); });

  updateUI(nullptr);
}

EditExecutablesDialog::~EditExecutablesDialog() = default;

QListWidgetItem* EditExecutablesDialog::selectedItem()
{
  const auto selection = ui->list->selectedItems();

  if (selection.empty()) {
    return nullptr;
  }

  return selection[0];
}

Executable* EditExecutablesDialog::selectedExe()
{
  auto* item = selectedItem();
  if (!item) {
    return nullptr;
  }

  const auto& title = item->text();
  auto itor = m_executablesList.find(title);

  if (itor == m_executablesList.end()) {
    return nullptr;
  }

  return &*itor;
}

void EditExecutablesDialog::fillExecutableList()
{
  ui->list->clear();

  for(const auto& exe : m_executablesList) {
    ui->list->addItem(createListItem(exe));
  }
}

QListWidgetItem* EditExecutablesDialog::createListItem(const Executable& exe)
{
  QListWidgetItem *newItem = new QListWidgetItem(exe.title());

  if (!exe.isCustom()) {
    auto f = newItem->font();
    f.setItalic(true);
    newItem->setFont(f);
  }

  return newItem;
}

void EditExecutablesDialog::updateUI(const Executable* e)
{
  // the ui is currently being set, ignore changes
  m_settingUI = true;

  if (e) {
    setEdits(*e);
    ui->remove->setEnabled(e->isCustom());
  } else {
    clearEdits();
    ui->remove->setEnabled(false);
  }

  // any changes from now on are from the user
  m_settingUI = false;
}

void EditExecutablesDialog::clearEdits()
{
  ui->title->clear();
  ui->title->setEnabled(false);
  ui->binary->clear();
  ui->binary->setEnabled(false);
  ui->browseBinary->setEnabled(false);
  ui->workingDirectory->clear();
  ui->workingDirectory->setEnabled(false);
  ui->browseWorkingDirectory->setEnabled(false);
  ui->arguments->clear();
  ui->arguments->setEnabled(false);
  ui->overwriteSteamAppID->setEnabled(false);
  ui->overwriteSteamAppID->setChecked(false);
  ui->steamAppID->setEnabled(false);
  ui->steamAppID->clear();
  ui->createFilesInMod->setEnabled(false);
  ui->createFilesInMod->setChecked(false);
  ui->mods->setEnabled(false);
  ui->mods->setCurrentIndex(-1);
  ui->forceLoadLibraries->setEnabled(false);
  ui->forceLoadLibraries->setChecked(false);
  ui->configureLibraries->setEnabled(false);
  ui->useApplicationIcon->setEnabled(false);
  ui->useApplicationIcon->setChecked(false);
  ui->pluginProvidedLabel->setVisible(false);
}

void EditExecutablesDialog::setEdits(const Executable& e)
{
  ui->title->setText(e.title());
  ui->binary->setText(QDir::toNativeSeparators(e.binaryInfo().absoluteFilePath()));
  ui->workingDirectory->setText(QDir::toNativeSeparators(e.workingDirectory()));
  ui->arguments->setText(e.arguments());
  ui->overwriteSteamAppID->setChecked(!e.steamAppID().isEmpty());
  ui->steamAppID->setEnabled(!e.steamAppID().isEmpty());
  ui->steamAppID->setText(e.steamAppID());
  ui->useApplicationIcon->setChecked(e.usesOwnIcon());

  {
    int modIndex = -1;

    auto itor = m_customOverwrites.find(e.title());
    if (itor != m_customOverwrites.end()) {
      modIndex = ui->mods->findText(itor->second);

      if (modIndex == -1) {
        qWarning().nospace()
          << "executable '" << e.title() << "' uses mod '" << itor->second << "' "
          << "as a custom overwrite, but that mod doesn't exist";
      }
    }

    ui->createFilesInMod->setChecked(modIndex != -1);
    ui->mods->setEnabled(modIndex != -1);
    ui->mods->setCurrentIndex(modIndex);
  }

  {
    auto itor = m_forcedLibraries.find(e.title());
    const auto hasForcedLibraries = (itor != m_forcedLibraries.end());

    ui->forceLoadLibraries->setChecked(hasForcedLibraries);
    ui->configureLibraries->setEnabled(hasForcedLibraries);
  }

  ui->pluginProvidedLabel->setVisible(!e.isCustom());

  // only enabled for custom executables
  ui->title->setEnabled(e.isCustom());
  ui->binary->setEnabled(e.isCustom());
  ui->browseBinary->setEnabled(e.isCustom());
  ui->workingDirectory->setEnabled(e.isCustom());
  ui->browseWorkingDirectory->setEnabled(e.isCustom());
  ui->arguments->setEnabled(e.isCustom());
  ui->overwriteSteamAppID->setEnabled(e.isCustom());
  ui->useApplicationIcon->setEnabled(e.isCustom());

  // always enabled
  ui->createFilesInMod->setEnabled(true);
  ui->forceLoadLibraries->setEnabled(true);
}

void EditExecutablesDialog::save()
{
  if (m_settingUI) {
    return;
  }

  auto* e = selectedExe();
  if (!e) {
    qWarning("trying to save but nothing is selected");
    return;
  }

  qDebug().nospace() << "saving '" << e->title() << "'";

  // title may have changed, start with the stuff using it
  if (ui->createFilesInMod->isChecked()) {
    m_customOverwrites[e->title()] = ui->mods->currentText();
  } else {
    auto itor = m_customOverwrites.find(e->title());
    if (itor != m_customOverwrites.end()) {
      m_customOverwrites.erase(itor);
    }
  }

  // forced libraries are saved in on_configureLibraries_clicked()

  e->title(ui->title->text());
  e->binaryInfo(ui->binary->text());
  e->workingDirectory(ui->workingDirectory->text());
  e->arguments(ui->arguments->text());

  if (ui->overwriteSteamAppID->isChecked()) {
    e->steamAppID(ui->steamAppID->text());
  } else {
    e->steamAppID("");
  }

  if (ui->useApplicationIcon->isChecked()) {
    e->flags(e->flags() | Executable::UseApplicationIcon);
  } else {
    e->flags(e->flags() & (~Executable::UseApplicationIcon));
  }
}

void EditExecutablesDialog::on_list_itemSelectionChanged()
{
  updateUI(selectedExe());
}

void EditExecutablesDialog::on_add_clicked()
{
  auto title = newExecutableTitle();
  if (title.isNull()) {
    return;
  }

  auto e = Executable()
    .title(title)
    .flags(Executable::CustomExecutable);

  m_executablesList.setExecutable(e);

  auto* item = createListItem(e);
  ui->list->addItem(item);
  item->setSelected(true);
}

void EditExecutablesDialog::on_remove_clicked()
{
  auto* item = selectedItem();
  if (!item) {
    qWarning("trying to remove entry but nothing is selected");
    return;
  }

  auto* exe = selectedExe();
  if (!exe) {
    qWarning("trying to remove entry but nothing is selected");
    return;
  }

  const int currentRow = ui->list->row(item);
  delete item;


  // removing custom overwrite
  {
    auto itor = m_customOverwrites.find(exe->title());
    if (itor != m_customOverwrites.end()) {
      m_customOverwrites.erase(itor);
    }
  }

  // removing forced libraries
  {
    auto itor = m_forcedLibraries.find(exe->title());
    if (itor != m_forcedLibraries.end()) {
      m_forcedLibraries.erase(itor);
    }
  }

  // removing from main list, must be done last because it invalidates the
  // exe pointer
  m_executablesList.remove(exe->title());


  // reselecting the same row as before, or the last one
  if (currentRow >= ui->list->count()) {
    // that was the last item, select the new list item, if any
    if (ui->list->count() > 0) {
      ui->list->item(ui->list->count() - 1)->setSelected(true);
    }
  } else {
    ui->list->item(currentRow)->setSelected(true);
  }
}

void EditExecutablesDialog::on_title_textChanged(const QString& s)
{
  if (m_settingUI) {
    return;
  }

  // must save first because it relies on the text in the list to find the
  // executable to modify
  save();

  // once the executable is saved, the list item must be changed to match the
  // new name
  if (auto* i=selectedItem()) {
    i->setText(s);
  }
}

void EditExecutablesDialog::on_overwriteSteamAppID_toggled(bool checked)
{
  if (m_settingUI) {
    return;
  }

  ui->steamAppID->setEnabled(checked);
  save();
}

void EditExecutablesDialog::on_createFilesInMod_toggled(bool checked)
{
  if (m_settingUI) {
    return;
  }

  ui->mods->setEnabled(checked);
  save();
}

void EditExecutablesDialog::on_forceLoadLibraries_toggled(bool checked)
{
  if (m_settingUI) {
    return;
  }

  ui->configureLibraries->setEnabled(ui->forceLoadLibraries->isChecked());
  save();
}

void EditExecutablesDialog::on_browseBinary_clicked()
{
  const QString binaryName = FileDialogMemory::getOpenFileName(
    "editExecutableBinary", this, tr("Select a binary"), ui->binary->text(),
    tr("Executable (%1)").arg("*.exe *.bat *.jar"));

  if (binaryName.isNull()) {
    // canceled
    return;
  }

  if (binaryName.endsWith(".jar", Qt::CaseInsensitive)) {
    setJarBinary(binaryName);
  } else {
    ui->binary->setText(QDir::toNativeSeparators(binaryName));
  }

  if (ui->title->text().isEmpty()) {
    ui->title->setText(QFileInfo(binaryName).baseName());
  }

  save();
}

void EditExecutablesDialog::on_browseWorkingDirectory_clicked()
{
  QString dirName = FileDialogMemory::getExistingDirectory(
    "editExecutableDirectory", this, tr("Select a directory"),
    ui->workingDirectory->text());

  if (dirName.isNull()) {
    // canceled
    return;
  }

  ui->workingDirectory->setText(dirName);
}

void EditExecutablesDialog::on_configureLibraries_clicked()
{
  auto* e = selectedExe();
  if (!e) {
    qWarning("trying to configure libraries but nothing is selected");
    return;
  }

  ForcedLoadDialog dialog(m_gamePlugin, this);

  auto itor = m_forcedLibraries.find(e->title());
  if (itor != m_forcedLibraries.end()) {
    dialog.setValues(itor->second);
  }

  if (dialog.exec() == QDialog::Accepted) {
    m_forcedLibraries[e->title()] = dialog.values();
    save();
  }
}

void EditExecutablesDialog::setJarBinary(const QString& binaryName)
{
  auto java = OrganizerCore::findJavaInstallation(binaryName);

  if (java.isEmpty()) {
    QMessageBox::information(
      this, tr("Java (32-bit) required"),
      tr("MO requires 32-bit java to run this application. If you already "
         "have it installed, select javaw.exe from that installation as "
         "the binary."));
  }

  // only save once

  m_settingUI = true;
  ui->binary->setText(java);
  ui->workingDirectory->setText(QDir::toNativeSeparators(QFileInfo(binaryName).absolutePath()));
  ui->arguments->setText("-jar \"" + QDir::toNativeSeparators(binaryName) + "\"");
  m_settingUI = false;

  save();
}

QString EditExecutablesDialog::newExecutableTitle()
{
  const auto prefix = tr("New Executable");

  QString title = prefix;

  for (int i=1; i<100; ++i) {
    if (!m_executablesList.titleExists(title)) {
      return title;
    }

    title = prefix + QString(" (%1)").arg(i);
  }

  qCritical().nospace() << "ran out of new executable titles";
  return QString::null;
}



ExecutablesList EditExecutablesDialog::getExecutablesList() const
{
  ExecutablesList newList;
  for (int i = 0; i < ui->list->count(); ++i) {
    const auto& title = ui->list->item(i)->text();
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


void EditExecutablesDialog::updateButtonStates()
{
  bool enabled = true;

  QString filePath(ui->binary->text());
  QFileInfo fileInfo(filePath);
  if (!fileInfo.exists())
    enabled = false;
  if (!fileInfo.isFile())
    enabled = false;

  QString dirPath(ui->workingDirectory->text());
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
  if (ui->useApplicationIcon->isChecked())
    flags |= Executable::UseApplicationIcon;

  m_executablesList.setExecutable(Executable()
    .title(ui->title->text())
    .binaryInfo(QDir::fromNativeSeparators(ui->binary->text()))
    .arguments(ui->arguments->text())
    .steamAppID(ui->overwriteSteamAppID->isChecked() ? ui->steamAppID->text() : "")
    .workingDirectory(QDir::fromNativeSeparators(ui->workingDirectory->text()))
    .flags(flags));

  if (ui->createFilesInMod->isChecked()) {
    m_profile->storeSetting("custom_overwrites", ui->title->text(),
                            ui->mods->currentText());
  }
  else {
	  m_profile->removeSetting("custom_overwrites", ui->title->text());
  }

  //m_profile->removeForcedLibraries(ui->title->text());
  //m_profile->storeForcedLibraries(ui->title->text(), m_forcedLibraries);
  //m_profile->setForcedLibrariesEnabled(ui->title->text(), ui->forceLoadLibraries->isChecked());
}


void EditExecutablesDialog::delayedRefresh()
{
  /*QModelIndex index = ui->executablesListBox->currentIndex();
  resetInput();
  refreshExecutablesWidget();
  on_executablesListBox_clicked(index);*/
}







bool EditExecutablesDialog::executableChanged()
{
  /*if (m_currentItem != nullptr) {
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
    forcedLibrariesDirty |= m_profile->setting("forced_libraries", ui->title->text() + "/enabled", false).toBool() !=
                            ui->forceLoadLibraries->isChecked();

    return selectedExecutable.title() != ui->title->text()
        || selectedExecutable.arguments() != ui->arguments->text()
        || selectedExecutable.steamAppID() != ui->steamAppID->text()
        || !storedCustomOverwrite.isEmpty() != ui->createFilesInMod->isChecked()
        || !storedCustomOverwrite.isEmpty() && (storedCustomOverwrite != ui->mods->currentText())
        || selectedExecutable.workingDirectory() != QDir::fromNativeSeparators(ui->workingDirectory->text())
        || selectedExecutable.binaryInfo().absoluteFilePath() != QDir::fromNativeSeparators(ui->binary->text())
        || selectedExecutable.usesOwnIcon() != ui->useApplicationIcon->isChecked()
        || forcedLibrariesDirty
      ;
  } else {
    QFileInfo fileInfo(ui->binary->text());
    return !ui->binary->text().isEmpty()
        && !ui->title->text().isEmpty()
        && fileInfo.exists()
        && fileInfo.isFile();
  }*/

  return false;
}

void EditExecutablesDialog::on_buttons_accepted()
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
      //refreshExecutablesWidget();
    }
  }

  accept();
}

void EditExecutablesDialog::on_buttons_rejected()
{
  reject();
}

/*void EditExecutablesDialog::on_executablesListBox_clicked(const QModelIndex &current)
{
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
  }
}*/
