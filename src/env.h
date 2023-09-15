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

// used by HMenuPtr, calls DestroyMenu() as the deleter
//
struct HMenuFreer
{
  using pointer = HMENU;

  void operator()(HMENU h)
  {
    if (h != 0) {
      ::DestroyMenu(h);
    }
  }
};

using HMenuPtr = std::unique_ptr<HMENU, HMenuFreer>;

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
  void operator()(void* p) { std::free(p); }
};

template <class T>
using MallocPtr = std::unique_ptr<T, MallocFreer>;

// used by LocalPtr, calls LocalFree() as the deleter
//
template <class T>
struct LocalFreer
{
  using pointer = T;

  void operator()(T p) { ::LocalFree(p); }
};

template <class T>
using LocalPtr = std::unique_ptr<T, LocalFreer<T>>;

// used by the CoTaskMemPtr, calls CoTaskMemFree() as the deleter
//
template <class T>
struct CoTaskMemFreer
{
  using pointer = T;

  void operator()(T p) { ::CoTaskMemFree(p); }
};

template <class T>
using CoTaskMemPtr = std::unique_ptr<T, CoTaskMemFreer<T>>;

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

class ModuleNotification
{
public:
  ModuleNotification(QObject* o, std::function<void(Module)> f);
  ~ModuleNotification();

  ModuleNotification(const ModuleNotification&)            = delete;
  ModuleNotification& operator=(const ModuleNotification&) = delete;

  ModuleNotification(ModuleNotification&&)            = default;
  ModuleNotification& operator=(ModuleNotification&&) = default;

  void setCookie(void* c);
  void fire(QString path, std::size_t fileSize);

private:
  void* m_cookie;
  QObject* m_object;
  std::set<QString> m_loaded;
  std::function<void(Module)> m_f;
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

  // will call `f` on the same thread `o` is running on every time a module
  // is loaded in the process
  //
  std::unique_ptr<ModuleNotification> onModuleLoaded(QObject* o,
                                                     std::function<void(Module)> f);

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
void set(const QString& name, const QString& value);

QString path();
QString appendToPath(const QString& s);
QString prependToPath(const QString& s);
void setPath(const QString& s);

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

struct Association
{
  // path to the executable associated with the file
  QFileInfo executable;

  // full command line associated with the file, no replacements
  QString commandLine;

  // command line _without_ the executable and with placeholders such as %1
  // replaced by the given file
  QString formattedCommandLine;
};

// returns the associated executable and command line, executable is empty on
// error
//
Association getAssociation(const QFileInfo& file);

// returns whether the given value exists
//
bool registryValueExists(const QString& key, const QString& value);

// deletes a registry key if it's empty or only contains empty keys
//
void deleteRegistryKeyIfEmpty(const QString& name);

// returns the path to this process
//
std::filesystem::path thisProcessPath();

}  // namespace env

#endif  // ENV_ENV_H
