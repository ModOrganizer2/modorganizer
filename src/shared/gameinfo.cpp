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

namespace MOShared {


GameInfo* GameInfo::s_Instance = NULL;


GameInfo::GameInfo(const std::wstring &omoDirectory, const std::wstring &gameDirectory)
  : m_GameDirectory(gameDirectory), m_OrganizerDirectory(omoDirectory)
{
}


void GameInfo::identifyMyGamesDirectory(const std::wstring &file)
{
  wchar_t myDocuments[MAX_PATH];
  memset(myDocuments, '\0', MAX_PATH);

  ::SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, myDocuments);

  m_MyGamesDirectory.assign(myDocuments).append(L"\\My Games");
  if (!FileExists(m_MyGamesDirectory.substr().append(L"/").append(file))) {
    if (::SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_DEFAULT, myDocuments) != S_OK) {
      m_MyGamesDirectory.assign(myDocuments).append(L"\\My Games");
    }
  }
  if (!FileExists(m_MyGamesDirectory.substr().append(L"/").append(file))) {
    m_MyGamesDirectory.assign(getSpecialPath(L"Personal")).append(L"\\My Games");
  }
}


bool GameInfo::identifyGame(const std::wstring &omoDirectory, const std::wstring &searchPath)
{
  if (OblivionInfo::identifyGame(searchPath)) {
    s_Instance = new OblivionInfo(omoDirectory, searchPath);
  } else if (Fallout3Info::identifyGame(searchPath)) {
    s_Instance = new Fallout3Info(omoDirectory, searchPath);
  } else if (FalloutNVInfo::identifyGame(searchPath)) {
    s_Instance = new FalloutNVInfo(omoDirectory, searchPath);
  } else if (SkyrimInfo::identifyGame(searchPath)) {
    s_Instance = new SkyrimInfo(omoDirectory, searchPath);
  }

  return s_Instance != NULL;
}


bool GameInfo::init(const std::wstring &omoDirectory, const std::wstring &gamePath)
{
  if (s_Instance == NULL) {
    if (gamePath.length() == 0) {
      // search upward in the directory until a recognized game-binary is found
      std::wstring searchPath(omoDirectory);
      while (!identifyGame(omoDirectory, searchPath)) {
        size_t lastSep = searchPath.find_last_of(L"/\\");
        if (lastSep == std::string::npos) {
          return false;
        }
        searchPath.erase(lastSep);
      }
    } else if (!identifyGame(omoDirectory, gamePath)) {
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
  std::wostringstream temp;
  temp << m_OrganizerDirectory << L"\\mods";
  return temp.str();
}

std::wstring GameInfo::getProfilesDir() const
{
  std::wostringstream temp;
  temp << m_OrganizerDirectory << L"\\profiles";
  return temp.str();
}

std::wstring GameInfo::getIniFilename() const
{
  std::wostringstream temp;
  temp << m_OrganizerDirectory << L"\\ModOrganizer.ini";
  return temp.str();
}


std::wstring GameInfo::getDownloadDir() const
{
  std::wostringstream temp;
  temp << m_OrganizerDirectory << L"\\downloads";
  return temp.str();
}


std::wstring GameInfo::getCacheDir() const
{
  std::wostringstream temp;
  temp << m_OrganizerDirectory << L"\\webcache";
  return temp.str();
}


std::wstring GameInfo::getOverwriteDir() const
{
  std::wostringstream temp;
  temp << m_OrganizerDirectory << "\\overwrite";
  return temp.str();
}


std::wstring GameInfo::getLogDir() const
{
  std::wostringstream temp;
  temp << m_OrganizerDirectory << "\\logs";
  return temp.str();
}


std::wstring GameInfo::getTutorialDir() const
{
  std::wostringstream temp;
  temp << m_OrganizerDirectory << "\\tutorials";
  return temp.str();
}


bool GameInfo::requiresSteam() const
{
  return FileExists(getGameDirectory().append(L"\\steam_api.dll"));
}


std::wstring GameInfo::getLocalAppFolder() const
{
  return getSpecialPath(L"Local AppData");
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

  return temp;
}

} // namespace MOShared
