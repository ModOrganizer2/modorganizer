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

#include "directoryentry.h"
#include "windows_error.h"
#include "leaktrace.h"
#include "error_report.h"
#include <bsatk.h>
#include <boost/bind.hpp>
#include <boost/scoped_array.hpp>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <sstream>
#include <ctime>
#include <algorithm>
#include <map>
#include <atomic>


namespace MOShared {

static const int MAXPATH_UNICODE = 32767;

class OriginConnection {

public:

  typedef int Index;
  static const int INVALID_INDEX = INT_MIN;

public:

  OriginConnection()
    : m_NextID(0)
  {
    LEAK_TRACE;
  }

  ~OriginConnection()
  {
    LEAK_UNTRACE;
  }

  FilesOrigin& createOrigin(const std::wstring &originName, const std::wstring &directory, int priority,
                            boost::shared_ptr<FileRegister> fileRegister, boost::shared_ptr<OriginConnection> originConnection) {
    int newID = createID();
    m_Origins[newID] = FilesOrigin(newID, originName, directory, priority, fileRegister, originConnection);
    m_OriginsNameMap[originName] = newID;
    m_OriginsPriorityMap[priority] = newID;
    return m_Origins[newID];
  }

  bool exists(const std::wstring &name) {
    return m_OriginsNameMap.find(name) != m_OriginsNameMap.end();
  }

  FilesOrigin &getByID(Index ID) {
    return m_Origins[ID];
  }

  FilesOrigin &getByName(const std::wstring &name) {
    std::map<std::wstring, int>::iterator iter = m_OriginsNameMap.find(name);
    if (iter != m_OriginsNameMap.end()) {
      return m_Origins[iter->second];
    } else {
      std::ostringstream stream;
      stream << "invalid origin name: " << ToString(name, false);
      throw std::runtime_error(stream.str());
    }
  }

  void changePriorityLookup(int oldPriority, int newPriority)
  {
    auto iter = m_OriginsPriorityMap.find(oldPriority);
    if (iter != m_OriginsPriorityMap.end()) {
      Index idx = iter->second;
      m_OriginsPriorityMap.erase(iter);
      m_OriginsPriorityMap[newPriority] = idx;
    }
  }

  void changeNameLookup(const std::wstring &oldName, const std::wstring &newName)
  {
    auto iter = m_OriginsNameMap.find(oldName);
    if (iter != m_OriginsNameMap.end()) {
      Index idx = iter->second;
      m_OriginsNameMap.erase(iter);
      m_OriginsNameMap[newName] = idx;
    } else {
      log("failed to change name lookup from %ls to %ls", oldName.c_str(), newName.c_str());
    }
  }

private:

  Index createID() {
    return m_NextID++;
  }

private:

  Index m_NextID;

