#include "fileregister.h"
#include "directoryentry.h"
#include "fileentry.h"
#include "filesorigin.h"
#include "originconnection.h"
#include <boost/make_shared.hpp>
#include <log.h>

namespace MOShared
{

using namespace MOBase;

FileRegister::FileRegister(boost::shared_ptr<OriginConnection> originConnection)
    : m_OriginConnection(originConnection), m_NextIndex(0)
{}

bool FileRegister::indexValid(FileIndex index) const
{
  std::shared_lock lock(m_Mutex);

  if (index < m_Files.size()) {
    return (m_Files[index].get() != nullptr);
  }

  return false;
}

FileEntryPtr FileRegister::createFile(std::wstring name, DirectoryEntry* parent,
                                      DirectoryStats& stats)
{
  const auto index = generateIndex();
  auto p           = boost::make_shared<FileEntry>(index, std::move(name), parent);

  // Fast path: if the slot already exists, assign it under a SHARED lock. This
  // is safe because the container only ever grows (so the slot stays valid), the
  // index is unique and not yet visible to any reader, and a shared lock still
  // excludes the exclusive resize below - so the deque's structure can't change
  // underneath us. Concurrent createFile/getFile calls touch distinct elements,
  // so they no longer serialize.
  {
    std::shared_lock lock(m_Mutex);
    if (index < m_Files.size()) {
      m_Files[index] = p;
      return p;
    }
  }

  // Slow path: the deque needs to grow, which is a structural change and must be
  // exclusive. Grow in chunks (indices are dense and monotonic) so this runs
  // rarely; trailing unused slots stay null and are bounds/null-checked by every
  // accessor, and highestCount() reports the true count via m_NextIndex.
  {
    std::unique_lock lock(m_Mutex);
    if (index >= m_Files.size()) {
      constexpr size_t chunk = 4096;
      m_Files.resize(((index + chunk) / chunk) * chunk);
    }
    m_Files[index] = p;
  }

  return p;
}

FileIndex FileRegister::generateIndex()
{
  return m_NextIndex++;
}

FileEntryPtr FileRegister::getFile(FileIndex index) const
{
  std::shared_lock lock(m_Mutex);

  if (index < m_Files.size()) {
    return m_Files[index];
  } else {
    return {};
  }
}

bool FileRegister::removeFile(FileIndex index)
{
  std::scoped_lock lock(m_Mutex);

  if (index < m_Files.size()) {
    FileEntryPtr p;
    m_Files[index].swap(p);

    if (p) {
      unregisterFile(p);
      return true;
    }
  }

  log::error("{}: {}", QObject::tr("invalid file index for remove"), index);
  return false;
}

void FileRegister::removeOrigin(FileIndex index, OriginID originID)
{
  std::unique_lock lock(m_Mutex);

  if (index < m_Files.size()) {
    FileEntryPtr& p = m_Files[index];

    if (p) {
      if (p->removeOrigin(originID)) {
        m_Files[index] = {};
        lock.unlock();
        unregisterFile(p);
        return;
      }
    }
  }

  log::error("{}: {}", QObject::tr("invalid file index for remove (for origin)"),
             index);
}

void FileRegister::removeOriginMulti(std::set<FileIndex> indices, OriginID originID)
{
  std::vector<FileEntryPtr> removedFiles;

  {
    std::scoped_lock lock(m_Mutex);

    for (auto iter = indices.begin(); iter != indices.end();) {
      const auto index = *iter;

      if (index < m_Files.size()) {
        const auto& p = m_Files[index];

        if (p && p->removeOrigin(originID)) {
          removedFiles.push_back(p);
          m_Files[index] = {};
          ++iter;
          continue;
        }
      }

      iter = indices.erase(iter);
    }
  }

  // optimization: this is only called when disabling an origin and in this case
  // we don't have to remove the file from the origin

  // need to remove files from their parent directories. multiple ways to go
  // about this:
  //   a) for each file, search its parents file-list (preferably by name) and
  //      remove what is found
  //   b) gather the parent directories, go through the file list for each once
  //      and remove all files that have been removed
  //
  // the latter should be faster when there are many files in few directories.
  // since this is called only when disabling an origin that is probably
  // frequently the case

  std::set<DirectoryEntry*> parents;
  for (const FileEntryPtr& file : removedFiles) {
    if (file->getParent() != nullptr) {
      parents.insert(file->getParent());
    }
  }

  for (DirectoryEntry* parent : parents) {
    parent->removeFiles(indices);
  }
}

void FileRegister::sortOrigins()
{
  // m_Mutex guards the m_Files container during iteration; m_OriginsSortMutex is
  // held exclusively for the whole pass so each FileEntry::sortOrigins no longer
  // needs to lock its own per-entry mutex. Readers take m_OriginsSortMutex
  // shared, so they are correctly excluded for the duration of the sort.
  std::scoped_lock lock(m_Mutex, m_OriginsSortMutex);

  for (auto&& p : m_Files) {
    if (p) {
      p->sortOrigins();
    }
  }
}

void FileRegister::unregisterFile(FileEntryPtr file)
{
  bool ignore;

  // unregister from origin
  OriginID originID = file->getOrigin(ignore);
  m_OriginConnection->getByID(originID).removeFile(file->getIndex());
  const auto& alternatives = file->getAlternatives();

  for (const auto& alt : alternatives) {
    m_OriginConnection->getByID(alt.originID()).removeFile(file->getIndex());
  }

  // unregister from directory
  if (file->getParent() != nullptr) {
    file->getParent()->removeFile(file->getIndex());
  }
}

}  // namespace MOShared
