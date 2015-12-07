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

#include "fallout4info.h"
#include "util.h"
#include <tchar.h>
#include <ShlObj.h>
#include <sstream>
#include "windows_error.h"
#include "error_report.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <boost/assign.hpp>

namespace MOShared {

Fallout4Info::Fallout4Info(const std::wstring &moDirectory, const std::wstring &moDataDirectory, const std::wstring &gameDirectory)
  : GameInfo(moDirectory, moDataDirectory, gameDirectory)
{
  identifyMyGamesDirectory(L"fallout4");
}

bool Fallout4Info::identifyGame(const std::wstring &searchPath)
{
  return FileExists(searchPath, L"Fallout4.exe") &&
         FileExists(searchPath, L"Fallout4Launcher.exe");
}

std::wstring Fallout4Info::getRegPathStatic()
{
  HKEY key;
  LONG errorcode = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Bethesda Softworks\\Fallout4",
                                  0, KEY_QUERY_VALUE, &key);

  if (errorcode != ERROR_SUCCESS) {
    return std::wstring();
  }

  WCHAR temp[MAX_PATH];
  DWORD bufferSize = MAX_PATH;

  if (::RegQueryValueExW(key, L"Installed Path", nullptr, nullptr, (LPBYTE)temp, &bufferSize) == ERROR_SUCCESS) {
    return std::wstring(temp);
  } else {
    return std::wstring();
  }
}


std::vector<std::wstring> Fallout4Info::getDLCPlugins()
{
  return std::vector<std::wstring>();
}

std::vector<std::wstring> Fallout4Info::getSavegameAttachmentExtensions()
{
  return std::vector<std::wstring>();
}

std::vector<std::wstring> Fallout4Info::getIniFileNames()
{
  return boost::assign::list_of(L"fallout4.ini")(L"fallout4prefs.ini");
}

std::wstring Fallout4Info::getReferenceDataFile()
{
  return L"Fallout - Meshes.bsa";
}

std::wstring Fallout4Info::getNexusPage(bool nmmScheme)
{
  if (nmmScheme) {
    return L"http://nmm.nexusmods.com/fallout4";
  } else {
    return L"http://www.nexusmods.com/fallout4";
  }
}

std::wstring Fallout4Info::getNexusInfoUrlStatic()
{
  return L"http://nmm.nexusmods.com/fallout4";
}

int Fallout4Info::getNexusModIDStatic()
{
  return 377160;
}

} // namespace MOShared