  std::map<Index, FilesOrigin> m_Origins;
  std::map<std::wstring, Index> m_OriginsNameMap;
  std::map<int, Index> m_OriginsPriorityMap;

};


//
// FilesOrigin
//


void FilesOrigin::enable(bool enabled, time_t notAfter)
{
  if (!enabled) {
    std::set<FileEntry::Index> copy = m_Files;
    m_FileRegister.lock()->removeOriginMulti(copy, m_ID, notAfter);
    m_Files.clear();
  }
  m_Disabled = !enabled;
}


void FilesOrigin::removeFile(FileEntry::Index index)
{
  auto iter = m_Files.find(index);
  if (iter != m_Files.end()) {
    m_Files.erase(iter);
  }
}



static std::wstring tail(const std::wstring &source, const size_t count)
{
  if (count >= source.length()) {
    return source;
  }

  return source.substr(source.length() - count);
}


FilesOrigin::FilesOrigin()
  : m_ID(0), m_Disabled(false), m_Name(), m_Path(), m_Priority(0)
{
  LEAK_TRACE;
}

FilesOrigin::FilesOrigin(const FilesOrigin &reference)
  : m_ID(reference.m_ID)
  , m_Disabled(reference.m_Disabled)
  , m_Name(reference.m_Name)
  , m_Path(reference.m_Path)
  , m_Priority(reference.m_Priority)
  , m_FileRegister(reference.m_FileRegister)
  , m_OriginConnection(reference.m_OriginConnection)
{
  LEAK_TRACE;
}


FilesOrigin::FilesOrigin(int ID, const std::wstring &name, const std::wstring &path, int priority, boost::shared_ptr<MOShared::FileRegister> fileRegister, boost::shared_ptr<MOShared::OriginConnection> originConnection)
  : m_ID(ID), m_Disabled(false), m_Name(name), m_Path(path), m_Priority(priority),
    m_FileRegister(fileRegister), m_OriginConnection(originConnection)
{
  LEAK_TRACE;
}

FilesOrigin::~FilesOrigin()
{
  LEAK_UNTRACE;
}


void FilesOrigin::setPriority(int priority)
{
  m_OriginConnection.lock()->changePriorityLookup(m_Priority, priority);

  m_Priority = priority;
}


void FilesOrigin::setName(const std::wstring &name)
{
  m_OriginConnection.lock()->changeNameLookup(m_Name, name);
  // change path too
  if (tail(m_Path, m_Name.length()) == m_Name) {
    m_Path = m_Path.substr(0, m_Path.length() - m_Name.length()).append(name);
  }
  m_Name = name;
}

std::vector<FileEntry::Ptr> FilesOrigin::getFiles() const
{
  std::vector<FileEntry::Ptr> result;
  for (FileEntry::Index fileIdx : m_Files)
    if (FileEntry::Ptr p = m_FileRegister.lock()->getFile(fileIdx))
      result.push_back(p);

  return result;
}

bool FilesOrigin::containsArchive(std::wstring archiveName)
{
  for (FileEntry::Index fileIdx : m_Files)
    if (FileEntry::Ptr p = m_FileRegister.lock()->getFile(fileIdx))
      if (p->isFromArchive(archiveName)) return true;
  return false;
}

//
// FileEntry
//

void FileEntry::addOrigin(int origin, FILETIME fileTime, const std::wstring &archive, int order)
{
  m_LastAccessed = time(nullptr);
  if (m_Parent != nullptr) {
    m_Parent->propagateOrigin(origin);
  }
  if (m_Origin == -1) {
    m_Origin = origin;
    m_FileTime = fileTime;
    m_Archive = std::pair<std::wstring, int>(archive, order);
  } else if ((m_Parent != nullptr)
             && (m_Parent->getOriginByID(origin).getPriority() > m_Parent->getOriginByID(m_Origin).getPriority())
             && (archive.size() == 0 || m_Archive.first.size() > 0 )) {
    if (std::find_if(m_Alternatives.begin(), m_Alternatives.end(), [&](const std::pair<int, std::pair<std::wstring, int>> &i) -> bool { return i.first == m_Origin; }) == m_Alternatives.end()) {
      m_Alternatives.push_back(std::pair<int, std::pair<std::wstring, int>>(m_Origin, m_Archive));
    }
    m_Origin = origin;
    m_FileTime = fileTime;
    m_Archive = std::pair<std::wstring, int>(archive, order);
  } else {
    bool found = false;
    if (m_Origin == origin) {
      // already an origin
      return;
    }
    for (std::vector<std::pair<int, std::pair<std::wstring, int>>>::iterator iter = m_Alternatives.begin(); iter != m_Alternatives.end(); ++iter) {
      if (iter->first == origin) {
        // already an origin
        return;
      }
      if ((m_Parent != nullptr)
          && (m_Parent->getOriginByID(iter->first).getPriority() < m_Parent->getOriginByID(origin).getPriority())) {
        m_Alternatives.insert(iter, std::pair<int, std::pair<std::wstring, int>>(origin, std::pair<std::wstring, int>(archive, order)));
        found = true;
        break;
      }
    }
    if (!found) {
      m_Alternatives.push_back(std::pair<int, std::pair<std::wstring, int>>(origin, std::pair<std::wstring, int>(archive, order)));
    }
  }
}

bool FileEntry::removeOrigin(int origin)
{
  if (m_Origin == origin) {
    if (!m_Alternatives.empty()) {
      // find alternative with the highest priority
      std::vector<std::pair<int, std::pair<std::wstring, int>>>::iterator currentIter = m_Alternatives.begin();
      for (std::vector<std::pair<int, std::pair<std::wstring, int>>>::iterator iter = m_Alternatives.begin(); iter != m_Alternatives.end(); ++iter) {
        if (iter->first != origin) {

          //Both files are not from archives.
          if (!iter->second.first.size() && !currentIter->second.first.size()) {
            if ((m_Parent->getOriginByID(iter->first).getPriority() > m_Parent->getOriginByID(currentIter->first).getPriority())) {
              currentIter = iter;
            }
          }
          else {
            //Both files are from archives
            if (iter->second.first.size() && currentIter->second.first.size()) {
              if (iter->second.second > currentIter->second.second) {
                currentIter = iter;
              }
            }
            else {
              //Only one of the two is an archive, so we change currentIter only if he is the archive one.
              if (currentIter->second.first.size()) {
                currentIter = iter;
              }
            }
          }
        }
      }
      int currentID = currentIter->first;
      m_Archive = currentIter->second;
      m_Alternatives.erase(currentIter);

      m_Origin = currentID;

      // now we need to update the file time...
      std::wstring filePath = getFullPath();
      HANDLE file = ::CreateFile(filePath.c_str(), GENERIC_READ | GENERIC_WRITE,
                                 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (!::GetFileTime(file, nullptr, nullptr, &m_FileTime)) {
        // maybe this file is in a bsa, but there is no easy way to find out which. User should refresh
        // the view to find out
        //m_Archive = std::pair<std::wstring, int>(L"bsa?", -1);
      } else {
        //m_Archive = std::pair<std::wstring, int>(L"", -1);
      }

      ::CloseHandle(file);

    } else {
      m_Origin = -1;
      m_Archive = std::pair<std::wstring, int>(L"", -1);
      return true;
    }
  } else {
    std::vector<std::pair<int, std::pair<std::wstring, int>>>::iterator newEnd = std::find_if(m_Alternatives.begin(), m_Alternatives.end(), [&](const std::pair<int, std::pair<std::wstring, int>> &i) -> bool { return i.first == origin; });
    if (newEnd != m_Alternatives.end())
      m_Alternatives.erase(newEnd, m_Alternatives.end());
  }
  return false;
}

FileEntry::FileEntry()
  : m_Index(UINT_MAX), m_Name(), m_Origin(-1), m_Parent(nullptr), m_LastAccessed(time(nullptr))
{
  LEAK_TRACE;
}

FileEntry::FileEntry(Index index, const std::wstring &name, DirectoryEntry *parent)
  : m_Index(index), m_Name(name), m_Origin(-1), m_Archive(L"", -1), m_Parent(parent), m_LastAccessed(time(nullptr))
{
  LEAK_TRACE;
}

FileEntry::~FileEntry()
{
  LEAK_UNTRACE;
}

void FileEntry::sortOrigins()
{
  m_Alternatives.push_back(std::pair<int, std::pair<std::wstring, int>>(m_Origin, m_Archive));
  std::sort(m_Alternatives.begin(), m_Alternatives.end(), [&](const std::pair<int, std::pair<std::wstring, int>> &LHS, const std::pair<int, std::pair<std::wstring, int>> &RHS) -> bool {
    if (!LHS.second.first.size() && !RHS.second.first.size()) {
      int l = m_Parent->getOriginByID(LHS.first).getPriority(); if (l < 0) l = INT_MAX;
      int r = m_Parent->getOriginByID(RHS.first).getPriority(); if (r < 0) r = INT_MAX;

      return l < r;
    }

    if (LHS.second.first.size() && RHS.second.first.size()) {
      int l = LHS.second.second; if (l < 0) l = INT_MAX;
      int r = RHS.second.second; if (r < 0) r = INT_MAX;

      return l < r;
    }

    if (RHS.second.first.size()) return false;
    return true;
  });
  if (!m_Alternatives.empty()) {
    m_Origin = m_Alternatives.back().first;
    m_Archive = m_Alternatives.back().second;
    m_Alternatives.pop_back();
  }
}


bool FileEntry::recurseParents(std::wstring &path, const DirectoryEntry *parent) const
{
  if (parent == nullptr) {
    return false;
  } else {
    // don't append the topmost parent because it is the virtual data-root
    if (recurseParents(path, parent->getParent())) {
      path.append(L"\\").append(parent->getName());
    }
    return true;
  }
}

std::wstring FileEntry::getFullPath() const
{
  std::wstring result;
  bool ignore = false;
  result = m_Parent->getOriginByID(getOrigin(ignore)).getPath(); //base directory for origin
  recurseParents(result, m_Parent); // all intermediate directories
  return result + L"\\" + m_Name;
}

std::wstring FileEntry::getRelativePath() const
{
  std::wstring result;
  recurseParents(result, m_Parent); // all intermediate directories
  return result + L"\\" + m_Name;
}

bool FileEntry::isFromArchive(std::wstring archiveName)
{
  if (archiveName.length() == 0) return m_Archive.first.length() != 0;
  if (m_Archive.first.compare(archiveName) == 0) return true;
  for (auto alternative : m_Alternatives) {
    if (alternative.second.first.compare(archiveName) == 0) return true;
  }
  return false;
}


//
// DirectoryEntry
//
DirectoryEntry::DirectoryEntry(const std::wstring &name, DirectoryEntry *parent, int originID)
  : m_OriginConnection(new OriginConnection),
    m_Name(name), m_Parent(parent), m_Populated(false), m_TopLevel(true)
{
  m_FileRegister.reset(new FileRegister(m_OriginConnection));
  m_Origins.insert(originID);
  LEAK_TRACE;
}

DirectoryEntry::DirectoryEntry(const std::wstring &name, DirectoryEntry *parent, int originID,
               boost::shared_ptr<FileRegister> fileRegister, boost::shared_ptr<OriginConnection> originConnection)
  : m_FileRegister(fileRegister), m_OriginConnection(originConnection),
    m_Name(name), m_Parent(parent), m_Populated(false), m_TopLevel(false)
{
  LEAK_TRACE;
  m_Origins.insert(originID);
}


DirectoryEntry::~DirectoryEntry()
{
  LEAK_UNTRACE;
  clear();
}


const std::wstring &DirectoryEntry::getName() const
{
  return m_Name;
}


void DirectoryEntry::clear()
{
  m_Files.clear();
  for (DirectoryEntry *entry : m_SubDirectories) {
    delete entry;
  }
  m_SubDirectories.clear();
}


FilesOrigin &DirectoryEntry::createOrigin(const std::wstring &originName, const std::wstring &directory, int priority)
{
  if (m_OriginConnection->exists(originName)) {
    FilesOrigin &origin = m_OriginConnection->getByName(originName);
    origin.enable(true);
    return origin;
  } else {
    return m_OriginConnection->createOrigin(originName, directory, priority, m_FileRegister, m_OriginConnection);
  }
}


void DirectoryEntry::addFromOrigin(const std::wstring &originName, const std::wstring &directory, int priority)
{
  FilesOrigin &origin = createOrigin(originName, directory, priority);
  if (directory.length() != 0) {
    boost::scoped_array<wchar_t> buffer(new wchar_t[MAXPATH_UNICODE + 1]);
    memset(buffer.get(), L'\0', MAXPATH_UNICODE + 1);
    int offset = _snwprintf(buffer.get(), MAXPATH_UNICODE, L"%ls", directory.c_str());
    buffer.get()[offset] = L'\0';
    addFiles(origin, buffer.get(), offset);
  }
  m_Populated = true;
}


void DirectoryEntry::addFromBSA(const std::wstring &originName, std::wstring &directory, const std::wstring &fileName, int priority, int order)
{
  FilesOrigin &origin = createOrigin(originName, directory, priority);

  WIN32_FILE_ATTRIBUTE_DATA fileData;
  if (::GetFileAttributesExW(fileName.c_str(), GetFileExInfoStandard, &fileData) == 0) {
    throw windows_error("failed to determine file time");
  }
  FILETIME now;
  ::GetSystemTimeAsFileTime(&now);

  const double clfSecondsPer100ns = 100. * 1.E-9;

  ((ULARGE_INTEGER *)&now)->QuadPart -= ((double)5) / clfSecondsPer100ns;

  size_t namePos = fileName.find_last_of(L"\\/");
  if (namePos == std::wstring::npos) {
    namePos = 0;
  }
  else {
    ++namePos;
  }

  if (!containsArchive(fileName.substr(namePos)) || ::CompareFileTime(&fileData.ftLastWriteTime, &now) > 0) {
    BSA::Archive archive;
    BSA::EErrorCode res = archive.read(ToString(fileName, false).c_str(), false);
    if ((res != BSA::ERROR_NONE) && (res != BSA::ERROR_INVALIDHASHES)) {
      std::ostringstream stream;
      stream << "invalid bsa file: " << ToString(fileName, false) << " errorcode " << res << " - " << ::GetLastError();
      throw std::runtime_error(stream.str());
    }

    addFiles(origin, archive.getRoot(), fileData.ftLastWriteTime, fileName.substr(namePos), order);
    m_Populated = true;
  }
}

void DirectoryEntry::propagateOrigin(int origin)
{
  m_Origins.insert(origin);
  if (m_Parent != nullptr) {
    m_Parent->propagateOrigin(origin);
  }
}


static bool SupportOptimizedFind()
{
  // large fetch and basic info for FindFirstFileEx is supported on win server 2008 r2, win 7 and newer

  OSVERSIONINFOEX versionInfo;
  versionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  versionInfo.dwMajorVersion = 6;
  versionInfo.dwMinorVersion = 1;
  ULONGLONG mask = ::VerSetConditionMask(
                     ::VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL),
                     VER_MINORVERSION, VER_GREATER_EQUAL);

  bool res = ::VerifyVersionInfo(&versionInfo, VER_MAJORVERSION | VER_MINORVERSION, mask) == TRUE;
  return res;
}


static bool DirCompareByName(const DirectoryEntry *lhs, const DirectoryEntry *rhs)
{
  return _wcsicmp(lhs->getName().c_str(), rhs->getName().c_str()) < 0;
}


void DirectoryEntry::addFiles(FilesOrigin &origin, wchar_t *buffer, int bufferOffset)
{
  WIN32_FIND_DATAW findData;

  _snwprintf_s(buffer + bufferOffset, MAXPATH_UNICODE - bufferOffset, _TRUNCATE, L"\\*");

  HANDLE searchHandle = nullptr;

  if (SupportOptimizedFind()) {
    searchHandle = ::FindFirstFileExW(buffer, FindExInfoBasic, &findData, FindExSearchNameMatch, nullptr,
                                      FIND_FIRST_EX_LARGE_FETCH);
  } else {
    searchHandle = ::FindFirstFileExW(buffer, FindExInfoStandard, &findData, FindExSearchNameMatch, nullptr, 0);
  }

  if (searchHandle != INVALID_HANDLE_VALUE) {
    BOOL result = true;
    while (result) {
      if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        if ((wcscmp(findData.cFileName, L".") != 0) &&
            (wcscmp(findData.cFileName, L"..") != 0)) {
          int offset = _snwprintf(buffer + bufferOffset, MAXPATH_UNICODE, L"\\%ls", findData.cFileName);
          // recurse into subdirectories
          getSubDirectory(findData.cFileName, true, origin.getID())->addFiles(origin, buffer, bufferOffset + offset);
        }
      } else {
        insert(findData.cFileName, origin, findData.ftLastWriteTime, L"", -1);
      }
      result = ::FindNextFileW(searchHandle, &findData);
    }
  }
  std::sort(m_SubDirectories.begin(), m_SubDirectories.end(), &DirCompareByName);
  ::FindClose(searchHandle);
}


