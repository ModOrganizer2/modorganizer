#ifndef ENV_ENV_H
#define ENV_ENV_H

class Settings;

namespace env
{

class Module;
class Process;
class SecurityProduct;
class WindowsInfo;
class Metrics;


// used by DesktopDCPtr, calls ReleaseDC(0, dc) as the deleter
//
struct DesktopDCReleaser
{
  using pointer = HDC;

  void operator()(HDC dc)
  {
    if (dc != 0) {
      ::ReleaseDC(0, dc);
    }
  }
};

using DesktopDCPtr = std::unique_ptr<HDC, DesktopDCReleaser>;


// used by LibraryPtr, calls FreeLibrary as the deleter
//
struct LibraryFreer
{
  using pointer = HINSTANCE;

  void operator()(HINSTANCE h)
  {
    if (h != 0) {
      ::FreeLibrary(h);
    }
  }
};

using LibraryPtr = std::unique_ptr<HINSTANCE, LibraryFreer>;


// used by COMPtr, calls Release() as the deleter
//
struct COMReleaser
{
  void operator()(IUnknown* p)
  {
    if (p) {
      p->Release();
    }
  }
};

template <class T>
using COMPtr = std::unique_ptr<T, COMReleaser>;


// used by MallocPtr, calls std::free() as the deleter
//
struct MallocFreer
{
  void operator()(void* p)
  {
    std::free(p);
  }
};

template <class T>
using MallocPtr = std::unique_ptr<T, MallocFreer>;


// used by LocalPtr, calls LocalFree() as the deleter
//
template <class T>
struct LocalFreer
{
  using pointer = T;

  void operator()(T p)
  {
    ::LocalFree(p);
  }
};

template <class T>
using LocalPtr = std::unique_ptr<T, LocalFreer<T>>;


// creates a console in the constructor and destroys it in the destructor,
// also redirects standard streams
//
class Console
{
public:
  // opens the console and redirects standard streams to it
  //
  Console();

  // destroys the console and redirects the standard stream to NUL
  //
  ~Console();

private:
  // whether the console was allocated successfully
  bool m_hasConsole;

  // standard streams
  FILE* m_in;
  FILE* m_out;
  FILE* m_err;
};


// represents the process's environment
//
class Environment
{
public:
  Environment();
  ~Environment();

  // list of loaded modules in the current process
  //
  const std::vector<Module>& loadedModules() const;

  // list of running processes; not cached
  //
  std::vector<Process> runningProcesses() const;

  // information about the operating system
  //
  const WindowsInfo& windowsInfo() const;

  // information about the installed security products
  //
  const std::vector<SecurityProduct>& securityProducts() const;

  // information about displays
  //
  const Metrics& metrics() const;

  // timezone
  //
  QString timezone() const;

  // logs the environment
  //
  void dump(const Settings& s) const;

private:
  mutable std::vector<Module> m_modules;
  mutable std::unique_ptr<WindowsInfo> m_windows;
  mutable std::vector<SecurityProduct> m_security;
  mutable std::unique_ptr<Metrics> m_metrics;

  // dumps all the disks involved in the settings
  //
  void dumpDisks(const Settings& s) const;
};


// environment variables
//
QString get(const QString& name);
QString set(const QString& name, const QString& value);

QString path();
QString addPath(const QString& s);
QString setPath(const QString& s);


class Service
{
public:
  enum class StartType
  {
    None = 0,
    Disabled,
    Enabled
  };

  enum class Status
  {
    None = 0,
    Stopped,
    Running
  };


  explicit Service(QString name);
  Service(QString name, StartType st, Status s);

  bool isValid() const;

  const QString& name() const;
  StartType startType() const;
  Status status() const;

  QString toString() const;

private:
  QString m_name;
  StartType m_startType;
  Status m_status;
};


Service getService(const QString& name);
QString toString(Service::StartType st);
QString toString(Service::Status st);

enum class CoreDumpTypes
{
  Mini = 1,
  Data,
  Full
};

// creates a minidump file for this process
//
bool coredump(CoreDumpTypes type);

// finds another process with the same name as this one and creates a minidump
// file for it
//
bool coredumpOther(CoreDumpTypes type);

} // namespace env

#endif // ENV_ENV_H
