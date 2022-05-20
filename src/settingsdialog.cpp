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

#include "settingsdialog.h"
#include "settingsdialogdiagnostics.h"
#include "settingsdialoggeneral.h"
#include "settingsdialogmodlist.h"
#include "settingsdialognexus.h"
#include "settingsdialogpaths.h"
#include "settingsdialogplugins.h"
#include "settingsdialogtheme.h"
#include "settingsdialogworkarounds.h"
#include "ui_settingsdialog.h"

using namespace MOBase;

SettingsDialog::SettingsDialog(PluginContainer* pluginContainer,
                               ThemeManager const& manager, Settings& settings,
                               QWidget* parent)
    : TutorableDialog("SettingsDialog", parent), ui(new Ui::SettingsDialog),
      m_settings(settings), m_exit(Exit::None), m_pluginContainer(pluginContainer)
{
  ui->setupUi(this);

  m_tabs.push_back(
      std::unique_ptr<SettingsTab>(new GeneralSettingsTab(settings, *this)));
  m_tabs.push_back(
      std::unique_ptr<SettingsTab>(new ThemeSettingsTab(settings, manager, *this)));
  m_tabs.push_back(
      std::unique_ptr<SettingsTab>(new ModListSettingsTab(settings, *this)));
  m_tabs.push_back(std::unique_ptr<SettingsTab>(new PathsSettingsTab(settings, *this)));
  m_tabs.push_back(
      std::unique_ptr<SettingsTab>(new DiagnosticsSettingsTab(settings, *this)));
  m_tabs.push_back(std::unique_ptr<SettingsTab>(new NexusSettingsTab(settings, *this)));
  m_tabs.push_back(std::unique_ptr<SettingsTab>(
      new PluginsSettingsTab(settings, m_pluginContainer, *this)));
  m_tabs.push_back(
      std::unique_ptr<SettingsTab>(new WorkaroundsSettingsTab(settings, *this)));
}

PluginContainer* SettingsDialog::pluginContainer()
{
  return m_pluginContainer;
}

QWidget* SettingsDialog::parentWidgetForDialogs()
{
  if (isVisible()) {
    return this;
  } else {
    return parentWidget();
  }
}

void SettingsDialog::setExitNeeded(ExitFlags e)
{
  m_exit = e;
}

ExitFlags SettingsDialog::exitNeeded() const
{
  return m_exit;
}

int SettingsDialog::exec()
{
  GeometrySaver gs(m_settings, this);

  m_settings.widgets().restoreIndex(ui->tabWidget);

  auto ret = TutorableDialog::exec();

  m_settings.widgets().saveIndex(ui->tabWidget);

  if (ret == QDialog::Accepted) {
    for (auto&& tab : m_tabs) {
      tab->closing();
    }

    // update settings for each tab
    for (std::unique_ptr<SettingsTab> const& tab : m_tabs) {
      tab->update();
    }
  }

  return ret;
}

SettingsDialog::~SettingsDialog()
{
  disconnect(this);
  delete ui;
}

QString SettingsDialog::getColoredButtonStyleSheet() const
{
  return QString("QPushButton {"
                 "background-color: %1;"
                 "color: %2;"
                 "border: 1px solid;"
                 "padding: 3px;"
                 "}");
}

void SettingsDialog::accept()
{
  QString newModPath = ui->modDirEdit->text();
  newModPath         = PathSettings::resolve(newModPath, ui->baseDirEdit->text());

  if ((QDir::fromNativeSeparators(newModPath) !=
       QDir::fromNativeSeparators(Settings::instance().paths().mods(true))) &&
      (QMessageBox::question(
           parentWidgetForDialogs(), tr("Confirm"),
           tr("Changing the mod directory affects all your profiles! "
              "Mods not present (or named differently) in the new location "
              "will be disabled in all profiles. "
              "There is no way to undo this unless you backed up your "
              "profiles manually. Proceed?"),
           QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)) {
    return;
  }

  TutorableDialog::accept();
}

SettingsTab::SettingsTab(Settings& s, SettingsDialog& d)
    : ui(d.ui), m_settings(s), m_dialog(d)
{}

SettingsTab::~SettingsTab() = default;

Settings& SettingsTab::settings()
{
  return m_settings;
}

SettingsDialog& SettingsTab::dialog()
{
  return m_dialog;
}

QWidget* SettingsTab::parentWidget()
{
  return m_dialog.parentWidgetForDialogs();
}
