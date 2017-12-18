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


#pragma once


#include <QString>


class MOShortcut {

public:
  MOShortcut(const QString& link);

  /// true iff intialized using a valid moshortcut link
  operator bool() const { return m_valid; }

  bool hasInstance() const { return m_hasInstance; }

  const QString& instance() const { return m_instance; }

  const QString& executable() const { return m_executable; }

private:
  QString m_instance;
  QString m_executable;
  bool m_valid;
  bool m_hasInstance;
};
