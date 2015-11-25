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

#ifndef GAMEINFO_H
#define GAMEINFO_H

#include <string>
#include <vector>
#include <wchar.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace MOShared {


/**
  Class to manage information that depends on the used game type. The intention is to keep
  as much of the rest of omo is game-agnostic. It is mostly concerned with figuring out the
  correct paths, filenames and data types.
*/
class GameInfo
{

public:

  enum Type {
    TYPE_OBLIVION,
    TYPE_FALLOUT3,
    TYPE_FALLOUTNV,
    TYPE_SKYRIM
  };

  enum LoadOrderMechanism {
    TYPE_FILETIME,
    TYPE_PLUGINSTXT
  };

public:

  virtual ~GameInfo() {}

  //**USED ONLY IN HOOKDLL
  virtual std::wstring getRegPath() = 0;

  //**Used only in savegame which needs refactoring a lot.
  virtual GameInfo::Type getType() = 0;

  //**Currently only used in a nasty mess at initialisation time.
  virtual std::wstring getGameName() const = 0;

  //**USED IN HOOKDLL and in initialisation
  virtual std::wstring getGameDirectory() const;

  // get a list of file extensions for additional files belonging to a save game
  virtual std::vector<std::wstring> getSavegameAttachmentExtensions() = 0;

  //**USED ONLY IN HOOKDLL
  // file name of this games ini file(s)
  virtual std::vector<std::wstring> getIniFileNames() = 0;

  //**USED ONLY IN HOOKDLL
  virtual std::wstring getReferenceDataFile() = 0;

  virtual std::wstring getNexusPage(bool nmmScheme = true) = 0;
  virtual int getNexusGameID() = 0;

  //**USED ONLY IN HOOKDLL
  virtual bool rerouteToProfile(const wchar_t *fileName, const wchar_t *fullPath) = 0;

public:

  //**USED IN HOOKDLL
  // initialise with the path to the mo directory (needs to be where hook.dll is stored). This
  // needs to be called before the instance can be retrieved
  static bool init(const std::wstring &moDirectory, const std::wstring &gamePath = L"");

  //**USED IN HOOKDLL
  static GameInfo& instance();

protected:

  GameInfo(const std::wstring &gameDirectory);

  std::wstring getLocalAppFolder() const;
  const std::wstring &getMyGamesDirectory() const { return m_MyGamesDirectory; }
  void identifyMyGamesDirectory(const std::wstring &file);

private:

  static bool identifyGame(const std::wstring &searchPath);
  std::wstring getSpecialPath(LPCWSTR name) const;

  static void cleanup();

private:

  static GameInfo *s_Instance;

  std::wstring m_MyGamesDirectory;

  std::wstring m_GameDirectory;

};


} // namespace MOShared

#endif // GAMEINFO_H
