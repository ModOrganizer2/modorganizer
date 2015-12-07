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
  virtual std::wstring getRegPath() { return getRegPathStatic(); }
  virtual std::wstring getBinaryName() { return L"TESV.exe"; }

  virtual GameInfo::Type getType() { return TYPE_SKYRIM; }

  virtual std::wstring getGameName() const { return L"Skyrim"; }
  virtual std::wstring getGameShortName() const { return L"Skyrim"; }

  virtual LoadOrderMechanism getLoadOrderMechanism() const;

  virtual std::vector<std::wstring> getDLCPlugins();

  virtual std::vector<std::wstring> getSavegameAttachmentExtensions();

  // file name of this games ini (no path)
  virtual std::vector<std::wstring> getIniFileNames();

  virtual std::wstring getReferenceDataFile();

  virtual std::wstring getNexusPage(bool nmmScheme = true);

  static std::wstring getNexusInfoUrlStatic();
  virtual std::wstring getNexusInfoUrl() { return getNexusInfoUrlStatic(); }
  static int getNexusModIDStatic();
  virtual int getNexusModID() { return getNexusModIDStatic(); }
  static int getNexusGameIDStatic() { return 110; }
  virtual int getNexusGameID() { return getNexusGameIDStatic(); }

private:

  SkyrimInfo(const std::wstring &moDirectory, const std::wstring &moDataDirectory, const std::wstring &gameDirectory);

  static bool identifyGame(const std::wstring &searchPath);

private:

  std::wstring m_AppData;

};

} // namespace MOShared

#endif // SKYRIMINFO_H
