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
#include "../envfs.h"
#include "fileentry.h"
#include "filesorigin.h"
#include "originconnection.h"
#include "util.h"
#include "windows_error.h"
#include <filesystem>
#include <log.h>
#include <utility.h>

namespace MOShared
{

using namespace MOBase;
const int MAXPATH_UNICODE = 32767;

template <class F>
void elapsedImpl(std::chrono::nanoseconds& out, F&& f)
{
  if constexpr (DirectoryStats::EnableInstrumentation) {
    const auto start = std::chrono::high_resolution_clock::now();
    f();
    const auto end = std::chrono::high_resolution_clock::now();
    out += (end - start);
  } else {
    f();
  }
}

// elapsed() is not optimized out when EnableInstrumentation is false even
// though it's equivalent that this macro
#define elapsed(OUT, F) (F)();
// #define elapsed(OUT, F) elapsedImpl(OUT, F);

static bool SupportOptimizedFind()
{
  // large fetch and basic info for FindFirstFileEx is supported on win server 2008 r2,
  // win 7 and newer

  OSVERSIONINFOEX versionInfo;
  versionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  versionInfo.dwMajorVersion      = 6;
  versionInfo.dwMinorVersion      = 1;

  ULONGLONG mask = ::VerSetConditionMask(
      ::VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL), VER_MINORVERSION,
      VER_GREATER_EQUAL);

  return (::VerifyVersionInfo(&versionInfo, VER_MAJORVERSION | VER_MINORVERSION,
                              mask) == TRUE);
}

bool DirCompareByName::operator()(const DirectoryEntry* lhs,
                                  const DirectoryEntry* rhs) const
{
  return _wcsicmp(lhs->getName().c_str(), rhs->getName().c_str()) < 0;
}

DirectoryEntry::DirectoryEntry(std::wstring name, DirectoryEntry* parent, int originID)
    : m_OriginConnection(new OriginConnection), m_Name(std::move(name)),
      m_Parent(parent), m_Populated(false), m_TopLevel(true)
{
  m_FileRegister.reset(new FileRegister(m_OriginConnection));
  m_Origins.insert(originID);
}

DirectoryEntry::DirectoryEntry(std::wstring name, DirectoryEntry* parent, int originID,
                               boost::shared_ptr<FileRegister> fileRegister,
                               boost::shared_ptr<OriginConnection> originConnection)
    : m_FileRegister(fileRegister), m_OriginConnection(originConnection),
      m_Name(std::move(name)), m_Parent(parent), m_Populated(false), m_TopLevel(false)
{
  m_Origins.insert(originID);
}

DirectoryEntry::~DirectoryEntry()
{
  clear();
}

void DirectoryEntry::clear()
{
  for (auto itor = m_SubDirectories.rbegin(); itor != m_SubDirectories.rend(); ++itor) {
    delete *itor;
  }

  m_Files.clear();
  m_FilesLookup.clear();
  m_SubDirectories.clear();
  m_SubDirectoriesLookup.clear();
}

void DirectoryEntry::addFromOrigin(const std::wstring& originName,
                                   const std::wstring& directory, int priority,
                                   DirectoryStats& stats)
{
  env::DirectoryWalker walker;
  addFromOrigin(walker, originName, directory, priority, stats);
}

void DirectoryEntry::addFromOrigin(env::DirectoryWalker& walker,
                                   const std::wstring& originName,
                                   const std::wstring& directory, int priority,
                                   DirectoryStats& stats)
{
  FilesOrigin& origin = createOrigin(originName, directory, priority, stats);

  if (!directory.empty()) {
    addFiles(walker, origin, directory, stats);
  }

  m_Populated = true;
}

void DirectoryEntry::addFromList(const std::wstring& originName,
                                 const std::wstring& directory, env::Directory& root,
                                 int priority, DirectoryStats& stats)
{
  stats = {};

  FilesOrigin& origin = createOrigin(originName, directory, priority, stats);
  addDir(origin, root, stats);
}

