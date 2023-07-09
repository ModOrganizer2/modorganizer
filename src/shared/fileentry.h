#ifndef MO_REGISTER_FILEENTRY_INCLUDED
#define MO_REGISTER_FILEENTRY_INCLUDED

#include "fileregisterfwd.h"

namespace MOShared
{

class FileEntry
{
public:
  static constexpr uint64_t NoFileSize = std::numeric_limits<uint64_t>::max();

  FileEntry();
  FileEntry(FileIndex index, std::wstring name, DirectoryEntry* parent);

  // noncopyable
  FileEntry(const FileEntry&)            = delete;
  FileEntry& operator=(const FileEntry&) = delete;

  FileIndex getIndex() const { return m_Index; }

  void addOrigin(OriginID origin, FILETIME fileTime, std::wstring_view archive,
                 int order);

  // remove the specified origin from the list of origins that contain this
  // file. if no origin is left, the file is effectively deleted and true is
  // returned. otherwise, false is returned
  bool removeOrigin(OriginID origin);

  void sortOrigins();

  // gets the list of alternative origins (origins with lower priority than
  // the primary one). if sortOrigins has been called, it is sorted by priority
  // (ascending)
  const AlternativesVector& getAlternatives() const { return m_Alternatives; }

  const std::wstring& getName() const { return m_Name; }

  OriginID getOrigin() const { return m_Origin; }

  OriginID getOrigin(bool& archive) const
  {
    archive = m_Archive.isValid();
    return m_Origin;
  }

  const DataArchiveOrigin& getArchive() const { return m_Archive; }

  bool isFromArchive(std::wstring archiveName = L"") const;

  // if originID is -1, uses the main origin; if this file doesn't exist in the
  // given origin, returns an empty string
  //
  std::wstring getFullPath(OriginID originID = InvalidOriginID) const;

  std::wstring getRelativePath() const;

  DirectoryEntry* getParent() { return m_Parent; }

  void setFileTime(FILETIME fileTime) const { m_FileTime = fileTime; }

  FILETIME getFileTime() const { return m_FileTime; }

  void setFileSize(uint64_t size, uint64_t compressedSize)
  {
    m_FileSize           = size;
    m_CompressedFileSize = compressedSize;
  }

  uint64_t getFileSize() const { return m_FileSize; }

  uint64_t getCompressedFileSize() const { return m_CompressedFileSize; }

private:
  FileIndex m_Index;
  std::wstring m_Name;
  OriginID m_Origin;
  DataArchiveOrigin m_Archive;
  AlternativesVector m_Alternatives;
  DirectoryEntry* m_Parent;
  mutable FILETIME m_FileTime;
  uint64_t m_FileSize, m_CompressedFileSize;
  mutable std::mutex m_OriginsMutex;

  bool recurseParents(std::wstring& path, const DirectoryEntry* parent) const;
};

}  // namespace MOShared

#endif  // MO_REGISTER_FILEENTRY_INCLUDED
