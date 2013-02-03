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

#include "dummybsa.h"
#include <QFile>
#include <gameinfo.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>


static void writeUlong(unsigned char* buffer, int offset, unsigned long value)
{
  union {
    unsigned long ulValue;
    unsigned char cValue[4];
  };
  ulValue = value;
  memcpy(buffer + offset, cValue, 4);
}

static void writeUlonglong(unsigned char* buffer, int offset, unsigned long long value)
{
  union {
    unsigned long long ullValue;
    unsigned char cValue[8];
  };
  ullValue = value;
  memcpy(buffer + offset, cValue, 8);
}
/*
static unsigned long long genHashInt(const char* pos, const char* end)
{
  unsigned long long hash2 = 0;
  for (; pos < end; ++pos) {
    hash2 *= 0x1003f;
    hash2 += *pos;
  }
  return hash2;
}


static unsigned long long genHash(const char* fileName)
{
  char fileNameLower[MAX_PATH + 1];
  int i = 0;
  for (; i < MAX_PATH && fileName[i] != '\0'; ++i) {
    fileNameLower[i] = tolower(fileName[i]);
  }
  fileNameLower[i] = '\0';

  char* ext = strrchr(fileNameLower, '.');
  if (ext == NULL) {
    ext = fileNameLower + strlen(fileNameLower);
  }

  int length = ext - fileNameLower;

  unsigned long long hash = 0ULL;

  if (length > 0) {
    hash = *(ext - 1) |
           ((length > 2 ? *(ext - 2) : 0) << 8) |
           (length << 16) |
           (fileNameLower[0] << 24);
  }

  if (strcmp(ext + 1, "dds") == 0) {
    hash |= 0x8080;
  } // the rest is not supported currently

  hash |= genHashInt(fileNameLower + 1, ext - 2) << 0x32;
  hash |= genHashInt(ext, ext + strlen(ext)) << 0x32;

  return hash;
}*/


static unsigned long genHashInt(const unsigned char *pos, const unsigned char *end)
{
  unsigned long hash = 0;
  for (; pos < end; ++pos) {
    hash *= 0x1003f;
    hash += *pos;
  }
  return hash;
}


static unsigned long long genHash(const char* fileName)
{
  char fileNameLower[MAX_PATH + 1];
  int i = 0;
  for (; i < MAX_PATH && fileName[i] != '\0'; ++i) {
    fileNameLower[i] = static_cast<char>(tolower(fileName[i]));
    if (fileNameLower[i] == '\\') {
      fileNameLower[i] = '/';
    }
  }
  fileNameLower[i] = '\0';

  unsigned char *fileNameLowerU = reinterpret_cast<unsigned char*>(fileNameLower);

  char* ext = strrchr(fileNameLower, '.');
  if (ext == NULL) {
    ext = fileNameLower + strlen(fileNameLower);
  }
  unsigned char *extU = reinterpret_cast<unsigned char*>(ext);

  int length = ext - fileNameLower;

  unsigned long long hash = 0ULL;

  if (length > 0) {
    hash = *(extU - 1) |
           ((length > 2 ? *(ext - 2) : 0) << 8) |
           (length << 16) |
           (fileNameLowerU[0] << 24);
  }

  if (strlen(ext) > 0) {
    if (strcmp(ext + 1, "kf") == 0) {
      hash |= 0x80;
    } else if (strcmp(ext + 1, "nif") == 0) {
      hash |= 0x8000;
    } else if (strcmp(ext + 1, "dds") == 0) {
      hash |= 0x8080;
    } else if (strcmp(ext + 1, "wav") == 0) {
      hash |= 0x80000000;
    }

    unsigned long long temp = static_cast<unsigned long long>(genHashInt(
                fileNameLowerU + 1, extU - 2));
    temp += static_cast<unsigned long long>(genHashInt(
                extU, extU + strlen(ext)));

    hash |= (temp & 0xFFFFFFFF) << 32;
  }
  return hash;
}


DummyBSA::DummyBSA()
  : m_Version(GameInfo::instance().getBSAVersion()), m_FolderName(""), m_FileName("dummy.dds"),
    m_TotalFileNameLength(0)
{
}


void DummyBSA::writeHeader(QFile& file)
{
  unsigned char header[] = {
                     'B',  'S',  'A', '\0', // magic string
                    0xDE, 0xAD, 0xBE, 0xEF, // version - insert later
                    0x24, 0x00, 0x00, 0x00, // offset to folder recors. header size is static
                    0xDE, 0xAD, 0xBE, 0xEF, // archive flags - insert later
                    0x01, 0x00, 0x00, 0x00, // folder count
                    0x01, 0x00, 0x00, 0x00, // file count
                    0xDE, 0xAD, 0xBE, 0xEF, // total folder names length - insert later
                    0xDE, 0xAD, 0xBE, 0xEF, // total file names length - insert later
                    0xDE, 0xAD, 0xBE, 0xEF  // file flags - insert later
                  };

  writeUlong(header, 4, GameInfo::instance().getBSAVersion());
  writeUlong(header, 12, 0x01 | 0x02); // has directories and has files.
  writeUlong(header, 24, m_FolderName.length() + 1); // empty folder name
  writeUlong(header, 28, m_TotalFileNameLength);   // single character file name

  writeUlong(header, 32, 2); // has dds

  file.write(reinterpret_cast<char*>(header), sizeof(header));
}


void DummyBSA::writeFolderRecord(QFile& file, const std::string& folderName)
{
  unsigned char folderRecord[] = {
                          0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, // folder hash
                          0x01, 0x00, 0x00, 0x00, // file count
                          0xDE, 0xAD, 0xBE, 0xEF, // offset to folder name
                        };
  // we'd usually have to sort folders be the hash value generated here
  writeUlonglong(folderRecord, 0, genHash(folderName.c_str()));
  writeUlong(    folderRecord, 12, 0x34 + m_TotalFileNameLength); // TODO: this should be calculated properly

  file.write(reinterpret_cast<char*>(folderRecord), sizeof(folderRecord));
}


void DummyBSA::writeFileRecord(QFile& file, const std::string& fileName)
{
  unsigned char fileRecord[] = {
                        0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF, // file name hash
                        0xDE, 0xAD, 0xBE, 0xEF, // size
                        0xDE, 0xAD, 0xBE, 0xEF, // offset to file data
                      };

  // we'd usually have to sort files by the value generated here
  writeUlonglong(fileRecord,  0, genHash(fileName.c_str()));
  writeUlong(    fileRecord,  8, 0);
  writeUlong(    fileRecord, 12, 0x44 + (fileName.length() + 1) + 4); // after this record we expect the filename and 4 bytes of file size

  file.write(reinterpret_cast<char*>(fileRecord), sizeof(fileRecord));
}


void DummyBSA::writeFileRecordBlocks(QFile& file, const std::string& folderName)
{
  file.write(folderName.c_str(), folderName.length() + 1);

  writeFileRecord(file, m_FileName);
}


void DummyBSA::write(const QString& fileName)
{
  QFile file(fileName);
  file.open(QIODevice::WriteOnly);

  m_TotalFileNameLength = m_FileName.length() + 1;

  writeHeader(file);
  writeFolderRecord(file, m_FolderName);
  writeFileRecordBlocks(file, m_FolderName);
  file.write(m_FileName.c_str() , m_FileName.length() + 1);
  char fileSize[] = { 0x00, 0x00, 0x00, 0x00 };
  file.write(fileSize, sizeof(fileSize));
  file.close();
}
