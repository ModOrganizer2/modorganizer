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

#ifndef ACTIVATEMODSDIALOG_H
#define ACTIVATEMODSDIALOG_H

#include "tutorabledialog.h"
#include <map>
#include <set>

namespace Ui {
    class ActivateModsDialog;
}

/**
 * @brief Dialog that is used to batch activate/deactivate mods and plugins
 **/
class ActivateModsDialog : public MOBase::TutorableDialog
{
  Q_OBJECT

public:
 /**
  * @brief constructor
  *
  * @param missingPlugins a map containing missing plugins that need to be activated
  * @param parent ... Defaults to 0.
  **/
 explicit ActivateModsDialog(const std::map<QString, std::vector<QString> > &missingPlugins, QWidget *parent = 0);
  ~ActivateModsDialog();

  /**
   * @brief get a list of mods that the user chose to activate
   * 
   * @note This can of ocurse only be called after the dialog has been displayed
   *
   * @return set< QString > the mods to activate
   **/
  std::set<QString> getModsToActivate();

  /**
   * @brief get a list of plugins that should be activated
   *
   * @return set< QString > the plugins to activate. This contains only plugins that become available after enabling the mods retrieved with getModsToActivate
   **/
  std::set<QString> getESPsToActivate();

private slots:

private:
  Ui::ActivateModsDialog *ui;
};

#endif // ACTIVATEMODSDIALOG_H
