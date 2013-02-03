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

#ifndef SAVEGAMEINFOWIDGETGAMEBRYO_H
#define SAVEGAMEINFOWIDGETGAMEBRYO_H


#include "savegamegamebyro.h"
#include "savegameinfowidget.h"
#include "pluginlist.h"


class SaveGameInfoWidgetGamebryo : public SaveGameInfoWidget
{
public:

  explicit SaveGameInfoWidgetGamebryo(const SaveGame *saveGame, PluginList *pluginList, QWidget *parent = 0);

  virtual void setSave(const SaveGame *saveGame);

private:

  PluginList *m_PluginList;

};

#endif // SAVEGAMEINFOWIDGETGAMEBRYO_H
