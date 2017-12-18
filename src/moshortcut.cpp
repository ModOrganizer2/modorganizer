/*
Copyright (C) 2016 Sebastian Herbord. All rights reserved.

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


#include "moshortcut.h"


MOShortcut::MOShortcut(const QString& link)
  : m_valid(link.startsWith("moshortcut://"))
  , m_hasInstance(false)
{
  if (m_valid) {
    int start = (int)strlen("moshortcut://");
    int sep = link.indexOf(':', start);
    if (sep >= 0) {
      m_hasInstance = true;
      m_instance = link.mid(start, sep - start);
      m_executable = link.mid(sep + 1);
    }
    else
      m_executable = link.mid(start);
  }
}
