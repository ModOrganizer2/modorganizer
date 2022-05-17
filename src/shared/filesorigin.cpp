#include "filesorigin.h"
#include "fileentry.h"
#include "fileregister.h"
#include "originconnection.h"

namespace MOShared
{

std::wstring tail(const std::wstring& source, const size_t count)
{
  if (count >= source.length()) {
    return source;
  }

  return source.substr(source.length() - count);
}

FilesOrigin::FilesOrigin()
    : m_ID(0), m_Disabled(false), m_Name(), m_Path(), m_Priority(0)
{}

FilesOrigin::FilesOrigin(OriginID ID, const std::wstring& name,
                         const std::wstring& path, int priority,
                         boost::shared_ptr<MOShared::FileRegister> fileRegister,
                         boost::shared_ptr<MOShared::OriginConnection> originConnection)
    : m_ID(ID), m_Disabled(false), m_Name(name), m_Path(path), m_Priority(priority),
      m_FileRegister(fileRegister), m_OriginConnection(originConnection)
{}

void FilesOrigin::setPriority(int priority)
{
  m_Priority = priority;
}

void FilesOrigin::setName(const std::wstring& name)
{
  m_OriginConnection.lock()->changeNameLookup(m_Name, name);

  // change path too
  if (tail(m_Path, m_Name.length()) == m_Name) {
    m_Path = m_Path.substr(0, m_Path.length() - m_Name.length()).append(name);
  }

  m_Name = name;
}

std::vector<FileEntryPtr> FilesOrigin::getFiles() const
{
  std::vector<FileEntryPtr> result;

  {
    std::scoped_lock lock(m_Mutex);

    for (FileIndex fileIdx : m_Files) {
      if (FileEntryPtr p = m_FileRegister.lock()->getFile(fileIdx)) {
        result.push_back(p);
      }
    }
  }

  return result;
}

FileEntryPtr FilesOrigin::findFile(FileIndex index) const
{
  return m_FileRegister.lock()->getFile(index);
}

void FilesOrigin::enable(bool enabled)
{
  DirectoryStats dummy;
  enable(enabled, dummy);
}

void FilesOrigin::enable(bool enabled, DirectoryStats& stats)
{
  if (!enabled) {
    ++stats.originsNeededEnabled;

    std::set<FileIndex> copy;

    {
      std::scoped_lock lock(m_Mutex);
      copy = m_Files;
      m_Files.clear();
    }

    m_FileRegister.lock()->removeOriginMulti(copy, m_ID);
  }

  m_Disabled = !enabled;
}

void FilesOrigin::removeFile(FileIndex index)
{
  std::scoped_lock lock(m_Mutex);

  auto iter = m_Files.find(index);

  if (iter != m_Files.end()) {
    m_Files.erase(iter);
  }
}

bool FilesOrigin::containsArchive(std::wstring archiveName)
{
  std::scoped_lock lock(m_Mutex);

  for (FileIndex fileIdx : m_Files) {
    if (FileEntryPtr p = m_FileRegister.lock()->getFile(fileIdx)) {
      if (p->isFromArchive(archiveName)) {
        return true;
      }
    }
  }

  return false;
}

}  // namespace MOShared
