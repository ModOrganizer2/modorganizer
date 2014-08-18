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

#include "falloutnvinfo.h"
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


FalloutNVInfo::FalloutNVInfo(const std::wstring &moDirectory, const std::wstring &moDataDirectory, const std::wstring &gameDirectory)
  : GameInfo(moDirectory, moDataDirectory, gameDirectory)
{
  identifyMyGamesDirectory(L"falloutnv");
}

bool FalloutNVInfo::identifyGame(const std::wstring &searchPath)
{
  return FileExists(searchPath, L"FalloutNV.exe") &&
         FileExists(searchPath, L"FalloutNVLauncher.exe");
}

unsigned long FalloutNVInfo::getBSAVersion()
{
  return 0x68;
}

std::wstring FalloutNVInfo::getRegPathStatic()
{
  HKEY key;
  LONG errorcode = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Bethesda Softworks\\FalloutNV",
                                  0, KEY_QUERY_VALUE, &key);

  if (errorcode != ERROR_SUCCESS) {
    return L"";
  }

  WCHAR temp[MAX_PATH];
  DWORD bufferSize = MAX_PATH;

  errorcode = ::RegQueryValueExW(key, L"Installed Path", NULL, NULL, (LPBYTE)temp, &bufferSize);

  return std::wstring(temp);
}

std::wstring FalloutNVInfo::getInvalidationBSA()
{
  return L"Fallout - Invalidation.bsa";
}

bool FalloutNVInfo::isInvalidationBSA(const std::wstring &bsaName)
{
  static LPCWSTR invalidation[] = { L"Fallout - AI!.bsa", L"Fallout - Invalidation.bsa", NULL };

  for (int i = 0; invalidation[i] != NULL; ++i) {
    if (wcscmp(bsaName.c_str(), invalidation[i]) == 0) {
      return true;
    }
  }
  return false;
}

std::wstring FalloutNVInfo::getDocumentsDir()
{
  std::wostringstream temp;
  temp << getMyGamesDirectory() << L"\\FalloutNV";

  return temp.str();
}

std::wstring FalloutNVInfo::getSaveGameDir()
{
  std::wostringstream temp;
  temp << getDocumentsDir() << L"\\Saves";
  return temp.str();
}

std::vector<std::wstring> FalloutNVInfo::getPrimaryPlugins()
{
  return boost::assign::list_of(L"falloutnv.esm");
}

std::vector<std::wstring> FalloutNVInfo::getVanillaBSAs()
{
  return boost::assign::list_of (L"Fallout - Textures.bsa")
                                (L"Fallout - Textures2.bsa")
                                (L"Fallout - Meshes.bsa")
                                (L"Fallout - Voices1.bsa")
                                (L"Fallout - Sound.bsa")
                                (L"Fallout - Misc.bsa");
}

std::vector<std::wstring> FalloutNVInfo::getDLCPlugins()
{
  return boost::assign::list_of (L"DeadMoney.esm")
                                (L"HonestHearts.esm")
                                (L"OldWorldBlues.esm")
                                (L"LonesomeRoad.esm")
                                (L"GunRunnersArsenal.esm")
                                (L"CaravanPack.esm")
                                (L"ClassicPack.esm")
                                (L"MercenaryPack.esm")
                                (L"TribalPack.esm")
      ;
}

std::vector<std::wstring> FalloutNVInfo::getSavegameAttachmentExtensions()
{
  return std::vector<std::wstring>();
}

std::vector<std::wstring> FalloutNVInfo::getIniFileNames()
{
  return boost::assign::list_of(L"fallout.ini")(L"falloutprefs.ini");
}

std::wstring FalloutNVInfo::getSaveGameExtension()
{
  return L"*.fos";
}

std::wstring FalloutNVInfo::getReferenceDataFile()
{
  return L"Fallout - Meshes.bsa";
}


std::wstring FalloutNVInfo::getOMODExt()
{
  return L"fomod";
}


