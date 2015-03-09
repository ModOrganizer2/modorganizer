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

#ifndef OBLIVIONINFO_H
#define OBLIVIONINFO_H

#include "gameinfo.h"

namespace MOShared {

class OblivionInfo : public GameInfo
{

  friend class GameInfo;

public:

  virtual ~OblivionInfo() {}

  static std::wstring getRegPathStatic();
  virtual std::wstring getRegPath() { return getRegPathStatic(); }
  virtual std::wstring getBinaryName() { return L"Oblivion.exe"; }

  virtual GameInfo::Type getType() { return TYPE_OBLIVION; }

  virtual std::wstring getGameName() const { return L"Oblivion"; }
  virtual std::wstring getGameShortName() const { return L"Oblivion"; }
/*
  virtual std::wstring getInvalidationBSA()
  {
    return L"Oblivion - Invalidation.bsa";
  }

  virtual bool isInvalidationBSA(const std::wstring &bsaName)
  {
    static LPCWSTR invalidation[] = { L"Oblivion - Invalidation.bsa", L"ArchiveInvalidationInvalidated!.bsa",
                                      L"BSARedirection.bsa", nullptr };

    for (int i = 0; invalidation[i] != nullptr; ++i) {
      if (wcscmp(bsaName.c_str(), invalidation[i]) == 0) {
        return true;
      }
    }
    return false;
  }

  virtual std::vector<std::wstring> getVanillaBSAs()
  {
    return boost::assign::list_of(L"Oblivion - Meshes.bsa")
        (L"Oblivion - Textures - Compressed.bsa")
        (L"Oblivion - Sounds.bsa")
        (L"Oblivion - Voices1.bsa")
        (L"Oblivion - Voices2.bsa")
        (L"Oblivion - Misc.bsa");
  }

  virtual std::vector<std::wstring> getPrimaryPlugins()
  {
    return boost::assign::list_of(L"oblivion.esm");
  }*/

  virtual std::vector<std::wstring> getDLCPlugins();
  virtual std::vector<std::wstring> getSavegameAttachmentExtensions();

  // file name of this games ini (no path)
  virtual std::vector<std::wstring> getIniFileNames();

  virtual std::wstring getReferenceDataFile();

  virtual std::wstring getNexusPage(bool nmmScheme = true);
  static std::wstring getNexusInfoUrlStatic();
  virtual std::wstring getNexusInfoUrl() { return getNexusInfoUrlStatic(); }
  static int getNexusModIDStatic();
  virtual int getNexusModID() { return getNexusModIDStatic(); }
  virtual int getNexusGameID() { return 101; }

  virtual bool rerouteToProfile(const wchar_t *fileName, const wchar_t *fullPath);

  // get a list of executables (game binary and known-to-work 3rd party tools). All of these are relative to
  // the game directory
  //virtual std::vector<ExecutableInfo> getExecutables();

  virtual std::wstring archiveListKey() { return L"SArchiveList"; }

private:

  OblivionInfo(const std::wstring &moDirectory, const std::wstring &moDataDirectory, const std::wstring &gameDirectory);

  static bool identifyGame(const std::wstring &searchPath);

};

} // namespace MOShared

#endif // OBLIVIONINFO_H
