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

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include "shared/util.h"
#include "tutorabledialog.h"

class PluginManager;
class Settings;
class SettingsDialog;
class ThemeManager;
class TranslationManager;

namespace Ui
{
class SettingsDialog;
}

class SettingsTab
{
public:
  SettingsTab(Settings& settings, SettingsDialog& m_dialog);
  virtual ~SettingsTab();

  virtual void update() = 0;
  virtual void closing() {}

protected:
  Ui::SettingsDialog* ui;

  Settings& settings();
  SettingsDialog& dialog();
  QWidget* parentWidget();

private:
  Settings& m_settings;
  SettingsDialog& m_dialog;
};

/**
 * dialog used to change settings for Mod Organizer. On top of the
 * settings managed by the "Settings" class, this offers a button to open the
 * CategoriesDialog
 **/
class SettingsDialog : public MOBase::TutorableDialog
{
  Q_OBJECT;
  friend class SettingsTab;

public:
  explicit SettingsDialog(PluginManager& pluginManager,
                          ThemeManager const& themeManager,
                          TranslationManager const& translationManager,
                          Settings& settings, QWidget* parent = 0);

  ~SettingsDialog();

  /**
   * @brief get stylesheet of settings buttons with colored background
   * @return string of stylesheet
   */
  QString getColoredButtonStyleSheet() const;

  QWidget* parentWidgetForDialogs();

  void setExitNeeded(ExitFlags e);
  ExitFlags exitNeeded() const;

  int exec() override;

public slots:
  virtual void accept();

private:
  Ui::SettingsDialog* ui;
  Settings& m_settings;
  std::vector<std::unique_ptr<SettingsTab>> m_tabs;
  ExitFlags m_exit;
  PluginManager* m_pluginManager;
};

#endif  // SETTINGSDIALOG_H
