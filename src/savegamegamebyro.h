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

#ifndef SAVEGAMEGAMEBRYO_H
#define SAVEGAMEGAMEBRYO_H

#include "savegame.h"

#include <QMetaType>
#include <QObject>
#include <QString>

namespace MOBase { class IPluginGame; class ISaveGame; }

/**
 * @brief represents a single save game
 **/
class SaveGameGamebryo : public SaveGame {

Q_OBJECT

public:

  /**
   * @brief construct a save game and immediately read out information from the file
   *
   * @param filename absolute path of the save game file
   **/
  SaveGameGamebryo(QObject *parent, const QString &filename, MOBase::IPluginGame const *game);

  /*

  SaveGameGamebryo(const SaveGameGamebryo &reference);

  SaveGameGamebryo &operator=(const SaveGameGamebryo &reference);

  ~SaveGameGamebryo();
  */

  /**
   * @return number of plugins that were enabled when the save game was created
   **/
  int numPlugins() const { return m_Plugins.size(); }

  /**
   * retrieve the name of one of the plugins that were enabled when the save game
   * was created. valid indices are in the range between [0, numPlugins()[
   * @param index plugin index
   * @return name of the plugin
   **/
  const QString &plugin(int index) const { return m_Plugins.at(index); }

private:

  QStringList m_Plugins;
  //Note: This isn't owned by us so safe to copy
  MOBase::ISaveGame const *m_Save;

};

#endif // SAVEGAMEGAMEBRYO_H
