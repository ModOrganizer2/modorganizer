/*
Copyright (C) 2014 Sebastian Herbord. All rights reserved.

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


#include "safewritefile.h"
#include <QStringList>


using namespace MOBase;


SafeWriteFile::SafeWriteFile(const QString &fileName)
: m_FileName(fileName)
{
  if (!m_TempFile.open()) {
    throw MyException(QObject::tr("failed to open temporary file"));
  }
}


QFile *SafeWriteFile::operator->() {
  Q_ASSERT(m_TempFile.isOpen());
  return &m_TempFile;
}


void SafeWriteFile::commit() {
  shellDeleteQuiet(m_FileName);
  m_TempFile.rename(m_FileName);
  m_TempFile.setAutoRemove(false);
  m_TempFile.close();
}

bool SafeWriteFile::commitIfDifferent(uint &inHash) {
  uint newHash = hash();
  if (newHash != inHash) {
    commit();
    inHash = newHash;
    return true;
  } else {
    return false;
  }
}

uint SafeWriteFile::hash()
{
  qint64 pos = m_TempFile.pos();
  m_TempFile.seek(0);
  QByteArray data = m_TempFile.readAll();
  m_TempFile.seek(pos);
  return qHash(data);
}
