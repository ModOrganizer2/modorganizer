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

#include "tutorabledialog.h"
#include "nxmaccessmanager.h"
#include <iplugin.h>
#include <QListWidgetItem>

class PluginContainer;
class Settings;

namespace Ui {
    class SettingsDialog;
}


/**
 * dialog used to change settings for Mod Organizer. On top of the
 * settings managed by the "Settings" class, this offers a button to open the
 * CategoriesDialog
 **/
class SettingsDialog : public MOBase::TutorableDialog
{
    Q_OBJECT

public:
  explicit SettingsDialog(
    PluginContainer *pluginContainer, Settings* settings, QWidget *parent = 0);

  ~SettingsDialog();

  /**
  * @brief get stylesheet of settings buttons with colored background
  * @return string of stylesheet
  */
  QString getColoredButtonStyleSheet() const;

  // temp
  Ui::SettingsDialog *ui;
  bool m_keyChanged;
  bool m_GeometriesReset;
  PluginContainer *m_PluginContainer;

public slots:
  virtual void accept();

public:
  bool getApiKeyChanged();
  bool getResetGeometries();

private:
  Settings* m_settings;


};

#endif // SETTINGSDIALOG_H
