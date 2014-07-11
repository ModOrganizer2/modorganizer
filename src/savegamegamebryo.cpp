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

#include "savegamegamebyro.h"
#include <QFile>
#include <QBuffer>
#include <set>
#include "gameinfo.h"
#include <QFileInfo>
#include <QDateTime>
#include <limits>


using namespace MOShared;


template <typename T>
void FileRead(QFile &file, T &value)
{
  int read = file.read(reinterpret_cast<char*>(&value), sizeof(T));
  if (read != sizeof(T)) {
    throw std::runtime_error("unexpected end of file");
  }
}


template <typename T>
void FileSkip(QFile &file, int count = 1)
{
  char ignore[sizeof(T)];
  for (int i = 0; i < count; ++i) {
    if (file.read(ignore, sizeof(T)) != sizeof(T)) {
      throw std::runtime_error("unexpected end of file");
    }
  }
}


QString ReadBString(QFile &file)
{
  char buffer[256];
  file.read(buffer, 1); // size including zero termination
  unsigned char size = buffer[0];
  file.read(buffer, size);
  return QString::fromLatin1(buffer, size);
}


QString ReadFOSString(QFile &file, bool delimiter)
{
  union {
    char lengthBuffer[2];
    unsigned short length;
  };

  file.read(lengthBuffer, 2);
  if (delimiter) {
    FileSkip<char>(file); // 0x7c
  }
  char *buffer = new char[length];
  file.read(buffer, length);

  QString result = QString::fromLatin1(buffer, length);
  delete [] buffer;

  return result;
}


SaveGameGamebryo::SaveGameGamebryo(QObject *parent)
  : SaveGame(parent), m_Plugins()
{
}


SaveGameGamebryo::SaveGameGamebryo(QObject *parent, const QString &fileName)
  : SaveGame(parent, fileName), m_Plugins()
{
  readFile(fileName);
}


SaveGameGamebryo::SaveGameGamebryo(const SaveGameGamebryo& reference)
  : SaveGame(reference), m_Plugins(reference.m_Plugins)
{
}


SaveGameGamebryo& SaveGameGamebryo::operator=(const SaveGameGamebryo &reference)
{
  if (&reference != this) {
    SaveGame::operator =(reference);
    m_Plugins = reference.m_Plugins;
  }
  return *this;
}


SaveGameGamebryo::~SaveGameGamebryo()
{
}





void SaveGameGamebryo::readSkyrimFile(QFile &saveFile)
{
  char fileID[14];

  saveFile.read(fileID, 13);
  fileID[13] = '\0';
  if (strncmp(fileID, "TESV_SAVEGAME", 13) != 0) {
    throw std::runtime_error(QObject::tr("wrong file format").toUtf8().constData());
  }

  FileSkip<unsigned long>(saveFile); // header size
  FileSkip<unsigned long>(saveFile); // header version, -> 8
  FileRead(saveFile, m_SaveNumber);

  m_PCName = ReadFOSString(saveFile, false);

  unsigned long temp;
  FileRead(saveFile, temp); // player level
  m_PCLevel = static_cast<unsigned short>(temp);

  m_PCLocation = ReadFOSString(saveFile, false);
  ReadFOSString(saveFile, false); // playtime as ascii hhh.mm.ss
  ReadFOSString(saveFile, false); // race name (i.e. BretonRace)


  FileSkip<unsigned short>(saveFile); // ???
  FileSkip<float>(saveFile, 2); // ???
  FileSkip<unsigned char>(saveFile, 8); // filetime

//  FileSkip<unsigned char>(saveFile, 18); // ??? 18 bytes of data. not completely random, maybe a time stamp? maybe

  unsigned long width, height;
  FileRead(saveFile, width); // 320
  FileRead(saveFile, height); // 192

  QScopedArrayPointer<unsigned char> buffer(new unsigned char[width * height * 3]);
  saveFile.read(reinterpret_cast<char*>(buffer.data()), width * height * 3);
  // why do I have to copy here? without the copy, the buffer seems to get deleted after the
  // temporary vanishes, but Qts implicit sharing should handle that?
  m_Screenshot = QImage(buffer.data(), width, height, QImage::Format_RGB888).copy();

  FileSkip<unsigned char>(saveFile); // form version
  FileSkip<unsigned long>(saveFile); // plugin info size

  unsigned char pluginCount;
  FileRead(saveFile, pluginCount);

  for (int i = 0; i < pluginCount; ++i) {
    m_Plugins.push_back(ReadFOSString(saveFile, false));
  }
}


