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

#include "oblivioninfo.h"
#include <tchar.h>
#include <ShlObj.h>
#include "util.h"
#include <sstream>
#include "windows_error.h"
#include "error_report.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <boost/assign.hpp>

namespace MOShared {


OblivionInfo::OblivionInfo(const std::wstring &omoDirectory, const std::wstring &gameDirectory)
  : GameInfo(omoDirectory, gameDirectory)
{
  identifyMyGamesDirectory(L"oblivion");
}

bool OblivionInfo::identifyGame(const std::wstring &searchPath)
{
  return FileExists(searchPath, L"Oblivion.exe") &&
         FileExists(searchPath, L"OblivionLauncher.exe");
}

unsigned long OblivionInfo::getBSAVersion()
{
  return 0x67;
}

std::wstring OblivionInfo::getRegPathStatic()
{
  HKEY key;
  LONG errorcode = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Bethesda Softworks\\Oblivion",
                                  0, KEY_QUERY_VALUE, &key);

  if (errorcode != ERROR_SUCCESS) {
    return L"";
  }

  WCHAR temp[MAX_PATH];
  DWORD bufferSize = MAX_PATH;

  errorcode = ::RegQueryValueExW(key, L"Installed Path", NULL, NULL, (LPBYTE)temp, &bufferSize);

  return std::wstring(temp);
}

std::wstring OblivionInfo::getInvalidationBSA()
{
  return L"Oblivion - Invalidation.bsa";
}

bool OblivionInfo::isInvalidationBSA(const std::wstring &bsaName)
{
  static LPCWSTR invalidation[] = { L"Oblivion - Invalidation.bsa", L"ArchiveInvalidationInvalidated!.bsa",
                                    L"BSARedirection.bsa", NULL };

  for (int i = 0; invalidation[i] != NULL; ++i) {
    if (wcscmp(bsaName.c_str(), invalidation[i]) == 0) {
      return true;
    }
  }
  return false;
}

std::wstring OblivionInfo::getDocumentsDir()
{
  std::wostringstream temp;
  temp << getMyGamesDirectory() << L"\\Oblivion";

  return temp.str();
}

std::wstring OblivionInfo::getSaveGameDir()
{
  std::wostringstream temp;
  temp << getDocumentsDir() << L"\\Saves";
  return temp.str();
}


std::vector<std::wstring> OblivionInfo::getPrimaryPlugins()
{
  return boost::assign::list_of(L"oblivion.esm");
}


std::vector<std::wstring> OblivionInfo::getVanillaBSAs()
{
  return boost::assign::list_of(L"Oblivion - Meshes.bsa")
                               (L"Oblivion - Textures - Compressed.bsa")
                               (L"Oblivion - Sounds.bsa")
                               (L"Oblivion - Voices1.bsa")
                               (L"Oblivion - Voices2.bsa")
                               (L"Oblivion - Misc.bsa");
}


std::vector<std::wstring> OblivionInfo::getDLCPlugins()
{
  return boost::assign::list_of (L"DLCShiveringIsles.esp")
                                (L"Knights.esp")
                                (L"DLCFrostcrag.esp")
                                (L"DLCSpellTomes.esp")
                                (L"DLCMehrunesRazor.esp")
                                (L"DLCOrrery.esp")
                                (L"DLCSpellTomes.esp")
                                (L"DLCThievesDen.esp")
                                (L"DLCVileLair.esp")
                                (L"DLCHorseArmor.esp")
      ;
}


std::vector<std::wstring> OblivionInfo::getSavegameAttachmentExtensions()
{
  return boost::assign::list_of(L"obse");
}


std::vector<std::wstring> OblivionInfo::getIniFileNames()
{
  return boost::assign::list_of(L"oblivion.ini")(L"oblivionprefs.ini");
}


