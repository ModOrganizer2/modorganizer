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

#ifndef FALLOUT3INFO_H
#define FALLOUT3INFO_H

#include "gameinfo.h"

namespace MOShared {


class Fallout3Info : public GameInfo
{

  friend class GameInfo;

public:

  virtual ~Fallout3Info() {}

  static std::wstring getRegPathStatic();
  virtual std::wstring getRegPath() const { return getRegPathStatic(); }

  // file name of this games ini (no path)
  virtual std::vector<std::wstring> getIniFileNames() const;

  virtual std::wstring getReferenceDataFile() const;

  virtual bool rerouteToProfile(const wchar_t *fileName, const wchar_t *fullPath) const;

  virtual std::wstring archiveListKey() const { return L"SArchiveList"; }

private:

  Fallout3Info(const std::wstring &gameDirectory);

  static bool identifyGame(const std::wstring &searchPath);

};

} // namespace MOShared

#endif // FALLOUT3INFO_H
