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

#include "loadmechanism.h"
#include "utility.h"
#include "shared/util.h"
#include <iplugingame.h>
#include <scriptextender.h>
#include "shared/appconfig.h"
#include <log.h>

using namespace MOBase;
using namespace MOShared;

LoadMechanism::LoadMechanism()
  : m_SelectedMechanism(LOAD_MODORGANIZER)
{
}

bool LoadMechanism::isDirectLoadingSupported() const
{
  return true;
}

void LoadMechanism::activate(EMechanism)
{
  // no-op
}


QString toString(LoadMechanism::EMechanism e)
{
  switch (e)
  {
    case LoadMechanism::LOAD_MODORGANIZER:
      return "ModOrganizer";

    default:
      return QString("unknown (%1)").arg(static_cast<int>(e));
  }
}
