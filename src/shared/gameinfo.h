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

enum CloseMOStyle {
  DEFAULT_CLOSE,
  DEFAULT_STAY,
  NEVER_CLOSE
};

struct ExecutableInfo {

  ExecutableInfo(const std::wstring &aTitle, const std::wstring &aBinary,
                 const std::wstring &aArguments, const std::wstring &aWorkingDirectory, CloseMOStyle aCloseMO)
    : title(aTitle), binary(aBinary), arguments(aArguments), workingDirectory(aWorkingDirectory),
      closeMO(aCloseMO), steamAppID(L"") {}
  ExecutableInfo(const std::wstring &aTitle, const std::wstring &aBinary,
                 const std::wstring &aArguments, const std::wstring &aWorkingDirectory,
                 CloseMOStyle aCloseMO, const std::wstring &aSteamAppID)
    : title(aTitle), binary(aBinary), arguments(aArguments), workingDirectory(aWorkingDirectory),
      closeMO(aCloseMO), steamAppID(aSteamAppID) {}
  std::wstring title;
  std::wstring binary;
  std::wstring arguments;
  std::wstring workingDirectory;
  CloseMOStyle closeMO;
  std::wstring steamAppID;
};


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

  std::wstring getOrganizerDirectory() { return m_OrganizerDirectory; }

  virtual std::wstring getRegPath() = 0;
  virtual std::wstring getBinaryName() = 0;

  virtual unsigned long getBSAVersion() = 0;

  virtual GameInfo::Type getType() = 0;

  virtual std::wstring getGameName() const = 0;
  virtual std::wstring getGameShortName() const = 0;

  // full path to this games "My Games"-directory
  virtual std::wstring getDocumentsDir() = 0;

  virtual std::wstring getSaveGameDir() = 0;

  /// determine the load order mechanism used by this game. this may throw an
  /// exception if the mechanism can't be determined
  virtual LoadOrderMechanism getLoadOrderMechanism() const { return TYPE_FILETIME; }

  virtual std::wstring getGameDirectory() const;
  // get absolute path to the directory where omo stores its mods
  virtual std::wstring getModsDir() const;
  // get absolute path to the directory where omo stores its profiles
  virtual std::wstring getProfilesDir() const;

  virtual std::wstring getIniFilename() const;
  virtual std::wstring getDownloadDir() const;
  virtual std::wstring getCacheDir() const;
  virtual std::wstring getOverwriteDir() const;
  virtual std::wstring getLogDir() const;
  virtual std::wstring getTutorialDir() const;

  virtual bool requiresBSAInvalidation() const { return true; }
  virtual bool requiresSteam() const;

  virtual std::wstring getInvalidationBSA() = 0;

  virtual bool isInvalidationBSA(const std::wstring &bsaName) = 0;

  // the key in the game's ini-file that defines the list of bsas to load
  virtual std::wstring archiveListKey() = 0;

  virtual std::vector<std::wstring> getPrimaryPlugins() = 0;

  virtual std::vector<std::wstring> getVanillaBSAs() = 0;

  // file name of this games ini file(s)
  virtual std::vector<std::wstring> getIniFileNames() = 0;

  virtual std::wstring getSaveGameExtension() = 0;
  virtual std::wstring getReferenceDataFile() = 0;

  virtual std::wstring getOMODExt() = 0;

  virtual std::vector<std::wstring> getSteamVariants() const;
  virtual std::wstring getSteamAPPId(int variant = 0) const = 0;

  virtual std::wstring getSEName() = 0;

  virtual std::wstring getNexusPage() = 0;
  virtual std::wstring getNexusInfoUrl() = 0;
  virtual int getNexusModID() = 0;

  // clone relevant files to the specified directory
  virtual void createProfile(const std::wstring &directory, bool useDefaults) = 0;
  virtual void repairProfile(const std::wstring &directory) = 0;

  virtual bool rerouteToProfile(const wchar_t *fileName, const wchar_t *fullPath) = 0;

  // get a list of executables (game binary and known-to-work 3rd party tools). All of these are relative to
  // the game directory. the boolean says whether omo should be closed when the executable is started
  virtual std::vector<ExecutableInfo> getExecutables() = 0;

public:

  // initialise with the path to the omo directory (needs to be where hook.dll is stored). This
  // needs to be called before the instance can be retrieved
  static bool init(const std::wstring &omoDirectory, const std::wstring &gamePath = L"");

  static GameInfo& instance();

protected:

  GameInfo(const std::wstring &omoDirectory, const std::wstring &gameDirectory);

  std::wstring getLocalAppFolder() const;
  const std::wstring &getMyGamesDirectory() const { return m_MyGamesDirectory; }
  void identifyMyGamesDirectory(const std::wstring &file);

private:

  static bool identifyGame(const std::wstring &omoDirectory, const std::wstring &searchPath);
  std::wstring getSpecialPath(LPCWSTR name) const;

  static void cleanup();

private:

  static GameInfo *s_Instance;

  std::wstring m_MyGamesDirectory;

  std::wstring m_GameDirectory;
  std::wstring m_OrganizerDirectory;

};


} // namespace MOShared

#endif // GAMEINFO_H