void DirectoryEntry::addDir(FilesOrigin& origin, env::Directory& d,
                            DirectoryStats& stats)
{
  elapsed(stats.dirTimes, [&] {
    for (auto& sd : d.dirs) {
      auto* sdirEntry = getSubDirectory(sd, true, stats, origin.getID());
      sdirEntry->addDir(origin, sd, stats);
    }
  });

  elapsed(stats.fileTimes, [&] {
    for (auto& f : d.files) {
      insert(f, origin, L"", -1, stats);
    }
  });

  m_Populated = true;
}

void DirectoryEntry::addFromAllBSAs(const std::wstring& originName,
                                    const std::wstring& directory, int priority,
                                    const std::vector<std::wstring>& archives,
                                    const std::set<std::wstring>& enabledArchives,
                                    const std::vector<std::wstring>& loadOrder,
                                    DirectoryStats& stats)
{
  for (const auto& archive : archives) {
    const std::filesystem::path archivePath(archive);
    const auto filename = archivePath.filename().native();

    if (!enabledArchives.contains(filename)) {
      continue;
    }

    const auto filenameLc = ToLowerCopy(filename);

    int order = -1;

    for (auto plugin : loadOrder) {
      const auto pluginNameLc =
          ToLowerCopy(std::filesystem::path(plugin).stem().native());

      if (filenameLc.starts_with(pluginNameLc + L" - ") ||
          filenameLc.starts_with(pluginNameLc + L".")) {
        auto itor = std::find(loadOrder.begin(), loadOrder.end(), plugin);
        if (itor != loadOrder.end()) {
          order = std::distance(loadOrder.begin(), itor);
        }
      }
    }

    addFromBSA(originName, directory, archivePath.native(), priority, order, stats);
  }
}

void DirectoryEntry::addFromBSA(const std::wstring& originName,
                                const std::wstring& directory,
                                const std::wstring& archivePath, int priority,
                                int order, DirectoryStats& stats)
{
  FilesOrigin& origin    = createOrigin(originName, directory, priority, stats);
  const auto archiveName = std::filesystem::path(archivePath).filename().native();

  if (containsArchive(archiveName)) {
    return;
  }

  BSA::Archive archive;
  BSA::EErrorCode res = BSA::ERROR_NONE;

  try {
    // read() can return an error, but it can also throw if the file is not a
    // valid bsa
    res = archive.read(ToString(archivePath, false).c_str(), false);
  } catch (std::exception& e) {
    log::error("invalid bsa '{}', error {}", archivePath, e.what());
    return;
  }

  if ((res != BSA::ERROR_NONE) && (res != BSA::ERROR_INVALIDHASHES)) {
    log::error("invalid bsa '{}', error {}", archivePath, res);
    return;
  }

  std::error_code ec;
  const auto lwt = std::filesystem::last_write_time(archivePath, ec);
  FILETIME ft    = {};

  if (ec) {
    log::warn("failed to get last modified date for '{}', {}", archivePath,
              ec.message());
  } else {
    ft = ToFILETIME(lwt);
  }

  addFiles(origin, archive.getRoot(), ft, archiveName, order, stats);

  m_Populated = true;
}

void DirectoryEntry::propagateOrigin(int origin)
{
  {
    std::scoped_lock lock(m_OriginsMutex);
    m_Origins.insert(origin);
  }

  if (m_Parent != nullptr) {
    m_Parent->propagateOrigin(origin);
  }
}

bool DirectoryEntry::originExists(const std::wstring& name) const
{
  return m_OriginConnection->exists(name);
}

FilesOrigin& DirectoryEntry::getOriginByID(int ID) const
{
  return m_OriginConnection->getByID(ID);
}

FilesOrigin& DirectoryEntry::getOriginByName(const std::wstring& name) const
{
  return m_OriginConnection->getByName(name);
}

const FilesOrigin* DirectoryEntry::findOriginByID(int ID) const
{
  return m_OriginConnection->findByID(ID);
}

