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

#ifndef SKYRIMINFO_H
#define SKYRIMINFO_H


#include "gameinfo.h"

namespace MOShared {


class SkyrimInfo : public GameInfo
{

  friend class GameInfo;

public:

  virtual ~SkyrimInfo() {}

  static std::wstring getRegPathStatic();
  virtual std::wstring getRegPath() const { return getRegPathStatic(); }

  // file name of this games ini (no path)
  virtual std::vector<std::wstring> getIniFileNames() const;

  virtual std::wstring getReferenceDataFile() const;

  virtual bool rerouteToProfile(const wchar_t *fileName, const wchar_t *fullPath) const;

private:

  SkyrimInfo(const std::wstring &gameDirectory);

  static bool identifyGame(const std::wstring &searchPath);

private:

  std::wstring m_AppData;

};

} // namespace MOShared

#endif // SKYRIMINFO_H
