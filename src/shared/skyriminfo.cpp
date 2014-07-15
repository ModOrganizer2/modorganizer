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


SkyrimInfo::SkyrimInfo(const std::wstring &moDirectory, const std::wstring &gameDirectory)
  : GameInfo(moDirectory, gameDirectory)
{
  identifyMyGamesDirectory(L"skyrim");

  wchar_t appDataPath[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appDataPath))) {
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
    return L"";
  }

  WCHAR temp[MAX_PATH];
  DWORD bufferSize = MAX_PATH;

  errorcode = ::RegQueryValueExW(key, L"Installed Path", NULL, NULL, (LPBYTE)temp, &bufferSize);

  return std::wstring(temp);
}


std::wstring SkyrimInfo::getInvalidationBSA()
{
  return L"Skyrim - Invalidation.bsa";
}

bool SkyrimInfo::isInvalidationBSA(const std::wstring &bsaName)
{
  static LPCWSTR invalidation[] = { L"Skyrim - Invalidation.bsa", NULL };

  for (int i = 0; invalidation[i] != NULL; ++i) {
    if (wcscmp(bsaName.c_str(), invalidation[i]) == 0) {
      return true;
    }
  }
  return false;
}


GameInfo::LoadOrderMechanism SkyrimInfo::getLoadOrderMechanism() const
{
  std::wstring fileName = getGameDirectory().append(L"\\TESV.exe");

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

std::wstring SkyrimInfo::getDocumentsDir()
{
  std::wostringstream temp;
  temp << getMyGamesDirectory() << L"\\Skyrim";

  return temp.str();
}

std::wstring SkyrimInfo::getSaveGameDir()
{
  std::wostringstream temp;
  temp << getDocumentsDir() << L"\\Saves";
  return temp.str();
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

std::wstring SkyrimInfo::getSaveGameExtension()
{
  return L"*.ess";
}

std::wstring SkyrimInfo::getReferenceDataFile()
{
  return L"Skyrim - Meshes.bsa";
}


std::wstring SkyrimInfo::getOMODExt()
{
  return L"fomod";
}


std::wstring SkyrimInfo::getSteamAPPId(int) const
{
  return L"72850";
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


void SkyrimInfo::createProfile(const std::wstring &directory, bool useDefaults)
{
  { // copy plugins.txt
    std::wstring target = directory.substr().append(L"\\plugins.txt");
    if (!FileExists(target)) {
      std::wostringstream source;
      source << getLocalAppFolder() << "\\Skyrim\\plugins.txt";
      if (!::CopyFileW(source.str().c_str(), target.c_str(), true)) {
        HANDLE file = ::CreateFileW(target.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        ::CloseHandle(file);
      }
    }
    target = directory.substr().append(L"\\loadorder.txt");
    if (!FileExists(target)) {
      std::wostringstream source;
      source << getLocalAppFolder() << "\\Skyrim\\loadorder.txt";
      if (!::CopyFileW(source.str().c_str(), target.c_str(), true)) {
        HANDLE file = ::CreateFileW(target.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        ::CloseHandle(file);
      }
    }
  }

  { // copy skyrim.ini-file
    std::wstring target = directory.substr().append(L"\\skyrim.ini");
    if (!FileExists(target)) {
      std::wostringstream source;
      if (useDefaults) {
        source << getGameDirectory() << L"\\skyrim_default.ini";
      } else {
        source << getMyGamesDirectory() << L"\\Skyrim";
        if (FileExists(source.str(), L"skyrim.ini")) {
          source << L"\\skyrim.ini";
        } else {
          source.str(L"");
          source << getGameDirectory() << L"\\skyrim_default.ini";
        }
      }
      if (!::CopyFileW(source.str().c_str(), target.c_str(), true)) {
        if (::GetLastError() != ERROR_FILE_EXISTS) {
          std::ostringstream stream;
          stream << "failed to copy ini file: " << ToString(source.str(), false);
          throw windows_error(stream.str());
        }
      }
    }
  }

  { // copy skyrimprefs.ini-file
    std::wstring target = directory.substr().append(L"\\skyrimprefs.ini");
    if (!FileExists(target)) {
      std::wostringstream source;
      source << getMyGamesDirectory() << L"\\Skyrim\\skyrimprefs.ini";
      if (!::CopyFileW(source.str().c_str(), target.c_str(), true)) {
        if (::GetLastError() != ERROR_FILE_EXISTS) {
          std::ostringstream stream;
          stream << "failed to copy ini file: " << ToString(source.str(), false);
          throw windows_error(stream.str());
        }
      }
    }
  }
}


void SkyrimInfo::repairProfile(const std::wstring &directory)
{
  createProfile(directory, false);
}


bool SkyrimInfo::rerouteToProfile(const wchar_t *fileName, const wchar_t *fullPath)
{
  static LPCWSTR profileFiles[] = { L"skyrim.ini", L"skyrimprefs.ini", L"loadorder.txt", NULL };

  for (int i = 0; profileFiles[i] != NULL; ++i) {
    if (_wcsicmp(fileName, profileFiles[i]) == 0) {
      return true;
    }
  }

  if ((_wcsicmp(fileName, L"plugins.txt") == 0) &&
      (m_AppData.empty() || (StrStrIW(fullPath, m_AppData.c_str()) != NULL))) {
    return true;
  }

  return false;
}


std::vector<ExecutableInfo> SkyrimInfo::getExecutables()
{
  std::vector<ExecutableInfo> result;
  result.push_back(ExecutableInfo(L"SKSE", L"skse_loader.exe", L"", L"", DEFAULT_CLOSE));
  result.push_back(ExecutableInfo(L"SBW", L"SBW.exe", L"", L"", DEFAULT_CLOSE));
  result.push_back(ExecutableInfo(L"Skyrim", L"TESV.exe", L"", L"", DEFAULT_CLOSE));
  result.push_back(ExecutableInfo(L"Skyrim Launcher", L"SkyrimLauncher.exe", L"", L"", DEFAULT_CLOSE));
  result.push_back(ExecutableInfo(L"BOSS", L"BOSS/BOSS.exe", L"", L"", DEFAULT_STAY));
  result.push_back(ExecutableInfo(L"Creation Kit", L"CreationKit.exe", L"", L"", DEFAULT_STAY, L"202480"));

  return result;
}

} // namespace MOShared
