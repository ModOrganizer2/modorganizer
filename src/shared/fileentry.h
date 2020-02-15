#ifndef MO_REGISTER_FILEENTRY_INCLUDED
#define MO_REGISTER_FILEENTRY_INCLUDED

#include "fileregisterfwd.h"

namespace MOShared
{

class FileEntry
{
public:
  static constexpr uint64_t NoFileSize =
    std::numeric_limits<uint64_t>::max();

  typedef unsigned int Index;
  typedef boost::shared_ptr<FileEntry> Ptr;

  FileEntry();
  FileEntry(Index index, std::wstring name, DirectoryEntry *parent);

  // noncopyable
  FileEntry(const FileEntry&) = delete;
  FileEntry& operator=(const FileEntry&) = delete;

  Index getIndex() const
  {
    return m_Index;
  }

  void addOrigin(
    int origin, FILETIME fileTime, std::wstring_view archive, int order);

  // remove the specified origin from the list of origins that contain this
  // file. if no origin is left, the file is effectively deleted and true is
  // returned. otherwise, false is returned
  bool removeOrigin(int origin);

  void sortOrigins();

  // gets the list of alternative origins (origins with lower priority than
  // the primary one). if sortOrigins has been called, it is sorted by priority
  // (ascending)
  const AlternativesVector &getAlternatives() const
  {
    return m_Alternatives;
  }

  const std::wstring &getName() const
  {
    return m_Name;
  }

  int getOrigin() const
  {
    return m_Origin;
  }

  int getOrigin(bool &archive) const
  {
    archive = (m_Archive.first.length() != 0);
    return m_Origin;
  }

  const std::pair<std::wstring, int> &getArchive() const
  {
    return m_Archive;
  }

  bool isFromArchive(std::wstring archiveName = L"") const;

  // if originID is -1, uses the main origin; if this file doesn't exist in the
  // given origin, returns an empty string
  //
  std::wstring getFullPath(int originID=-1) const;

  std::wstring getRelativePath() const;

  DirectoryEntry *getParent()
  {
    return m_Parent;
  }

  void setFileTime(FILETIME fileTime) const
  {
    m_FileTime = fileTime;
  }

  FILETIME getFileTime() const
  {
    return m_FileTime;
  }

  void setFileSize(uint64_t size, uint64_t compressedSize)
  {
    m_FileSize = size;
    m_CompressedFileSize = compressedSize;
  }

  uint64_t getFileSize() const
  {
    return m_FileSize;
  }

  uint64_t getCompressedFileSize() const
  {
    return m_CompressedFileSize;
  }

private:
  Index m_Index;
  std::wstring m_Name;
  int m_Origin;
  std::pair<std::wstring, int> m_Archive;
  AlternativesVector m_Alternatives;
  DirectoryEntry *m_Parent;
  mutable FILETIME m_FileTime;
  uint64_t m_FileSize, m_CompressedFileSize;
  mutable std::mutex m_OriginsMutex;

  bool recurseParents(std::wstring &path, const DirectoryEntry *parent) const;
};

} // namespace

#endif // MO_REGISTER_FILEENTRY_INCLUDED
