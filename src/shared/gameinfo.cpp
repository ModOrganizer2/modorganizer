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

#include "gameinfo.h"

#include "windows_error.h"

#include "oblivioninfo.h"
#include "fallout3info.h"
#include "falloutnvinfo.h"
#include "skyriminfo.h"
#include "util.h"

#include <shlobj.h>
#include <sstream>
#include <cassert>
#include <boost/assign.hpp>

namespace MOShared {


GameInfo* GameInfo::s_Instance = NULL;


GameInfo::GameInfo(const std::wstring &omoDirectory, const std::wstring &gameDirectory)
  : m_GameDirectory(gameDirectory), m_OrganizerDirectory(omoDirectory)
{
  atexit(&cleanup);
}


void GameInfo::cleanup() {
  delete GameInfo::s_Instance;
  GameInfo::s_Instance = NULL;
}


void GameInfo::identifyMyGamesDirectory(const std::wstring &file)
{
  // this function attempts 3 (three!) ways to determine the correct "My Games" folder.
  wchar_t myDocuments[MAX_PATH];
  memset(myDocuments, '\0', MAX_PATH * sizeof(wchar_t));

  m_MyGamesDirectory.clear();

  // a) this is the way it should work. get the configured My Documents\My Games directory
  if (::SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, myDocuments) == S_OK) {
    m_MyGamesDirectory = std::wstring(myDocuments) + L"\\My Games";
  }

  // b) if there is no <game> directory there, look in the default directory
  if (m_MyGamesDirectory.empty()
      || !FileExists(m_MyGamesDirectory + L"\\" + file)) {
    if (::SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_DEFAULT, myDocuments) == S_OK) {
      std::wstring fromDefault = std::wstring(myDocuments) + L"\\My Games";
      if (FileExists(fromDefault + L"\\" + file)) {
        m_MyGamesDirectory = fromDefault;
      }
    }
  }
  if (!FileExists(m_MyGamesDirectory.substr().append(L"/").append(file))) {
    m_MyGamesDirectory.assign(getSpecialPath(L"Personal")).append(L"\\My Games");
  }
}


bool GameInfo::identifyGame(const std::wstring &moDirectory, const std::wstring &searchPath)
{
  if (OblivionInfo::identifyGame(searchPath)) {
    s_Instance = new OblivionInfo(moDirectory, searchPath);
  } else if (Fallout3Info::identifyGame(searchPath)) {
    s_Instance = new Fallout3Info(moDirectory, searchPath);
  } else if (FalloutNVInfo::identifyGame(searchPath)) {
    s_Instance = new FalloutNVInfo(moDirectory, searchPath);
  } else if (SkyrimInfo::identifyGame(searchPath)) {
    s_Instance = new SkyrimInfo(moDirectory, searchPath);
  }

  return s_Instance != NULL;
}


bool GameInfo::init(const std::wstring &moDirectory, const std::wstring &gamePath)
{
  if (s_Instance == NULL) {
    if (gamePath.length() == 0) {
      // search upward in the directory until a recognized game-binary is found
      std::wstring searchPath(moDirectory);
      while (!identifyGame(moDirectory, searchPath)) {
        size_t lastSep = searchPath.find_last_of(L"/\\");
        if (lastSep == std::string::npos) {
          return false;
        }
        searchPath.erase(lastSep);
      }
    } else if (!identifyGame(moDirectory, gamePath)) {
      return false;
    }
  }
  return true;
}


GameInfo &GameInfo::instance()
{
  assert(s_Instance != NULL);
  return *s_Instance;
}

std::wstring GameInfo::getGameDirectory() const
{
  return m_GameDirectory;
}

std::wstring GameInfo::getModsDir() const
{
  return m_OrganizerDirectory + L"\\mods";
}

std::wstring GameInfo::getProfilesDir() const
{
  return m_OrganizerDirectory + L"\\profiles";
}

std::wstring GameInfo::getIniFilename() const
{
  return m_OrganizerDirectory + L"\\ModOrganizer.ini";
}


std::wstring GameInfo::getDownloadDir() const
{
  return m_OrganizerDirectory + L"\\downloads";
}


std::wstring GameInfo::getCacheDir() const
{
  return m_OrganizerDirectory + L"\\webcache";
}


std::wstring GameInfo::getOverwriteDir() const
{
  return m_OrganizerDirectory + L"\\overwrite";
}


std::wstring GameInfo::getLogDir() const
{
  return m_OrganizerDirectory + L"\\logs";
}


std::wstring GameInfo::getLootDir() const
{
  return m_OrganizerDirectory + L"\\loot";
}


std::wstring GameInfo::getTutorialDir() const
{
  return m_OrganizerDirectory + L"\\tutorials";
}


bool GameInfo::requiresSteam() const
{
  return FileExists(getGameDirectory().append(L"\\steam_api.dll"));
}

std::vector<std::wstring> GameInfo::getSteamVariants() const
{
  return boost::assign::list_of(L"Regular");
}


std::wstring GameInfo::getLocalAppFolder() const
{
  wchar_t localAppFolder[MAX_PATH];
  memset(localAppFolder, '\0', MAX_PATH * sizeof(wchar_t));

  if (::SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, localAppFolder) == S_OK) {
    return localAppFolder;
  } else {
    // fallback: try the registry
    return getSpecialPath(L"Local AppData");
  }
}

std::wstring GameInfo::getSpecialPath(LPCWSTR name) const
{
  HKEY key;
  LONG errorcode = ::RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\User Shell Folders",
                                  0, KEY_QUERY_VALUE, &key);

  if (errorcode != ERROR_SUCCESS) {
    throw windows_error("failed to look up special folder", errorcode);
  }

  WCHAR temp[MAX_PATH];
  DWORD bufferSize = MAX_PATH;

  errorcode = ::RegQueryValueExW(key, name, NULL, NULL, (LPBYTE)temp, &bufferSize);
  if (errorcode != ERROR_SUCCESS) {
    throw windows_error("failed to look up special folder", errorcode);
  }

  WCHAR temp2[MAX_PATH];
  // try to expand variables in the path, if any
  if (::ExpandEnvironmentStringsW(temp, temp2, MAX_PATH) != 0) {
    return temp2;
  } else {
    return temp;
  }
}

} // namespace MOShared