int DirectoryEntry::anyOrigin() const
{
  bool ignore;

  for (auto iter = m_Files.begin(); iter != m_Files.end(); ++iter) {
    FileEntryPtr entry = m_FileRegister->getFile(iter->second);
    if ((entry.get() != nullptr) && !entry->isFromArchive()) {
      return entry->getOrigin(ignore);
    }
  }

  // if we got here, no file directly within this directory is a valid indicator for a
  // mod, thus we continue looking in subdirectories
  for (DirectoryEntry* entry : m_SubDirectories) {
    int res = entry->anyOrigin();
    if (res != InvalidOriginID) {
      return res;
    }
  }

  return *(m_Origins.begin());
}

std::vector<FileEntryPtr> DirectoryEntry::getFiles() const
{
  std::vector<FileEntryPtr> result;
  result.reserve(m_Files.size());

  for (auto iter = m_Files.begin(); iter != m_Files.end(); ++iter) {
    result.push_back(m_FileRegister->getFile(iter->second));
  }

  return result;
}

DirectoryEntry* DirectoryEntry::findSubDirectory(const std::wstring& name,
                                                 bool alreadyLowerCase) const
{
  SubDirectoriesLookup::const_iterator itor;

  if (alreadyLowerCase) {
    itor = m_SubDirectoriesLookup.find(name);
  } else {
    itor = m_SubDirectoriesLookup.find(ToLowerCopy(name));
  }

  if (itor == m_SubDirectoriesLookup.end()) {
    return nullptr;
  }

  return itor->second;
}

DirectoryEntry* DirectoryEntry::findSubDirectoryRecursive(const std::wstring& path)
{
  DirectoryStats dummy;
  return getSubDirectoryRecursive(path, false, dummy, InvalidOriginID);
}

const FileEntryPtr DirectoryEntry::findFile(const std::wstring& name,
                                            bool alreadyLowerCase) const
{
  FilesLookup::const_iterator iter;

  if (alreadyLowerCase) {
    iter = m_FilesLookup.find(DirectoryEntryFileKey(name));
  } else {
    iter = m_FilesLookup.find(DirectoryEntryFileKey(ToLowerCopy(name)));
  }

  if (iter != m_FilesLookup.end()) {
    return m_FileRegister->getFile(iter->second);
  } else {
    return FileEntryPtr();
  }
}

const FileEntryPtr DirectoryEntry::findFile(const DirectoryEntryFileKey& key) const
{
  auto iter = m_FilesLookup.find(key);

  if (iter != m_FilesLookup.end()) {
    return m_FileRegister->getFile(iter->second);
  } else {
    return FileEntryPtr();
  }
}

bool DirectoryEntry::hasFile(const std::wstring& name) const
{
  return m_Files.contains(ToLowerCopy(name));
}

bool DirectoryEntry::containsArchive(std::wstring archiveName)
{
  for (auto iter = m_Files.begin(); iter != m_Files.end(); ++iter) {
    FileEntryPtr entry = m_FileRegister->getFile(iter->second);
    if (entry->isFromArchive(archiveName)) {
      return true;
    }
  }

  return false;
}

const FileEntryPtr DirectoryEntry::searchFile(const std::wstring& path,
                                              const DirectoryEntry** directory) const
{
  if (directory != nullptr) {
    *directory = nullptr;
  }

  if ((path.length() == 0) || (path == L"*")) {
    // no file name -> the path ended on a (back-)slash
    if (directory != nullptr) {
      *directory = this;
    }

    return FileEntryPtr();
  }

  const size_t len = path.find_first_of(L"\\/");

  if (len == std::string::npos) {
    // no more path components
    auto iter = m_Files.find(ToLowerCopy(path));

    if (iter != m_Files.end()) {
      return m_FileRegister->getFile(iter->second);
    } else if (directory != nullptr) {
      DirectoryEntry* temp = findSubDirectory(path);
      if (temp != nullptr) {
        *directory = temp;
      }
    }
  } else {
    // file is in a subdirectory, recurse into the matching subdirectory
    std::wstring pathComponent = path.substr(0, len);
    DirectoryEntry* temp       = findSubDirectory(pathComponent);

    if (temp != nullptr) {
      if (len >= path.size()) {
        log::error("{}", QObject::tr("unexpected end of path"));
        return FileEntryPtr();
      }

      return temp->searchFile(path.substr(len + 1), directory);
    }
  }

  return FileEntryPtr();
}

