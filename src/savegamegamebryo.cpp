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

#include "savegamegamebyro.h"

#include "isavegame.h"
#include "savegameinfo.h"
#include "iplugingame.h"
#include "gamebryosavegame.h"

#include <limits>
#include <set>

using namespace MOBase;


SaveGameGamebryo::SaveGameGamebryo(QObject *parent, const QString &fileName, IPluginGame const *game)
  : SaveGame(parent, fileName, game)
  , m_Plugins()
{
  SaveGameInfo const *info = game->feature<SaveGameInfo>();
  if (info != nullptr) {
    ISaveGame const *save = info->getSaveGameInfo(fileName);
    m_Save = save;

    //Kludgery
    GamebryoSaveGame const *s = dynamic_cast<GamebryoSaveGame const *>(save);
    m_PCName = s->getPCName();
    m_PCLevel = s->getPCLevel();
    m_PCLocation = s->getPCLocation();
    m_SaveNumber = s->getSaveNumber();

    QDateTime modified = s->getCreationTime();
    memset(&m_CreationTime, 0, sizeof(SYSTEMTIME));

    m_CreationTime.wDay = static_cast<WORD>(modified.date().day());
    m_CreationTime.wDayOfWeek = static_cast<WORD>(modified.date().dayOfWeek());
    m_CreationTime.wMonth = static_cast<WORD>(modified.date().month());
    m_CreationTime.wYear =static_cast<WORD>( modified.date().year());

    m_CreationTime.wHour = static_cast<WORD>(modified.time().hour());
    m_CreationTime.wMinute = static_cast<WORD>(modified.time().minute());
    m_CreationTime.wSecond = static_cast<WORD>(modified.time().second());
    m_CreationTime.wMilliseconds = static_cast<WORD>(modified.time().msec());

    m_Screenshot = s->getScreenshot();

    m_Plugins = s->getPlugins();
  }
}
