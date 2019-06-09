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

  m_customOverwrites.load(profile, m_executablesList);
  m_forcedLibraries.load(profile, m_executablesList);

  fillExecutableList();
  ui->mods->addItems(modList.allMods());

  // some widgets need to do more than just save() and have their own handler
  connect(ui->binary, &QLineEdit::textChanged, [&]{ save(); });
  connect(ui->workingDirectory, &QLineEdit::textChanged, [&]{ save(); });
  connect(ui->arguments, &QLineEdit::textChanged, [&]{ save(); });
  connect(ui->steamAppID, &QLineEdit::textChanged, [&]{ save(); });
  connect(ui->mods, &QComboBox::currentTextChanged, [&]{ save(); });
  connect(ui->useApplicationIcon, &QCheckBox::toggled, [&]{ save(); });

  // select the first one in the list, if any
  if (ui->list->count() > 0) {
    ui->list->item(0)->setSelected(true);
  } else {
    updateUI(nullptr);
  }
}

EditExecutablesDialog::~EditExecutablesDialog() = default;

ExecutablesList EditExecutablesDialog::getExecutablesList() const
{
  ExecutablesList newList;

  // make sure the executables are in the same order as in the list
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

const CustomOverwrites& EditExecutablesDialog::getCustomOverwrites() const
{
  return m_customOverwrites;
}

const ForcedLibraries& EditExecutablesDialog::getForcedLibraries() const
{
  return m_forcedLibraries;
}

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

    if (const auto mod=m_customOverwrites.find(e.title())) {
      modIndex = ui->mods->findText(*mod);

      if (modIndex == -1) {
        qWarning().nospace()
          << "executable '" << e.title() << "' uses mod '" << *mod << "' "
          << "as a custom overwrite, but that mod doesn't exist";
      }
    }

    ui->createFilesInMod->setChecked(modIndex != -1);
    ui->mods->setEnabled(modIndex != -1);
    ui->mods->setCurrentIndex(modIndex);
  }

  {
    const auto hasForcedLibraries = m_forcedLibraries.find(e.title()).has_value();

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
    m_customOverwrites.set(e->title(), ui->mods->currentText());
  } else {
    m_customOverwrites.remove(e->title());
  }

  // forced libraries are saved in on_configureLibraries_clicked()

  // now rename both the custom overwrites and forced libraries if the title
  // is being changed
  if (e->title() != ui->title->text()) {
    m_customOverwrites.rename(e->title(), ui->title->text());
    m_forcedLibraries.rename(e->title(), ui->title->text());
  }

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


  m_customOverwrites.remove(exe->title());
  m_forcedLibraries.remove(exe->title());

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

  // must save before modifying the item in the list widget because saving
  // relies on the item's text being the same as an item in m_executablesList
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

  if (auto list=m_forcedLibraries.find(e->title())) {
    dialog.setValues(*list);
  }

  if (dialog.exec() == QDialog::Accepted) {
    m_forcedLibraries.set(e->title(), dialog.values());
    save();
  }
}

void EditExecutablesDialog::on_buttons_accepted()
{
  accept();
}

void EditExecutablesDialog::on_buttons_rejected()
{
  reject();
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


void CustomOverwrites::load(Profile* p, const ExecutablesList& exes)
{
  for (const auto& e : exes) {
    const auto s = p->setting("custom_overwrites", e.title()).toString();

    if (!s.isEmpty()) {
      m_map[e.title()] = s;
    }
  }
}

std::optional<QString> CustomOverwrites::find(const QString& title) const
{
  auto itor = m_map.find(title);
  if (itor == m_map.end()) {
    return {};
  }

  return itor->second;
}

void CustomOverwrites::set(const QString& title, const QString& mod)
{
  m_map[title] = mod;
}

void CustomOverwrites::rename(const QString& oldTitle, const QString& newTitle)
{
  auto itor = m_map.find(oldTitle);
  if (itor == m_map.end()) {
    return;
  }

  // copy to new title, erase old
  m_map[newTitle] = itor->second;
  m_map.erase(itor);
}

void CustomOverwrites::remove(const QString& title)
{
  auto itor = m_map.find(title);

  if (itor != m_map.end()) {
    m_map.erase(itor);
  }
}


void ForcedLibraries::load(Profile* p, const ExecutablesList& exes)
{
  for (const auto& e : exes) {
    if (p->forcedLibrariesEnabled(e.title())) {
      m_map[e.title()] = p->determineForcedLibraries(e.title());
    }
  }
}

std::optional<ForcedLibraries::list_type> ForcedLibraries::find(const QString& title) const
{
  auto itor = m_map.find(title);
  if (itor == m_map.end()) {
    return {};
  }

  return itor->second;
}

void ForcedLibraries::set(const QString& title, const list_type& mod)
{
  m_map[title] = mod;
}

void ForcedLibraries::rename(const QString& oldTitle, const QString& newTitle)
{
  auto itor = m_map.find(oldTitle);
  if (itor == m_map.end()) {
    return;
  }

  // copy to new title, erase old
  m_map[newTitle] = itor->second;
  m_map.erase(itor);
}

void ForcedLibraries::remove(const QString& title)
{
  auto itor = m_map.find(title);

  if (itor != m_map.end()) {
    m_map.erase(itor);
  }
}
