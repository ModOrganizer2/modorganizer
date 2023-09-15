#ifndef MO_REGISTER_FILESORIGIN_INCLUDED
#define MO_REGISTER_FILESORIGIN_INCLUDED

#include "fileregisterfwd.h"

namespace MOShared
{

// represents a mod or the data directory, providing files to the tree
class FilesOrigin
{
public:
  FilesOrigin();

  FilesOrigin(OriginID ID, const std::wstring& name, const std::wstring& path,
              int priority, boost::shared_ptr<FileRegister> fileRegister,
              boost::shared_ptr<OriginConnection> originConnection);

  // noncopyable
  FilesOrigin(const FilesOrigin&)            = delete;
  FilesOrigin& operator=(const FilesOrigin&) = delete;

  // sets priority for this origin (does not automatically refresh
  // the structure)
  void setPriority(int priority);

  int getPriority() const { return m_Priority; }

  void setName(const std::wstring& name);
  const std::wstring& getName() const { return m_Name; }

  OriginID getID() const { return m_ID; }

  const std::wstring& getPath() const { return m_Path; }

  std::vector<FileEntryPtr> getFiles() const;
  FileEntryPtr findFile(FileIndex index) const;

  void enable(bool enabled, DirectoryStats& stats);
  void enable(bool enabled);

  bool isDisabled() const { return m_Disabled; }

  void addFile(FileIndex index)
  {
    std::scoped_lock lock(m_Mutex);
    m_Files.insert(index);
  }

  void removeFile(FileIndex index);

  bool containsArchive(std::wstring archiveName);

private:
  OriginID m_ID;
  bool m_Disabled;
  std::set<FileIndex> m_Files;
  std::wstring m_Name;
  std::wstring m_Path;
  int m_Priority;
  boost::weak_ptr<FileRegister> m_FileRegister;
  boost::weak_ptr<OriginConnection> m_OriginConnection;
  mutable std::mutex m_Mutex;
};

}  // namespace MOShared

#endif  // MO_REGISTER_FILESORIGIN_INCLUDED
