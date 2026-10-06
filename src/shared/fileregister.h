#ifndef MO_REGISTER_FILESREGISTER_INCLUDED
#define MO_REGISTER_FILESREGISTER_INCLUDED

#include "fileregisterfwd.h"
#include <boost/shared_ptr.hpp>
#include <mutex>
#include <shared_mutex>

namespace MOShared
{

class FileRegister
{
public:
  FileRegister(boost::shared_ptr<OriginConnection> originConnection);

  // noncopyable
  FileRegister(const FileRegister&)            = delete;
  FileRegister& operator=(const FileRegister&) = delete;

  bool indexValid(FileIndex index) const;

  FileEntryPtr createFile(std::wstring name, DirectoryEntry* parent,
                          DirectoryStats& stats);

  FileEntryPtr getFile(FileIndex index) const;

  size_t highestCount() const
  {
    // m_NextIndex is the number of files ever created (dense, monotonic), which
    // is the true count even though m_Files may be over-sized by chunk growth in
    // createFile.
    return m_NextIndex;
  }

  bool removeFile(FileIndex index);
  void removeOrigin(FileIndex index, OriginID originID);
  void removeOriginMulti(std::set<FileIndex> indices, OriginID originID);

  void sortOrigins();

  // Serializes origin-data access (sortOrigins vs. the FileEntry readers
  // getFullPath/isFromArchive) so that sortOrigins can run without taking each
  // FileEntry's own mutex per entry. sortOrigins takes this exclusively for the
  // whole pass; readers take it shared in addition to their per-entry lock.
  // This is intentionally separate from m_Mutex (which guards m_Files) and is
  // never held while m_Mutex is taken, to avoid lock-order coupling.
  std::shared_mutex& originsSortMutex() const { return m_OriginsSortMutex; }

private:
  using FileMap = std::deque<FileEntryPtr>;

  // shared_mutex, not plain mutex: getFile() and createFile()'s element assign
  // take it SHARED so the parallel mod-loading threads don't serialize (each
  // createFile writes a unique, not-yet-visible index, and the container only
  // ever grows). Only the rare deque resize and the remove* paths take it
  // EXCLUSIVE.
  mutable std::shared_mutex m_Mutex;
  mutable std::shared_mutex m_OriginsSortMutex;
  FileMap m_Files;
  boost::shared_ptr<OriginConnection> m_OriginConnection;
  std::atomic<FileIndex> m_NextIndex;

  void unregisterFile(FileEntryPtr file);
  FileIndex generateIndex();
};

}  // namespace MOShared

#endif  // MO_REGISTER_FILESREGISTER_INCLUDED