void DirectoryEntry::removeFile(FileIndex index)
{
  removeFileFromList(index);
}

bool DirectoryEntry::removeFile(const std::wstring& filePath, int* origin)
{
  size_t pos = filePath.find_first_of(L"\\/");

  if (pos == std::string::npos) {
    return this->remove(filePath, origin);
  }

  std::wstring dirName = filePath.substr(0, pos);
  std::wstring rest    = filePath.substr(pos + 1);

  DirectoryStats dummy;
  DirectoryEntry* entry = getSubDirectoryRecursive(dirName, false, dummy);

  if (entry != nullptr) {
    return entry->removeFile(rest, origin);
  } else {
    return false;
  }
}

void DirectoryEntry::removeDir(const std::wstring& path)
{
  size_t pos = path.find_first_of(L"\\/");

  if (pos == std::string::npos) {
    for (auto iter = m_SubDirectories.begin(); iter != m_SubDirectories.end(); ++iter) {
      DirectoryEntry* entry = *iter;

      if (CaseInsensitiveEqual(entry->getName(), path)) {
        entry->removeDirRecursive();
        removeDirectoryFromList(iter);
        delete entry;
        break;
      }
    }
  } else {
    std::wstring dirName = path.substr(0, pos);
    std::wstring rest    = path.substr(pos + 1);

    DirectoryStats dummy;
    DirectoryEntry* entry = getSubDirectoryRecursive(dirName, false, dummy);

    if (entry != nullptr) {
      entry->removeDir(rest);
    }
  }
}

bool DirectoryEntry::remove(const std::wstring& fileName, int* origin)
{
  const auto lcFileName = ToLowerCopy(fileName);

  auto iter = m_Files.find(lcFileName);
  bool b    = false;

  if (iter != m_Files.end()) {
    if (origin != nullptr) {
      FileEntryPtr entry = m_FileRegister->getFile(iter->second);
      if (entry.get() != nullptr) {
        bool ignore;
        *origin = entry->getOrigin(ignore);
      }
    }

    b = m_FileRegister->removeFile(iter->second);
  }

  return b;
}

bool DirectoryEntry::hasContentsFromOrigin(int originID) const
{
  return m_Origins.find(originID) != m_Origins.end();
}

FilesOrigin& DirectoryEntry::createOrigin(const std::wstring& originName,
                                          const std::wstring& directory, int priority,
                                          DirectoryStats& stats)
{
  auto r = m_OriginConnection->getOrCreate(originName, directory, priority,
                                           m_FileRegister, m_OriginConnection, stats);

  if (r.second) {
    ++stats.originCreate;
  } else {
    ++stats.originExists;
  }

  return r.first;
}

void DirectoryEntry::removeFiles(const std::set<FileIndex>& indices)
{
  removeFilesFromList(indices);
}

FileEntryPtr DirectoryEntry::insert(std::wstring_view fileName, FilesOrigin& origin,
                                    FILETIME fileTime, std::wstring_view archive,
                                    int order, DirectoryStats& stats)
{
  std::wstring fileNameLower = ToLowerCopy(fileName);
  FileEntryPtr fe;

  DirectoryEntryFileKey key(std::move(fileNameLower));

  {
    std::unique_lock lock(m_FilesMutex);

    FilesLookup::iterator itor;

    elapsed(stats.filesLookupTimes, [&] {
      itor = m_FilesLookup.find(key);
    });

    if (itor != m_FilesLookup.end()) {
      lock.unlock();
      ++stats.fileExists;
      fe = m_FileRegister->getFile(itor->second);
    } else {
      ++stats.fileCreate;
      fe = m_FileRegister->createFile(std::wstring(fileName.begin(), fileName.end()),
                                      this, stats);

      elapsed(stats.addFileTimes, [&] {
        addFileToList(std::move(key.value), fe->getIndex());
      });

      // fileNameLower has moved from this point
    }
  }

  elapsed(stats.addOriginToFileTimes, [&] {
    fe->addOrigin(origin.getID(), fileTime, archive, order);
  });

  elapsed(stats.addFileToOriginTimes, [&] {
    origin.addFile(fe->getIndex());
  });

  return fe;
}

