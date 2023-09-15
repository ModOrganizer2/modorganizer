#ifndef MO_REGISTER_FILESREGISTER_INCLUDED
#define MO_REGISTER_FILESREGISTER_INCLUDED

#include "fileregisterfwd.h"
#include <boost/shared_ptr.hpp>
#include <mutex>

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
    std::scoped_lock lock(m_Mutex);
    return m_Files.size();
  }

  bool removeFile(FileIndex index);
  void removeOrigin(FileIndex index, OriginID originID);
  void removeOriginMulti(std::set<FileIndex> indices, OriginID originID);

  void sortOrigins();

private:
  using FileMap = std::deque<FileEntryPtr>;

  mutable std::mutex m_Mutex;
  FileMap m_Files;
  boost::shared_ptr<OriginConnection> m_OriginConnection;
  std::atomic<FileIndex> m_NextIndex;

  void unregisterFile(FileEntryPtr file);
  FileIndex generateIndex();
};

}  // namespace MOShared

#endif  // MO_REGISTER_FILESREGISTER_INCLUDED
