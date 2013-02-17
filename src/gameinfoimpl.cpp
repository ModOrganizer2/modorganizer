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

#include "gameinfoimpl.h"
#include <gameinfo.h>
#include <utility.h>
#include <QDir>


using namespace MOBase;
using namespace MOShared;


GameInfoImpl::GameInfoImpl()
{
}

IGameInfo::Type GameInfoImpl::type() const
{
  switch (GameInfo::instance().getType()) {
    case GameInfo::TYPE_OBLIVION: return IGameInfo::TYPE_OBLIVION;
    case GameInfo::TYPE_FALLOUT3: return IGameInfo::TYPE_FALLOUT3;
    case GameInfo::TYPE_FALLOUTNV: return IGameInfo::TYPE_FALLOUTNV;
    case GameInfo::TYPE_SKYRIM: return IGameInfo::TYPE_SKYRIM;
    default: throw MyException(QObject::tr("invalid game type %1").arg(GameInfo::instance().getType()));
  }
}


QString GameInfoImpl::path() const
{
  return QDir::fromNativeSeparators(ToQString(GameInfo::instance().getGameDirectory()));
}

QString GameInfoImpl::binaryName() const
{
  return ToQString(GameInfo::instance().getBinaryName());
}