FileEntryPtr DirectoryEntry::insert(env::File& file, FilesOrigin& origin,
                                    std::wstring_view archive, int order,
                                    DirectoryStats& stats)
{
  FileEntryPtr fe;

  {
    std::unique_lock lock(m_FilesMutex);

    FilesMap::iterator itor;

    elapsed(stats.filesLookupTimes, [&] {
      itor = m_Files.find(file.lcname);
    });

    if (itor != m_Files.end()) {
      lock.unlock();
      ++stats.fileExists;
      fe = m_FileRegister->getFile(itor->second);
    } else {
      ++stats.fileCreate;
      fe = m_FileRegister->createFile(std::move(file.name), this, stats);
      // file.name has been moved from this point

      elapsed(stats.addFileTimes, [&] {
        addFileToList(std::move(file.lcname), fe->getIndex());
      });

      // file.lcname has been moved from this point
    }
  }

  elapsed(stats.addOriginToFileTimes, [&] {
    fe->addOrigin(origin.getID(), file.lastModified, archive, order);
  });

  elapsed(stats.addFileToOriginTimes, [&] {
    origin.addFile(fe->getIndex());
  });

  return fe;
}

struct DirectoryEntry::Context
{
  FilesOrigin& origin;
  DirectoryStats& stats;
  std::stack<DirectoryEntry*> current;
};

void DirectoryEntry::addFiles(env::DirectoryWalker& walker, FilesOrigin& origin,
                              const std::wstring& path, DirectoryStats& stats)
{
  Context cx = {origin, stats};
  cx.current.push(this);

  if (std::filesystem::exists(path)) {
    walker.forEachEntry(
        path, &cx,
        [](void* pcx, std::wstring_view path) {
          onDirectoryStart((Context*)pcx, path);
        },

        [](void* pcx, std::wstring_view path) {
          onDirectoryEnd((Context*)pcx, path);
        },

        [](void* pcx, std::wstring_view path, FILETIME ft, uint64_t) {
          onFile((Context*)pcx, path, ft);
        });
  }
}

void DirectoryEntry::onDirectoryStart(Context* cx, std::wstring_view path)
{
  elapsed(cx->stats.dirTimes, [&] {
    auto* sd =
        cx->current.top()->getSubDirectory(path, true, cx->stats, cx->origin.getID());

    cx->current.push(sd);
  });
}

void DirectoryEntry::onDirectoryEnd(Context* cx, std::wstring_view path)
{
  elapsed(cx->stats.dirTimes, [&] {
    cx->current.pop();
  });
}

void DirectoryEntry::onFile(Context* cx, std::wstring_view path, FILETIME ft)
{
  elapsed(cx->stats.fileTimes, [&] {
    cx->current.top()->insert(path, cx->origin, ft, L"", -1, cx->stats);
  });
}