void DirectoryEntry::addFiles(FilesOrigin &origin, BSA::Folder::Ptr archiveFolder, FILETIME &fileTime, const std::wstring &archiveName, int order)
{
  // add files
  for (unsigned int fileIdx = 0; fileIdx < archiveFolder->getNumFiles(); ++fileIdx) {
    BSA::File::Ptr file = archiveFolder->getFile(fileIdx);
    insert(ToWString(file->getName(), true), origin, fileTime, archiveName, order);
  }

  // recurse into subdirectories
  for (unsigned int folderIdx = 0; folderIdx < archiveFolder->getNumSubFolders(); ++folderIdx) {
    BSA::Folder::Ptr folder = archiveFolder->getSubFolder(folderIdx);
    DirectoryEntry *folderEntry = getSubDirectoryRecursive(ToWString(folder->getName(), true), true, origin.getID());

    folderEntry->addFiles(origin, folder, fileTime, archiveName, order);
  }
}


bool DirectoryEntry::removeFile(const std::wstring &filePath, int *origin)
{
  size_t pos = filePath.find_first_of(L"\\/");
  if (pos == std::string::npos) {
    return this->remove(filePath, origin);
  } else {
    std::wstring dirName = filePath.substr(0, pos);
    std::wstring rest = filePath.substr(pos + 1);
    DirectoryEntry *entry = getSubDirectoryRecursive(dirName, false);
    if (entry != nullptr) {
      return entry->removeFile(rest, origin);
    } else {
      return false;
    }
  }
}

