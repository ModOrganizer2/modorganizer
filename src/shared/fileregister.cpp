#include "fileregister.h"
#include "directoryentry.h"
#include "fileentry.h"
#include "filesorigin.h"
#include "originconnection.h"
#include <log.h>

namespace MOShared
{

using namespace MOBase;

FileRegister::FileRegister(boost::shared_ptr<OriginConnection> originConnection)
    : m_OriginConnection(originConnection), m_NextIndex(0)
{}

bool FileRegister::indexValid(FileIndex index) const
{
  std::scoped_lock lock(m_Mutex);

  if (index < m_Files.size()) {
    return (m_Files[index].get() != nullptr);
  }

  return false;
}

FileEntryPtr FileRegister::createFile(std::wstring name, DirectoryEntry* parent,
                                      DirectoryStats& stats)
{
  const auto index = generateIndex();
  auto p           = FileEntryPtr(new FileEntry(index, std::move(name), parent));

  {
    std::scoped_lock lock(m_Mutex);

    if (index >= m_Files.size()) {
      m_Files.resize(index + 1);
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
  std::scoped_lock lock(m_Mutex);

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
  std::scoped_lock lock(m_Mutex);

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
