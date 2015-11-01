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

#include "gameinfoimpl.h"
#include "gameinfo.h"
#include <utility.h>

#include <QDebug>
#include <QDir>


using namespace MOBase;
using namespace MOShared;


GameInfoImpl::GameInfoImpl()
{
}

IGameInfo::Type GameInfoImpl::type() const
{
  switch (GameInfo::instance().getType()) {
    case GameInfo::TYPE_OBLIVION: return IGameInfo::TYPE_OBLIVION;
    case GameInfo::TYPE_FALLOUT3: return IGameInfo::TYPE_FALLOUT3;
    case GameInfo::TYPE_FALLOUTNV: return IGameInfo::TYPE_FALLOUTNV;
    case GameInfo::TYPE_SKYRIM: return IGameInfo::TYPE_SKYRIM;
    default: throw MyException(QObject::tr("invalid game type %1").arg(GameInfo::instance().getType()));
  }
}


QString GameInfoImpl::path() const
{
  return QDir::fromNativeSeparators(ToQString(GameInfo::instance().getGameDirectory()));
}

QString GameInfoImpl::binaryName() const
{
  return ToQString(GameInfo::instance().getBinaryName());
}

namespace {

QString GetAppVersion(std::wstring const &app_name)
{
  DWORD handle;
  DWORD info_len = ::GetFileVersionInfoSizeW(app_name.c_str(), &handle);
  if (info_len == 0) {
    qDebug("GetFileVersionInfoSizeW Error %d", ::GetLastError());
    throw std::runtime_error("Failed to get version info");
  }

  std::vector<char> buff(info_len);
  if( ! ::GetFileVersionInfoW(app_name.c_str(), handle, info_len, buff.data())) {
    qDebug("GetFileVersionInfoW Error %d", ::GetLastError());
    throw std::runtime_error("Failed to get version info");
  }

  VS_FIXEDFILEINFO *pFileInfo;
  UINT buf_len;
  if ( ! ::VerQueryValueW(buff.data(), L"\\", reinterpret_cast<LPVOID *>(&pFileInfo), &buf_len)) {
    qDebug("VerQueryValueW Error %d", ::GetLastError());
    throw std::runtime_error("Failed to get version info");
  }
  return QString("%1.%2.%3.%4").arg(HIWORD(pFileInfo->dwFileVersionMS))
                               .arg(LOWORD(pFileInfo->dwFileVersionMS))
                               .arg(HIWORD(pFileInfo->dwFileVersionLS))
                               .arg(LOWORD(pFileInfo->dwFileVersionLS));
}

}

QString GameInfoImpl::version() const
{
  std::wstring dir = GameInfo::instance().getGameDirectory();
  std::wstring exec = GameInfo::instance().getBinaryName();
  std::wstring target = L"\\\\?\\" + dir + L"\\" + exec;
  return GetAppVersion(target.c_str());
}

QString GameInfoImpl::extenderVersion() const
{
  std::wstring dir = GameInfo::instance().getGameDirectory();
  std::wstring exec = GameInfo::instance().getExtenderName();
  std::wstring target = L"\\\\?\\" + dir + L"\\" + exec;
  return GetAppVersion(target.c_str());
}
