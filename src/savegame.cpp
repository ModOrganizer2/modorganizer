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

#include "savegame.h"
#include <QFile>
#include <QBuffer>
#include <set>
#include <QFileInfo>
#include <QDateTime>
#include <utility.h>
#include <limits>
#include "gameinfo.h"


SaveGame::SaveGame(QObject *parent)
  : QObject(parent), m_FileName(), m_PCName(), m_PCLevel(0), m_PCLocation(), m_SaveNumber(0), m_Screenshot()
{
}


SaveGame::SaveGame(QObject *parent, const QString &filename)
  : QObject(parent), m_FileName(filename), m_PCName(), m_PCLevel(0), m_PCLocation(), m_SaveNumber(0), m_Screenshot()
{
}


SaveGame::SaveGame(const SaveGame& reference)
  : m_FileName(reference.m_FileName), m_PCName(reference.m_PCName), m_PCLevel(reference.m_PCLevel),
    m_PCLocation(reference.m_PCLocation), m_SaveNumber(reference.m_SaveNumber),
    m_Screenshot(reference.m_Screenshot)
{
}


SaveGame& SaveGame::operator=(const SaveGame &reference)
{
  if (&reference != this) {
    m_FileName = reference.m_FileName;
    m_PCName = reference.m_PCName;
    m_PCLevel = reference.m_PCLevel;
    m_PCLocation = reference.m_PCLocation;
    m_SaveNumber = reference.m_SaveNumber;
    m_Screenshot = reference.m_Screenshot;
  }
  return *this;
}


SaveGame::~SaveGame()
{
}

QStringList SaveGame::attachedFiles() const
{
  QStringList result;
  foreach (const std::wstring &ext, MOShared::GameInfo::instance().getSavegameAttachmentExtensions()) {
    QFileInfo fi(fileName());
    fi.setFile(fi.canonicalPath() + "/" + fi.completeBaseName() + "." + MOBase::ToQString(ext));
    if (fi.exists()) {
      result.append(fi.filePath());
    }
  }

  return result;
}

QStringList SaveGame::saveFiles() const
{
  QStringList result = attachedFiles();
  result.append(fileName());
  return result;
}


void SaveGame::setCreationTime(const QString &fileName)
{
  QFileInfo creationTime(fileName);
  QDateTime modified = creationTime.lastModified();
  memset(&m_CreationTime, 0, sizeof(SYSTEMTIME));

  m_CreationTime.wDay = static_cast<WORD>(modified.date().day());
  m_CreationTime.wDayOfWeek = static_cast<WORD>(modified.date().dayOfWeek());
  m_CreationTime.wMonth = static_cast<WORD>(modified.date().month());
  m_CreationTime.wYear =static_cast<WORD>( modified.date().year());

  m_CreationTime.wHour = static_cast<WORD>(modified.time().hour());
  m_CreationTime.wMinute = static_cast<WORD>(modified.time().minute());
  m_CreationTime.wSecond = static_cast<WORD>(modified.time().second());
  m_CreationTime.wMilliseconds = static_cast<WORD>(modified.time().msec());
}