void DirectoryEntry::removeDirRecursive()
{
  while (!m_Files.empty()) {
    m_FileRegister->removeFile(m_Files.begin()->second);
  }

  for (DirectoryEntry *entry : m_SubDirectories) {
    entry->removeDirRecursive();
    delete entry;
  }
  m_SubDirectories.clear();
}

void DirectoryEntry::removeDir(const std::wstring &path)
{
  size_t pos = path.find_first_of(L"\\/");
  if (pos == std::string::npos) {
    for (auto iter = m_SubDirectories.begin(); iter != m_SubDirectories.end(); ++iter) {
      DirectoryEntry *entry = *iter;
      if (CaseInsensitiveEqual(entry->getName(), path)) {
        entry->removeDirRecursive();
        m_SubDirectories.erase(iter);
        delete entry;
        break;
      }
    }
  } else {
    std::wstring dirName = path.substr(0, pos);
    std::wstring rest = path.substr(pos + 1);
    DirectoryEntry *entry = getSubDirectoryRecursive(dirName, false);
    if (entry != nullptr) {
      entry->removeDir(rest);
    }
  }
}

bool DirectoryEntry::hasContentsFromOrigin(int originID) const
{
  return m_Origins.find(originID) != m_Origins.end();
}

void DirectoryEntry::insertFile(const std::wstring &filePath, FilesOrigin &origin, FILETIME fileTime)
{
  size_t pos = filePath.find_first_of(L"\\/");
  if (pos == std::string::npos) {
    this->insert(filePath, origin, fileTime, std::wstring(), -1);
  } else {
    std::wstring dirName = filePath.substr(0, pos);
    std::wstring rest = filePath.substr(pos + 1);
    getSubDirectoryRecursive(dirName, true, origin.getID())->insertFile(rest, origin, fileTime);
  }
}


