#ifndef MO_REGISTER_FILEREGISTERFWD_INCLUDED
#define MO_REGISTER_FILEREGISTERFWD_INCLUDED

class DirectoryRefreshProgress;

namespace MOShared
{

struct DirectoryEntryFileKey
{
  DirectoryEntryFileKey(std::wstring v)
    : value(std::move(v)), hash(getHash(value))
  {
  }

  bool operator==(const DirectoryEntryFileKey& o) const
  {
    return (value == o.value);
  }

  static std::size_t getHash(const std::wstring& value)
  {
    return std::hash<std::wstring>()(value);
  }

  std::wstring value;
  const std::size_t hash;
};


class DirectoryEntry;
class OriginConnection;
class FileRegister;
class FilesOrigin;
class FileEntry;
struct DirectoryStats;

using FileEntryPtr = boost::shared_ptr<FileEntry>;
using FileIndex = unsigned int;
using OriginID = int;

constexpr FileIndex InvalidFileIndex = UINT_MAX;
constexpr OriginID InvalidOriginID = -1;

// if a file is in an archive, name is the name of the bsa and order
// is the order of the associated plugin in the plugins list
// is a file is not in an archive, archiveName is empty and order is usually
// -1
struct DataArchiveOrigin
{
  std::wstring name = L"";
  int order = -1;
  
  bool isValid() const {
    return name.size() > 0;
  }

  DataArchiveOrigin(std::wstring pname, int porder)
    : name(pname), order(porder) {}

  DataArchiveOrigin() {}
};

struct FileAlternative
{
  OriginID originID = -1;
  DataArchiveOrigin archive;

  bool isFromArchive() const {
    return archive.isValid();
  }
};

using AlternativesVector = std::vector<FileAlternative>;

struct DirectoryStats
{
  static constexpr bool EnableInstrumentation = false;

  std::string mod;

  std::chrono::nanoseconds dirTimes;
  std::chrono::nanoseconds fileTimes;
  std::chrono::nanoseconds sortTimes;

  std::chrono::nanoseconds subdirLookupTimes;
  std::chrono::nanoseconds addDirectoryTimes;

  std::chrono::nanoseconds filesLookupTimes;
  std::chrono::nanoseconds addFileTimes;
  std::chrono::nanoseconds addOriginToFileTimes;
  std::chrono::nanoseconds addFileToOriginTimes;
  std::chrono::nanoseconds addFileToRegisterTimes;

  int64_t originExists;
  int64_t originCreate;
  int64_t originsNeededEnabled;

  int64_t subdirExists;
  int64_t subdirCreate;

  int64_t fileExists;
  int64_t fileCreate;
  int64_t filesInsertedInRegister;
  int64_t filesAssignedInRegister;

  DirectoryStats();

  DirectoryStats& operator+=(const DirectoryStats& o);

  static std::string csvHeader();
  std::string toCsv() const;
};

} // namespace

#endif // MO_REGISTER_FILEREGISTERFWD_INCLUDED
