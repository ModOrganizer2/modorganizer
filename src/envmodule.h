#ifndef ENV_MODULE_H
#define ENV_MODULE_H

#include <QDateTime>
#include <QString>

namespace env
{

// used by HandlePtr, calls CloseHandle() as the deleter
//
struct HandleCloser
{
  using pointer = HANDLE;

  void operator()(HANDLE h)
  {
    if (h != INVALID_HANDLE_VALUE) {
      ::CloseHandle(h);
    }
  }
};

using HandlePtr = std::unique_ptr<HANDLE, HandleCloser>;

// represents one module
//
class Module
{
public:
  Module(QString path, std::size_t fileSize);

  // returns the module's path
  //
  const QString& path() const;

  // returns the module's path in lowercase and using forward slashes
  //
  QString displayPath() const;

  // returns the size in bytes, may be 0
  //
  std::size_t fileSize() const;

  // returns the x.x.x.x version embedded from the version info, may be empty
  //
  const QString& version() const;

  // returns the FileVersion entry from the resource file, returns
  // "(no version)" if not available
  //
  const QString& versionString() const;

  // returns the build date from the version info, or the creation time of the
  // file on the filesystem, may be empty
  //
  const QDateTime& timestamp() const;

  // returns the md5 of the file, may be empty for system files
  //
  const QString& md5() const;

  // converts timestamp() to a string for display, returns "(no timestamp)" if
  // not available
  //
  QString timestampString() const;

  // returns false for modules in the Windows root directory
  //
  bool interesting() const;

  // returns a string with all the above information on one line
  //
  QString toString() const;

private:
  // contains the information from the version resource
  //
  struct FileInfo
  {
    VS_FIXEDFILEINFO ffi;
    QString fileDescription;
  };

  QString m_path;
  std::size_t m_fileSize;
  QString m_version;
  QDateTime m_timestamp;
  QString m_versionString;
  QString m_md5;

  // returns information from the version resource
  //
  FileInfo getFileInfo() const;

  // uses VS_FIXEDFILEINFO to build the version string
  //
  QString getVersion(const VS_FIXEDFILEINFO& fi) const;

  // uses the file date from VS_FIXEDFILEINFO if available, or gets the
  // creation date on the file
  //
  QDateTime getTimestamp(const VS_FIXEDFILEINFO& fi) const;

  // returns the md5 hash unless the path contains "\windows\"
  //
  QString getMD5() const;

  // gets VS_FIXEDFILEINFO from the file version info buffer
  //
  VS_FIXEDFILEINFO getFixedFileInfo(std::byte* buffer) const;

  // gets FileVersion from the file version info buffer
  //
  QString getFileDescription(std::byte* buffer) const;
};

// represents one process
//
class Process
{
public:
  Process();
  explicit Process(HANDLE h);
  Process(DWORD pid, DWORD ppid, QString name);

  bool isValid() const;
  DWORD pid() const;
  DWORD ppid() const;
  const QString& name() const;

  HandlePtr openHandleForWait() const;

  // whether this process can be accessed; fails if the current process doesn't
  // have the proper permissions
  //
  bool canAccess() const;

  void addChild(Process p);
  std::vector<Process>& children();
  const std::vector<Process>& children() const;

private:
  DWORD m_pid;
  mutable std::optional<DWORD> m_ppid;
  mutable std::optional<QString> m_name;
  std::vector<Process> m_children;
};

std::vector<Process> getRunningProcesses();
std::vector<Module> getLoadedModules();

// works for both jobs and processes
//
Process getProcessTree(HANDLE h);

QString getProcessName(DWORD pid);
QString getProcessName(HANDLE process);

DWORD getProcessParentID(DWORD pid);
DWORD getProcessParentID(HANDLE handle);

}  // namespace env

#endif  // ENV_MODULE_H