void DirectoryEntry::removeFile(FileEntry::Index index)
{
  if (!m_Files.empty()) {
    auto iter = std::find_if(m_Files.begin(), m_Files.end(),
                             [&index](const std::pair<std::wstring, FileEntry::Index> &iter) -> bool {
                               return iter.second == index; } );
    if (iter != m_Files.end()) {
      m_Files.erase(iter);
    } else {
      log("file \"%ls\" not in directory \"%ls\"",
          m_FileRegister->getFile(index)->getName().c_str(),
          this->getName().c_str());
    }
  } else {
    log("file \"%ls\" not in directory \"%ls\", directory empty",
        m_FileRegister->getFile(index)->getName().c_str(),
        this->getName().c_str());
  }
}


void DirectoryEntry::removeFiles(const std::set<FileEntry::Index> &indices)
{
  for (auto iter = m_Files.begin(); iter != m_Files.end();) {
    if (indices.find(iter->second) != indices.end()) {
      m_Files.erase(iter++);
    } else {
      ++iter;
    }
  }
}

bool DirectoryEntry::containsArchive(std::wstring archiveName)
{
  for (auto iter = m_Files.begin(); iter != m_Files.end(); ++iter) {
    FileEntry::Ptr entry = m_FileRegister->getFile(iter->second);
    if (entry->isFromArchive(archiveName)) return true;
  }
  return false;
}