void SaveGameGamebryo::readESSFile(QFile &saveFile)
{
  char fileID[13];
  unsigned char versionMinor;
  unsigned long headerVersion, saveHeaderSize;
// *** format is different for fallout!
  saveFile.read(fileID, 12);
  fileID[12] = '\0';
  FileSkip<unsigned char>(saveFile); FileRead(saveFile, versionMinor);
  FileSkip<SYSTEMTIME>(saveFile); // modified time
  FileRead(saveFile, headerVersion); FileRead(saveFile, saveHeaderSize);

  if (strncmp(fileID, "TES4SAVEGAME", 12) != 0) {
    throw std::runtime_error(QObject::tr("wrong file format").toUtf8().constData());
  }

  FileRead(saveFile, m_SaveNumber);

  m_PCName = ReadBString(saveFile);

  FileRead(saveFile, m_PCLevel);
  m_PCLocation = ReadBString(saveFile);
  FileSkip<float>(saveFile);         // game days
  FileSkip<unsigned long>(saveFile); // game ticks
  FileRead(saveFile, m_CreationTime);

  unsigned long size;
  FileRead(saveFile, size); // screenshot size

  unsigned long width, height;
  FileRead(saveFile, width); FileRead(saveFile, height);
  QScopedArrayPointer<unsigned char> buffer(new unsigned char[width * height * 3]);
  saveFile.read(reinterpret_cast<char*>(buffer.data()), width * height * 3);
  // why do I have to copy here? without the copy, the buffer seems to get deleted after the
  // temporary vanishes, but Qts implicit sharing should handle that?
  m_Screenshot = QImage(buffer.data(), width, height, QImage::Format_RGB888).copy();

  unsigned char pluginCount;
  FileRead(saveFile, pluginCount);

  for (int i = 0; i < pluginCount; ++i) {
    QString name = ReadBString(saveFile);
    m_Plugins.push_back(name);
  }
}


void SaveGameGamebryo::readFOSFile(QFile &saveFile, bool newVegas)
{
  char fileID[13];
  saveFile.read(fileID, 12);
  // the signature is only 11 characters, the 12th is random?
  fileID[11] = '\0';

  if (strncmp(fileID, "FO3SAVEGAME", 11) != 0) {
    throw std::runtime_error(QObject::tr("wrong file format").toUtf8().constData());
  }

  char ignore = 0x00;
  while (ignore != 0x7c) {
    FileRead<char>(saveFile, ignore); // unknown
  }
  if (newVegas) {
    ignore = 0x00;
    // in new vegas there is another block of uninteresting (?) information
    FileSkip<char>(saveFile); // 0x7c
    while (ignore != 0x7c) {
      FileRead<char>(saveFile, ignore); // unknown
    }
  }

  unsigned long width, height;
  FileRead(saveFile, width);
  FileSkip<char>(saveFile); // 0x7c
  FileRead(saveFile, height);
  FileSkip<char>(saveFile); // 0x7c

  FileRead(saveFile, m_SaveNumber);
  FileSkip<char>(saveFile); // 0x7c

  m_PCName = ReadFOSString(saveFile, true);
  FileSkip<char>(saveFile); // 0x7c

  ReadFOSString(saveFile, true);
  FileSkip<char>(saveFile); // 0x7c

  long Level;
  FileRead(saveFile, Level);
  m_PCLevel = Level;
  FileSkip<char>(saveFile); // 0x7c

  m_PCLocation = ReadFOSString(saveFile, true);
  FileSkip<char>(saveFile); // 0x7c

  ReadFOSString(saveFile, true); // playtime

  FileSkip<char>(saveFile);

  QScopedArrayPointer<unsigned char> buffer(new unsigned char[width * height * 3]);
  saveFile.read(reinterpret_cast<char*>(buffer.data()), width * height * 3);
  // why do I have to copy here? without the copy, the buffer seems to get deleted after the
  // temporary vanishes, but Qts implicit sharing should handle that?
  m_Screenshot = QImage(buffer.data(), width, height, QImage::Format_RGB888).scaledToWidth(256);

  FileSkip<char>(saveFile, 5); // unknown

  unsigned char pluginCount = 0;
  FileRead(saveFile, pluginCount);
  FileSkip<char>(saveFile); // 0x7c

  for (int i = 0; i < pluginCount; ++i) {
    QString name = ReadFOSString(saveFile, true);
    m_Plugins.push_back(name);
    FileSkip<char>(saveFile); // 0x7c
  }
}


void SaveGameGamebryo::setCreationTime(const QString &fileName)
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


void SaveGameGamebryo::readFile(const QString &fileName)
{
  m_FileName = fileName;
  QFile saveFile(fileName);
  if (!saveFile.open(QIODevice::ReadOnly)) {
    throw std::runtime_error(QObject::tr("failed to open %1").arg(fileName).toUtf8().constData());
  }
  switch (GameInfo::instance().getType()) {
    case GameInfo::TYPE_FALLOUT3: {
      setCreationTime(fileName);
      readFOSFile(saveFile, false);
    } break;
    case GameInfo::TYPE_FALLOUTNV: {
      setCreationTime(fileName);
      readFOSFile(saveFile, true);
    } break;
    case GameInfo::TYPE_OBLIVION: {
      readESSFile(saveFile);
    } break;
    case GameInfo::TYPE_SKYRIM: {
      setCreationTime(fileName);
      readSkyrimFile(saveFile);
    } break;
  }

  saveFile.close();
}