std::wstring FalloutNVInfo::getSteamAPPId(int) const
{
  return L"22380";
}


void FalloutNVInfo::createProfile(const std::wstring &directory, bool useDefaults)
{
  std::wostringstream target;

  // copy plugins.txt
  target << directory << "\\plugins.txt";

  if (!FileExists(target.str())) {
    std::wostringstream source;
    source << getLocalAppFolder() << "\\FalloutNV\\plugins.txt";
    if (!::CopyFileW(source.str().c_str(), target.str().c_str(), true)) {
      HANDLE file = ::CreateFileW(target.str().c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
      ::CloseHandle(file);
    }
  }

  // copy ini-file
  target.str(L""); target.clear();
  target << directory << L"\\fallout.ini";

  if (!FileExists(target.str())) {
    std::wostringstream source;
    if (useDefaults) {
      source << getGameDirectory() << L"\\fallout_default.ini";
    } else {
      source << getMyGamesDirectory() << L"\\FalloutNV";
      if (FileExists(source.str(), L"fallout.ini")) {
        source << L"\\fallout.ini";
      } else {
        source.str(L"");
        source << getGameDirectory() << L"\\fallout_default.ini";
      }
    }

    if (!::CopyFileW(source.str().c_str(), target.str().c_str(), true)) {
      if (::GetLastError() != ERROR_FILE_EXISTS) {
        std::ostringstream stream;
        stream << "failed to copy ini file: " << ToString(source.str(), false);
        throw windows_error(stream.str());
      }
    }
  }
  { // copy falloutprefs.ini-file
    std::wstring target = directory.substr().append(L"\\falloutprefs.ini");
    if (!FileExists(target)) {
      std::wostringstream source;
      source << getMyGamesDirectory() << L"\\FalloutNV\\falloutprefs.ini";
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


std::wstring FalloutNVInfo::getSEName()
{
  return L"nvse";
}


std::wstring FalloutNVInfo::getNexusPage(bool nmmScheme)
{
  if (nmmScheme) {
    return L"http://nmm.nexusmods.com/newvegas";
  } else {
    return L"http://www.nexusmods.com/newvegas";
  }
}


std::wstring FalloutNVInfo::getNexusInfoUrlStatic()
{
  return L"http://nmm.nexusmods.com/newvegas";
}


int FalloutNVInfo::getNexusModIDStatic()
{
  return 42572;
}


void FalloutNVInfo::repairProfile(const std::wstring &directory)
{
  createProfile(directory, false);
}


bool FalloutNVInfo::rerouteToProfile(const wchar_t *fileName, const wchar_t*)
{
  static LPCWSTR profileFiles[] = { L"fallout.ini", L"falloutprefs.ini", L"plugins.txt", NULL };

  for (int i = 0; profileFiles[i] != NULL; ++i) {
    if (_wcsicmp(fileName, profileFiles[i]) == 0) {
      return true;
    }
  }
  return false;
}


std::vector<ExecutableInfo> FalloutNVInfo::getExecutables()
{
  std::vector<ExecutableInfo> result;
  result.push_back(ExecutableInfo(L"NVSE", L"nvse_loader.exe", L"", L"", DEFAULT_CLOSE));
  result.push_back(ExecutableInfo(L"New Vegas", L"falloutnv.exe", L"", L"", DEFAULT_CLOSE));
  result.push_back(ExecutableInfo(L"Fallout Mod Manager", L"fomm/fomm.exe", L"", L"", DEFAULT_CLOSE));
  result.push_back(ExecutableInfo(L"Construction Kit", L"geck.exe", L"", L"", DEFAULT_CLOSE));
  result.push_back(ExecutableInfo(L"Fallout Launcher", L"FalloutNVLauncher.exe", L"", L"", DEFAULT_CLOSE));
  result.push_back(ExecutableInfo(L"BOSS", L"BOSS/BOSS.exe", L"", L"", NEVER_CLOSE));

  return result;
}
} // namespace MOShared