void DirectoryEntry::addFiles(FilesOrigin& origin, const BSA::Folder::Ptr archiveFolder,
                              FILETIME fileTime, const std::wstring& archiveName,
                              int order, DirectoryStats& stats)
{
  // add files
  const auto fileCount = archiveFolder->getNumFiles();
  for (unsigned int i = 0; i < fileCount; ++i) {
    const BSA::File::Ptr file = archiveFolder->getFile(i);

    auto f = insert(ToWString(file->getName(), true), origin, fileTime, archiveName,
                    order, stats);

    if (f) {
      if (file->getUncompressedFileSize() > 0) {
        f->setFileSize(file->getFileSize(), file->getUncompressedFileSize());
      } else {
        f->setFileSize(file->getFileSize(), FileEntry::NoFileSize);
      }
    }
  }

  // recurse into subdirectories
  const auto dirCount = archiveFolder->getNumSubFolders();
  for (unsigned int i = 0; i < dirCount; ++i) {
    const BSA::Folder::Ptr folder = archiveFolder->getSubFolder(i);

    DirectoryEntry* folderEntry = getSubDirectoryRecursive(
        ToWString(folder->getName(), true), true, stats, origin.getID());

    folderEntry->addFiles(origin, folder, fileTime, archiveName, order, stats);
  }
}

DirectoryEntry* DirectoryEntry::getSubDirectory(std::wstring_view name, bool create,
                                                DirectoryStats& stats, int originID)
{
  std::wstring nameLc = ToLowerCopy(name);

  std::scoped_lock lock(m_SubDirMutex);

  SubDirectoriesLookup::iterator itor;
  elapsed(stats.subdirLookupTimes, [&] {
    itor = m_SubDirectoriesLookup.find(nameLc);
  });

  if (itor != m_SubDirectoriesLookup.end()) {
    ++stats.subdirExists;
    return itor->second;
  }

  if (create) {
    ++stats.subdirCreate;

    auto* entry = new DirectoryEntry(std::wstring(name.begin(), name.end()), this,
                                     originID, m_FileRegister, m_OriginConnection);

    elapsed(stats.addDirectoryTimes, [&] {
      addDirectoryToList(entry, std::move(nameLc));
      // nameLc is moved from this point
    });

    return entry;
  } else {
    return nullptr;
  }
}

DirectoryEntry* DirectoryEntry::getSubDirectory(env::Directory& dir, bool create,
                                                DirectoryStats& stats, int originID)
{
  SubDirectoriesLookup::iterator itor;

  std::scoped_lock lock(m_SubDirMutex);

  elapsed(stats.subdirLookupTimes, [&] {
    itor = m_SubDirectoriesLookup.find(dir.lcname);
  });

  if (itor != m_SubDirectoriesLookup.end()) {
    ++stats.subdirExists;
    return itor->second;
  }

  if (create) {
    ++stats.subdirCreate;

    auto* entry = new DirectoryEntry(std::move(dir.name), this, originID,
                                     m_FileRegister, m_OriginConnection);
    // dir.name is moved from this point

    elapsed(stats.addDirectoryTimes, [&] {
      addDirectoryToList(entry, std::move(dir.lcname));
    });

    // dir.lcname is moved from this point

    return entry;
  } else {
    return nullptr;
  }
}

DirectoryEntry* DirectoryEntry::getSubDirectoryRecursive(const std::wstring& path,
                                                         bool create,
                                                         DirectoryStats& stats,
                                                         int originID)
{
  if (path.length() == 0) {
    // path ended with a backslash?
    return this;
  }

  const size_t pos = path.find_first_of(L"\\/");

  if (pos == std::wstring::npos) {
    return getSubDirectory(path, create, stats);
  } else {
    DirectoryEntry* nextChild =
        getSubDirectory(path.substr(0, pos), create, stats, originID);

    if (nextChild == nullptr) {
      return nullptr;
    } else {
      return nextChild->getSubDirectoryRecursive(path.substr(pos + 1), create, stats,
                                                 originID);
    }
  }
}

void DirectoryEntry::removeDirRecursive()
{
  while (!m_Files.empty()) {
    m_FileRegister->removeFile(m_Files.begin()->second);
  }

  m_FilesLookup.clear();

  for (DirectoryEntry* entry : m_SubDirectories) {
    entry->removeDirRecursive();
    delete entry;
  }

  m_SubDirectories.clear();
  m_SubDirectoriesLookup.clear();
}

void DirectoryEntry::addDirectoryToList(DirectoryEntry* e, std::wstring nameLc)
{
  m_SubDirectories.insert(e);
  m_SubDirectoriesLookup.emplace(std::move(nameLc), e);
}

