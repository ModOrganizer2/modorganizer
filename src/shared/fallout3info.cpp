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

#include "fallout3info.h"
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

Fallout3Info::Fallout3Info(const std::wstring &moDirectory, const std::wstring &moDataDirectory, const std::wstring &gameDirectory)
  : GameInfo(moDirectory, moDataDirectory, gameDirectory)
{
  identifyMyGamesDirectory(L"fallout3");
}

bool Fallout3Info::identifyGame(const std::wstring &searchPath)
{
  return FileExists(searchPath, L"Fallout3.exe") &&
         FileExists(searchPath, L"FalloutLauncher.exe");
}

unsigned long Fallout3Info::getBSAVersion()
{
  return 0x68;
}

std::wstring Fallout3Info::getRegPathStatic()
{
  HKEY key;
  LONG errorcode = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Bethesda Softworks\\Fallout3",
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

std::wstring Fallout3Info::getInvalidationBSA()
{
  return L"Fallout - Invalidation.bsa";
}

bool Fallout3Info::isInvalidationBSA(const std::wstring &bsaName)
{
  static LPCWSTR invalidation[] = { L"Fallout - AI!.bsa", L"Fallout - Invalidation.bsa", nullptr };

  for (int i = 0; invalidation[i] != nullptr; ++i) {
    if (wcscmp(bsaName.c_str(), invalidation[i]) == 0) {
      return true;
    }
  }
  return false;
}

std::wstring Fallout3Info::getDocumentsDir()
{
  std::wostringstream temp;
  temp << getMyGamesDirectory() << L"\\Fallout3";

  return temp.str();
}

std::wstring Fallout3Info::getSaveGameDir()
{
  std::wostringstream temp;
  temp << getDocumentsDir() << L"\\Saves";
  return temp.str();
}

std::vector<std::wstring> Fallout3Info::getPrimaryPlugins()
{
  return boost::assign::list_of(L"fallout3.esm");
}

std::vector<std::wstring> Fallout3Info::getVanillaBSAs()
{
  return boost::assign::list_of (L"Fallout - Textures.bsa")
                                (L"Fallout - Meshes.bsa")
                                (L"Fallout - Voices.bsa")
                                (L"Fallout - Sound.bsa")
                                (L"Fallout - MenuVoices.bsa")
                                (L"Fallout - Misc.bsa");
}

std::vector<std::wstring> Fallout3Info::getDLCPlugins()
{
  return boost::assign::list_of (L"ThePitt.esm")
                                (L"Anchorage.esm")
                                (L"BrokenSteel.esm")
                                (L"PointLookout.esm")
                                (L"Zeta.esm")
      ;
}

std::vector<std::wstring> Fallout3Info::getSavegameAttachmentExtensions()
{
  return std::vector<std::wstring>();
}

std::vector<std::wstring> Fallout3Info::getIniFileNames()
{
  return boost::assign::list_of(L"fallout.ini")(L"falloutprefs.ini");
}

std::wstring Fallout3Info::getSaveGameExtension()
{
  return L"*.fos";
}

std::wstring Fallout3Info::getReferenceDataFile()
{
  return L"Fallout - Meshes.bsa";
}


std::wstring Fallout3Info::getOMODExt()
{
  return L"fomod";
}

std::vector<std::wstring> Fallout3Info::getSteamVariants() const
{
  return boost::assign::list_of(L"Regular")(L"Game Of The Year");
}

std::wstring Fallout3Info::getSteamAPPId(int variant) const
{
  switch (variant) {
    case 1:  return L"22370";
    default: return L"22300";
  }
}


std::wstring Fallout3Info::getSEName()
{
  return L"fose";
}


std::wstring Fallout3Info::getNexusPage(bool nmmScheme)
{
  if (nmmScheme) {
    return L"http://nmm.nexusmods.com/fallout3";
  } else {
    return L"http://www.nexusmods.com/fallout3";
  }
}


std::wstring Fallout3Info::getNexusInfoUrlStatic()
{
  return L"http://nmm.nexusmods.com/fallout3";
}


int Fallout3Info::getNexusModIDStatic()
{
  return 16348;
}


void Fallout3Info::createProfile(const std::wstring &directory, bool useDefaults)
{
  std::wostringstream target;

  // copy plugins.txt
  target << directory << "\\plugins.txt";

  if (!FileExists(target.str())) {
    std::wostringstream source;
    source << getLocalAppFolder() << "\\Fallout3\\plugins.txt";
    if (!::CopyFileW(source.str().c_str(), target.str().c_str(), true)) {
      HANDLE file = ::CreateFileW(target.str().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
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
      source << getMyGamesDirectory() << L"\\Fallout3";
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
    std::wstring target = directory + L"\\falloutprefs.ini";
    if (!FileExists(target)) {
      std::wstring source = getMyGamesDirectory() + L"\\Fallout3\\falloutprefs.ini";
      if (!::CopyFileW(source.c_str(), target.c_str(), true)) {
        if (::GetLastError() != ERROR_FILE_EXISTS) {
          throw windows_error(std::string("failed to copy ini file: ") + ToString(source, false));
        }
      }
    }
  }
}


void Fallout3Info::repairProfile(const std::wstring &directory)
{
  createProfile(directory, false);
}


bool Fallout3Info::rerouteToProfile(const wchar_t *fileName, const wchar_t*)
{
  static LPCWSTR profileFiles[] = { L"fallout.ini", L"falloutprefs.ini", L"plugins.txt", nullptr };

  for (int i = 0; profileFiles[i] != nullptr; ++i) {
    if (_wcsicmp(fileName, profileFiles[i]) == 0) {
      return true;
    }
  }
  return false;
}


/*std::vector<ExecutableInfo> Fallout3Info::getExecutables()
{
  std::vector<ExecutableInfo> result;
  result.push_back(ExecutableInfo(L"FOSE", L"fose_loader.exe", L"", L"", DEFAULT_CLOSE));
  result.push_back(ExecutableInfo(L"Fallout 3", L"fallout3.exe", L"", L"", DEFAULT_CLOSE));
  result.push_back(ExecutableInfo(L"Fallout Mod Manager", L"fomm/fomm.exe", L"", L"", DEFAULT_CLOSE));
  result.push_back(ExecutableInfo(L"Construction Kit", L"geck.exe", L"", L"", DEFAULT_CLOSE));
  result.push_back(ExecutableInfo(L"Fallout Launcher", L"FalloutLauncher.exe", L"", L"", DEFAULT_CLOSE));
  result.push_back(ExecutableInfo(L"BOSS", L"BOSS/BOSS.exe", L"", L"", NEVER_CLOSE));

  return result;
}*/
} // namespace MOShared
