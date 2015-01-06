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

#include "skyriminfo.h"

#include "util.h"
#include <tchar.h>
#include <ShlObj.h>
#include <sstream>
#include "windows_error.h"
#include "error_report.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Shlwapi.h>
#include <boost/assign.hpp>

namespace MOShared {


SkyrimInfo::SkyrimInfo(const std::wstring &moDirectory, const std::wstring &moDataDirectory, const std::wstring &gameDirectory)
  : GameInfo(moDirectory, moDataDirectory, gameDirectory)
{
  identifyMyGamesDirectory(L"skyrim");

  wchar_t appDataPath[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appDataPath))) {
    m_AppData = appDataPath;
  }
}

bool SkyrimInfo::identifyGame(const std::wstring &searchPath)
{
  return FileExists(searchPath, L"TESV.exe") &&
         FileExists(searchPath, L"SkyrimLauncher.exe");
}

unsigned long SkyrimInfo::getBSAVersion()
{
  return 0x68;
}


std::wstring SkyrimInfo::getRegPathStatic()
{
  HKEY key;
  LONG errorcode = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Bethesda Softworks\\Skyrim",
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


std::wstring SkyrimInfo::getInvalidationBSA()
{
  return L"Skyrim - Invalidation.bsa";
}

bool SkyrimInfo::isInvalidationBSA(const std::wstring &bsaName)
{
  static LPCWSTR invalidation[] = { L"Skyrim - Invalidation.bsa", nullptr };

  for (int i = 0; invalidation[i] != nullptr; ++i) {
    if (wcscmp(bsaName.c_str(), invalidation[i]) == 0) {
      return true;
    }
  }
  return false;
}


GameInfo::LoadOrderMechanism SkyrimInfo::getLoadOrderMechanism() const
{
  std::wstring fileName = getGameDirectory() + L"\\TESV.exe";

  try {
    VS_FIXEDFILEINFO versionInfo = GetFileVersion(fileName);
    if ((versionInfo.dwFileVersionMS > 0x10004) || // version >= 1.5.x?
        ((versionInfo.dwFileVersionMS == 0x10004) && (versionInfo.dwFileVersionLS >= 0x1A0000))) { // version >= ?.4.26
      return TYPE_PLUGINSTXT;
    } else {
      return TYPE_FILETIME;
    }
  } catch (const std::exception &e) {
    log("TESV.exe is invalid: %s", e.what());
    return TYPE_FILETIME;
  }
}

std::vector<std::wstring> SkyrimInfo::getPrimaryPlugins()
{
  return boost::assign::list_of(L"skyrim.esm")(L"update.esm");
}

std::vector<std::wstring> SkyrimInfo::getVanillaBSAs()
{
  return boost::assign::list_of(L"Skyrim - Misc.bsa")
                               (L"Skyrim - Shaders.bsa")
                               (L"Skyrim - Textures.bsa")
                               (L"HighResTexturePack01.bsa")
                               (L"HighResTexturePack02.bsa")
                               (L"HighResTexturePack03.bsa")
                               (L"Skyrim - Interface.bsa")
                               (L"Skyrim - Animations.bsa")
                               (L"Skyrim - Meshes.bsa")
                               (L"Skyrim - Sounds.bsa")
                               (L"Skyrim - Voices.bsa")
      (L"Skyrim - VoicesExtra.bsa");
}

std::vector<std::wstring> SkyrimInfo::getDLCPlugins()
{
  return boost::assign::list_of (L"Dawnguard.esm")
                                (L"Dragonborn.esm")
                                (L"HearthFires.esm")
                                (L"HighResTexturePack01.esp")
                                (L"HighResTexturePack02.esp")
                                (L"HighResTexturePack03.esp")
      ;
}

std::vector<std::wstring> SkyrimInfo::getSavegameAttachmentExtensions()
{
  return boost::assign::list_of(L"skse");
}

std::vector<std::wstring> SkyrimInfo::getIniFileNames()
{
  return boost::assign::list_of(L"skyrim.ini")(L"skyrimprefs.ini");
}

std::wstring SkyrimInfo::getReferenceDataFile()
{
  return L"Skyrim - Meshes.bsa";
}


std::wstring SkyrimInfo::getOMODExt()
{
  return L"fomod";
}

std::wstring SkyrimInfo::getSEName()
{
  return L"skse";
}


std::wstring SkyrimInfo::getNexusPage(bool nmmScheme)
{
  if (nmmScheme) {
    return L"http://nmm.nexusmods.com/skyrim";
  } else {
    return L"http://www.nexusmods.com/skyrim";
  }
}


std::wstring SkyrimInfo::getNexusInfoUrlStatic()
{
  return L"http://nmm.nexusmods.com/skyrim";
}


int SkyrimInfo::getNexusModIDStatic()
{
  return 1334;
}

bool SkyrimInfo::rerouteToProfile(const wchar_t *fileName, const wchar_t *fullPath)
{
  static LPCWSTR profileFiles[] = { L"skyrim.ini", L"skyrimprefs.ini", L"loadorder.txt", nullptr };

  for (int i = 0; profileFiles[i] != nullptr; ++i) {
    if (_wcsicmp(fileName, profileFiles[i]) == 0) {
      return true;
    }
  }

  if ((_wcsicmp(fileName, L"plugins.txt") == 0) &&
      (m_AppData.empty() || (StrStrIW(fullPath, m_AppData.c_str()) != nullptr))) {
    return true;
  }

  return false;
}


} // namespace MOShared