void DirectoryEntry::removeDirectoryFromList(SubDirectories::iterator itor)
{
  const auto* entry = *itor;

  {
    auto itor2 = std::find_if(m_SubDirectoriesLookup.begin(),
                              m_SubDirectoriesLookup.end(), [&](auto&& d) {
                                return (d.second == entry);
                              });

    if (itor2 == m_SubDirectoriesLookup.end()) {
      log::error("entry {} not in sub directories map", entry->getName());
    } else {
      m_SubDirectoriesLookup.erase(itor2);
    }
  }

  m_SubDirectories.erase(itor);
}

void DirectoryEntry::removeFileFromList(FileIndex index)
{
  auto removeFrom = [&](auto& list) {
    auto iter = std::find_if(list.begin(), list.end(), [&index](auto&& pair) {
      return (pair.second == index);
    });

    if (iter == list.end()) {
      auto f = m_FileRegister->getFile(index);

      if (f) {
        log::error("can't remove file '{}', not in directory entry '{}'", f->getName(),
                   getName());
      } else {
        log::error("can't remove file with index {}, not in directory entry '{}' and "
                   "not in register",
                   index, getName());
      }
    } else {
      list.erase(iter);
    }
  };

  removeFrom(m_FilesLookup);
  removeFrom(m_Files);
}

void DirectoryEntry::removeFilesFromList(const std::set<FileIndex>& indices)
{
  for (auto iter = m_Files.begin(); iter != m_Files.end();) {
    if (indices.find(iter->second) != indices.end()) {
      iter = m_Files.erase(iter);
    } else {
      ++iter;
    }
  }

  for (auto iter = m_FilesLookup.begin(); iter != m_FilesLookup.end();) {
    if (indices.find(iter->second) != indices.end()) {
      iter = m_FilesLookup.erase(iter);
    } else {
      ++iter;
    }
  }
}

void DirectoryEntry::addFileToList(std::wstring fileNameLower, FileIndex index)
{
  m_FilesLookup.emplace(fileNameLower, index);
  m_Files.emplace(std::move(fileNameLower), index);
  // fileNameLower has been moved from this point
}

struct DumpFailed : public std::runtime_error
{
  using runtime_error::runtime_error;
};

void DirectoryEntry::dump(const std::wstring& file) const
{
  try {
    std::FILE* f = nullptr;
    auto e       = _wfopen_s(&f, file.c_str(), L"wb");

    if (e != 0 || !f) {
      throw DumpFailed(std::format("failed to open, {} ({})", std::strerror(e), e));
    }

    Guard g([&] {
      std::fclose(f);
    });

    dump(f, L"Data");
  } catch (DumpFailed& e) {
    log::error("failed to write list to '{}': {}",
               QString::fromStdWString(file).toStdString(), e.what());
  }
}

void DirectoryEntry::dump(std::FILE* f, const std::wstring& parentPath) const
{
  {
    std::scoped_lock lock(m_FilesMutex);

    for (auto&& index : m_Files) {
      const auto file = m_FileRegister->getFile(index.second);
      if (!file) {
        continue;
      }

      if (file->isFromArchive()) {
        // TODO: don't list files from archives. maybe make this an option?
        continue;
      }

      const auto& o   = m_OriginConnection->getByID(file->getOrigin());
      const auto path = parentPath + L"\\" + file->getName();
      const auto line = path + L"\t(" + o.getName() + L")\r\n";

      const auto lineu8 = MOShared::ToString(line, true);

      if (std::fwrite(lineu8.data(), lineu8.size(), 1, f) != 1) {
        const auto e = errno;
        throw DumpFailed(std::format("failed to write, {} ({})", std::strerror(e), e));
      }
    }
  }

  {
    std::scoped_lock lock(m_SubDirMutex);
    for (auto&& d : m_SubDirectories) {
      const auto path = parentPath + L"\\" + d->m_Name;
      d->dump(f, path);
    }
  }
}

}  // namespace MOShared