int DirectoryEntry::anyOrigin() const
{
  bool ignore;
  for (auto iter = m_Files.begin(); iter != m_Files.end(); ++iter) {
    FileEntry::Ptr entry = m_FileRegister->getFile(iter->second);
    if ((entry.get() != nullptr) && !entry->isFromArchive()) {
      return entry->getOrigin(ignore);
    }
  }

  // if we got here, no file directly within this directory is a valid indicator for a mod, thus
  // we continue looking in subdirectories
  for (DirectoryEntry *entry : m_SubDirectories) {
    int res = entry->anyOrigin();
    if (res != -1){
      return res;
    }
  }
  return *(m_Origins.begin());
}


bool DirectoryEntry::originExists(const std::wstring &name) const
{
  return m_OriginConnection->exists(name);
}


FilesOrigin &DirectoryEntry::getOriginByID(int ID) const
{
  return m_OriginConnection->getByID(ID);
}


FilesOrigin &DirectoryEntry::getOriginByName(const std::wstring &name) const
{
  return m_OriginConnection->getByName(name);
}

/*
int DirectoryEntry::getOrigin(const std::wstring &path, bool &archive)
{
  const DirectoryEntry *directory = nullptr;
  const FileEntry::Ptr file = searchFile(path, &directory);
  if (file.get() != nullptr) {
    return file->getOrigin(archive);
  } else {
    if (directory != nullptr) {
      return directory->anyOrigin();
    } else {
      return -1;
    }
  }
}*/

