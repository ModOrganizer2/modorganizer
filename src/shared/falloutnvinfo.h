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

#ifndef FALLOUTNVINFO_H
#define FALLOUTNVINFO_H


#include "gameinfo.h"

namespace MOShared {


class FalloutNVInfo : public GameInfo
{

  friend class GameInfo;

public:

  virtual ~FalloutNVInfo() {}

  static std::wstring getRegPathStatic();
  virtual std::wstring getRegPath() { return getRegPathStatic(); }

  virtual GameInfo::Type getType() { return TYPE_FALLOUTNV; }

  virtual std::wstring getGameName() const { return L"New Vegas"; }

  virtual std::vector<std::wstring> getSavegameAttachmentExtensions();

  // file name of this games ini (no path)
  virtual std::vector<std::wstring> getIniFileNames();

  virtual std::wstring getReferenceDataFile();

  virtual bool rerouteToProfile(const wchar_t *fileName, const wchar_t *fullPath);

  // get a list of executables (game binary and known-to-work 3rd party tools). All of these are relative to
  // the game directory
  //virtual std::vector<ExecutableInfo> getExecutables();

  virtual std::wstring archiveListKey() { return L"SArchiveList"; }

private:

  FalloutNVInfo(const std::wstring &gameDirectory);

  static bool identifyGame(const std::wstring &searchPath);
};

} // namespace MOShared

#endif // FALLOUTNVINFO_H