void OblivionInfo::createProfile(const std::wstring &directory, bool useDefaults)
{
  std::wostringstream target;

  // copy plugins.txt
  target << directory << "\\plugins.txt";

  if (!FileExists(target.str())) {
    std::wostringstream source;
    source << getLocalAppFolder() << "\\Oblivion\\plugins.txt";
    if (!::CopyFileW(source.str().c_str(), target.str().c_str(), true)) {
      HANDLE file = ::CreateFileW(target.str().c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
      ::CloseHandle(file);
    }
  }

  // copy ini-file
  target.str(L""); target.clear();
  target << directory << L"\\oblivion.ini";

  if (!FileExists(target.str())) {
    std::wostringstream source;
    if (useDefaults) {
      source << getGameDirectory() << L"\\oblivion_default.ini";
    } else {
      source << getMyGamesDirectory() << L"Oblivion";
      if (FileExists(source.str(), L"oblivion.ini")) {
        source << L"\\oblivion.ini";
      } else {
        source.str(L"");
        source << getGameDirectory() << L"\\oblivion_default.ini";
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

  { // copy oblivionprefs.ini-file
    std::wstring target = directory.substr().append(L"\\oblivionprefs.ini");
    if (!FileExists(target)) {
      std::wostringstream source;
      source << getMyGamesDirectory() << L"\\Oblivion\\oblivionprefs.ini";
      if (!::CopyFileW(source.str().c_str(), target.c_str(), true)) {
        if ((::CreateFileW(target.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL) == INVALID_HANDLE_VALUE) &&
            (::GetLastError() != ERROR_FILE_EXISTS)) {
          std::ostringstream stream;
          stream << "failed to create ini file: " << ToString(target.c_str(), false);
          throw windows_error(stream.str());
        }
      }
    }
  }
}


std::wstring OblivionInfo::getSEName()
{
  return L"obse";
}


std::wstring OblivionInfo::getNexusPage(bool nmmScheme)
{
  if (nmmScheme) {
    return L"http://nmm.nexusmods.com/oblivion";
  } else {
    return L"http://www.nexusmods.com/oblivion";
  }
}


std::wstring OblivionInfo::getNexusInfoUrlStatic()
{
  return L"http://nmm.nexusmods.com/oblivion";
}


int OblivionInfo::getNexusModIDStatic()
{
  return 38277;
}


void OblivionInfo::repairProfile(const std::wstring &directory)
{
  createProfile(directory, false);
}


bool OblivionInfo::rerouteToProfile(const wchar_t *fileName, const wchar_t*)
{
  static LPCWSTR profileFiles[] = { L"oblivion.ini", L"oblivionprefs.ini", L"plugins.txt", NULL };

  for (int i = 0; profileFiles[i] != NULL; ++i) {
    if (_wcsicmp(fileName, profileFiles[i]) == 0) {
      return true;
    }
  }
  return false;
}


std::wstring OblivionInfo::getSaveGameExtension()
{
  return L"*.ess";
}

std::wstring OblivionInfo::getReferenceDataFile()
{
  return L"Oblivion - Meshes.bsa";
}


std::wstring OblivionInfo::getOMODExt()
{
  return L"omod";
}


std::wstring OblivionInfo::getSteamAPPId(int) const
{
  return L"22330";
}


std::vector<ExecutableInfo> OblivionInfo::getExecutables()
{
  std::vector<ExecutableInfo> result;
  result.push_back(ExecutableInfo(L"OBSE", L"obse_loader.exe", L"", L"", DEFAULT_CLOSE));
  result.push_back(ExecutableInfo(L"Oblivion", L"oblivion.exe", L"", L"", DEFAULT_CLOSE));
  result.push_back(ExecutableInfo(L"Oblivion Mod Manager", L"OblivionModManager.exe", L"", L"", DEFAULT_CLOSE));
  result.push_back(ExecutableInfo(L"Construction Set", L"TESConstructionSet.exe", L"", L"", DEFAULT_CLOSE));
  result.push_back(ExecutableInfo(L"Oblivion Launcher", L"OblivionLauncher.exe", L"", L"", DEFAULT_CLOSE));
  result.push_back(ExecutableInfo(L"BOSS", L"BOSS/BOSS.exe", L"", L"", NEVER_CLOSE));
  result.push_back(ExecutableInfo(L"BOSS (old)", L"Data/BOSS.exe", L"", L"", NEVER_CLOSE));

  return result;
}
} // namespace MOShared