std::vector<FileEntry::Ptr> DirectoryEntry::getFiles() const
{
  std::vector<FileEntry::Ptr> result;
  for (auto iter = m_Files.begin(); iter != m_Files.end(); ++iter) {
    result.push_back(m_FileRegister->getFile(iter->second));
  }
  return result;
}


const FileEntry::Ptr DirectoryEntry::searchFile(const std::wstring &path, const DirectoryEntry **directory) const
{
  if (directory != nullptr) {
    *directory = nullptr;
  }

  if ((path.length() == 0) || (path == L"*")) {
    // no file name -> the path ended on a (back-)slash
    if (directory != nullptr) {
      *directory = this;
    }
    return FileEntry::Ptr();
  }

  size_t len =  path.find_first_of(L"\\/");

  if (len == std::string::npos) {
    // no more path components
    auto iter = m_Files.find(ToLower(path));
    if (iter != m_Files.end()) {
      return m_FileRegister->getFile(iter->second);
    } else if (directory != nullptr) {
      DirectoryEntry *temp = findSubDirectory(path);
      if (temp != nullptr) {
        *directory = temp;
      }
    }
  } else {
    // file is in in a subdirectory, recurse into the matching subdirectory
    std::wstring pathComponent = path.substr(0, len);
    DirectoryEntry *temp = findSubDirectory(pathComponent);
    if (temp != nullptr) {
      if (len >= path.size()) {
        log("unexpected end of path");
        return FileEntry::Ptr();
      }
      return temp->searchFile(path.substr(len + 1), directory);
    }
  }
  return FileEntry::Ptr();
}


DirectoryEntry *DirectoryEntry::findSubDirectory(const std::wstring &name) const
{
  for (DirectoryEntry *entry : m_SubDirectories) {
    if (CaseInsensitiveEqual(entry->getName(), name)) {
      return entry;
    }
  }
  return nullptr;
}


DirectoryEntry *DirectoryEntry::findSubDirectoryRecursive(const std::wstring &path)
{
  return getSubDirectoryRecursive(path, false, -1);
}


const FileEntry::Ptr DirectoryEntry::findFile(const std::wstring &name) const
{
  auto iter = m_Files.find(ToLower(name));
  if (iter != m_Files.end()) {
    return m_FileRegister->getFile(iter->second);
  } else {
    return FileEntry::Ptr();
  }
}

DirectoryEntry *DirectoryEntry::getSubDirectory(const std::wstring &name, bool create, int originID)
{
  for (DirectoryEntry *entry : m_SubDirectories) {
    if (CaseInsensitiveEqual(entry->getName(), name)) {
      return entry;
    }
  }
  if (create) {
    std::vector<DirectoryEntry*>::iterator iter = m_SubDirectories.insert(m_SubDirectories.end(),
          new DirectoryEntry(name, this, originID, m_FileRegister, m_OriginConnection));
    return *iter;
  } else {
    return nullptr;
  }
}


DirectoryEntry *DirectoryEntry::getSubDirectoryRecursive(const std::wstring &path, bool create, int originID)
{
  if (path.length() == 0) {
    // path ended with a backslash?
    return this;
  }

  size_t pos = path.find_first_of(L"\\/");
  if (pos == std::wstring::npos) {
    return getSubDirectory(path, create);
  } else {
    DirectoryEntry *nextChild = getSubDirectory(path.substr(0, pos), create, originID);
    if (nextChild == nullptr) {
      return nullptr;
    } else {
      return nextChild->getSubDirectoryRecursive(path.substr(pos + 1), create, originID);
    }
  }
}



FileRegister::FileRegister(boost::shared_ptr<OriginConnection> originConnection)
  : m_OriginConnection(originConnection)
{
  LEAK_TRACE;
}

FileRegister::~FileRegister()
{
  LEAK_UNTRACE;
  m_Files.clear();
}


FileEntry::Index FileRegister::generateIndex()
{
  static std::atomic<FileEntry::Index> sIndex(0);
  return sIndex++;
}

bool FileRegister::indexValid(FileEntry::Index index) const
{
  return m_Files.find(index) != m_Files.end();
}

FileEntry::Ptr FileRegister::createFile(const std::wstring &name, DirectoryEntry *parent)
{
  FileEntry::Index index = generateIndex();
  m_Files[index] = FileEntry::Ptr(new FileEntry(index, name, parent));
  return m_Files[index];
}


FileEntry::Ptr FileRegister::getFile(FileEntry::Index index) const
{
  auto iter = m_Files.find(index);
  if (iter != m_Files.end()) {
    return iter->second;
  } else {
    return FileEntry::Ptr();
  }
}

void FileRegister::unregisterFile(FileEntry::Ptr file)
{
  bool ignore;
  // unregister from origin
  int originID = file->getOrigin(ignore);
  m_OriginConnection->getByID(originID).removeFile(file->getIndex());
  const std::vector<std::pair<int, std::pair<std::wstring, int>>> &alternatives = file->getAlternatives();
  for (auto iter = alternatives.begin(); iter != alternatives.end(); ++iter) {
    m_OriginConnection->getByID(iter->first).removeFile(file->getIndex());
  }

  // unregister from directory
  if (file->getParent() != nullptr) {
    file->getParent()->removeFile(file->getIndex());
  }
}


bool FileRegister::removeFile(FileEntry::Index index)
{
  auto iter = m_Files.find(index);
  if (iter != m_Files.end()) {
    unregisterFile(iter->second);
    m_Files.erase(index);
    return true;
  } else {
    log("invalid file index for remove: %lu", index);
    return false;
  }
}

void FileRegister::removeOrigin(FileEntry::Index index, int originID)
{
  auto iter = m_Files.find(index);
  if (iter != m_Files.end()) {
    if (iter->second->removeOrigin(originID)) {
      unregisterFile(iter->second);
      m_Files.erase(iter);
    }
  } else {
    log("invalid file index for remove (for origin): %lu", index);
  }
}

void FileRegister::removeOriginMulti(std::set<FileEntry::Index> indices, int originID, time_t notAfter)
{
  std::vector<FileEntry::Ptr> removedFiles;
  for (auto iter = indices.begin(); iter != indices.end();) {
    auto pos = m_Files.find(*iter);
    if (pos != m_Files.end()
        && (pos->second->lastAccessed() < notAfter)
        && pos->second->removeOrigin(originID)) {
      removedFiles.push_back(pos->second);
      m_Files.erase(pos);
      ++iter;
    } else {
      indices.erase(iter++);
    }
  }

  // optimization: this is only called when disabling an origin and in this case we don't have
  // to remove the file from the origin

  // need to remove files from their parent directories. multiple ways to go about this:
  // a) for each file, search its parents file-list (preferably by name) and remove what is found
  // b) gather the parent directories, go through the file list for each once and remove all files that have been removed
  // the latter should be faster when there are many files in few directories. since this is called
  // only when disabling an origin that is probably frequently the case
  std::set<DirectoryEntry*> parents;
  for (const FileEntry::Ptr &file : removedFiles) {
    if (file->getParent() != nullptr) {
      parents.insert(file->getParent());
    }
  }
  for (DirectoryEntry *parent : parents) {
    parent->removeFiles(indices);
  }
}

void FileRegister::sortOrigins()
{
  for (auto iter = m_Files.begin(); iter != m_Files.end(); ++iter) {
    iter->second->sortOrigins();
  }
}

} // namespace MOShared
