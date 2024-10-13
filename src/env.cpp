#include "env.h"
#include "envdump.h"
#include "envmetrics.h"
#include "envmodule.h"
#include "envsecurity.h"
#include "envshortcut.h"
#include "envwindows.h"
#include "settings.h"
#include "shared/util.h"
#include <log.h>
#include <utility.h>

namespace env
{

using namespace MOBase;

Console::Console() : m_hasConsole(false), m_in(nullptr), m_out(nullptr), m_err(nullptr)
{
  // try to attach to parent
  if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
    if (GetLastError() != ERROR_ACCESS_DENIED) {
      // parent has no console, create one
      if (!AllocConsole()) {
        // failed, ignore
        return;
      }
    }
  }

  m_hasConsole = true;

  // redirect stdin, stdout and stderr to it
  freopen_s(&m_in, "CONIN$", "r", stdin);
  freopen_s(&m_out, "CONOUT$", "w", stdout);
  freopen_s(&m_err, "CONOUT$", "w", stderr);
}

Console::~Console()
{
  // close redirected handles and redirect standard stream to NUL in case
  // they're used after this

  if (m_err) {
    std::fclose(m_err);
    freopen_s(&m_err, "NUL", "w", stderr);
  }

  if (m_out) {
    std::fclose(m_out);
    freopen_s(&m_out, "NUL", "w", stdout);
  }

  if (m_in) {
    std::fclose(m_in);
    freopen_s(&m_in, "NUL", "r", stdin);
  }

  // close console
  if (m_hasConsole) {
    FreeConsole();
  }
}

ModuleNotification::ModuleNotification(QObject* o, std::function<void(Module)> f)
    : m_cookie(nullptr), m_object(o), m_f(std::move(f))
{}

ModuleNotification::~ModuleNotification()
{
  if (!m_cookie) {
    return;
  }

  typedef NTSTATUS NTAPI LdrUnregisterDllNotificationType(PVOID Cookie);

  LibraryPtr ntdll(LoadLibraryW(L"ntdll.dll"));

  if (!ntdll) {
    log::error("failed to load ntdll.dll while unregistering for module notifications");
    return;
  }

  auto* LdrUnregisterDllNotification =
      reinterpret_cast<LdrUnregisterDllNotificationType*>(
          GetProcAddress(ntdll.get(), "LdrUnregisterDllNotification"));

  if (!LdrUnregisterDllNotification) {
    log::error("LdrUnregisterDllNotification not found in ntdll.dll");
    return;
  }

  const auto r = LdrUnregisterDllNotification(m_cookie);
  if (r != 0) {
    log::error("failed to unregister for module notifications, error {}", r);
  }
}

void ModuleNotification::setCookie(void* c)
{
  m_cookie = c;
}

void ModuleNotification::fire(QString path, std::size_t fileSize)
{
  if (m_loaded.contains(path)) {
    // don't notify if it's been loaded before
    return;
  }

  m_loaded.insert(path);

  // constructing a Module will query the version info of the file, which seems
  // to generate an access violation for at least plugin_python.dll on Windows 7
  //
  // it's not clear what the problem is, but making sure this is deferred until
  // _after_ the dll is loaded seems to fix it
  //
  // so this queues the callback in the main thread

  if (m_f) {
    QMetaObject::invokeMethod(
        m_object,
        [path, fileSize, f = m_f] {
          f(Module(path, fileSize));
        },
        Qt::QueuedConnection);
  }
}

Environment::Environment() {}

// anchor
Environment::~Environment() = default;

const std::vector<Module>& Environment::loadedModules() const
{
  if (m_modules.empty()) {
    m_modules = getLoadedModules();
  }

  return m_modules;
}

std::vector<Process> Environment::runningProcesses() const
{
  return getRunningProcesses();
}

const WindowsInfo& Environment::windowsInfo() const
{
  if (!m_windows) {
    m_windows.reset(new WindowsInfo);
  }

  return *m_windows;
}

const std::vector<SecurityProduct>& Environment::securityProducts() const
{
  if (m_security.empty()) {
    m_security = getSecurityProducts();
  }

  return m_security;
}

const Metrics& Environment::metrics() const
{
  if (!m_metrics) {
    m_metrics.reset(new Metrics);
  }

  return *m_metrics;
}

QString Environment::timezone() const
{
  TIME_ZONE_INFORMATION tz = {};

  const auto r = GetTimeZoneInformation(&tz);
  if (r == TIME_ZONE_ID_INVALID) {
    const auto e = GetLastError();
    log::error("failed to get timezone, {}", formatSystemMessage(e));
    return "unknown";
  }

  auto offsetString = [](int o) {
    return QString("%1%2:%3")
        .arg(o < 0 ? "" : "+")
        .arg(QString::number(o / 60), 2, QChar::fromLatin1('0'))
        .arg(QString::number(o % 60), 2, QChar::fromLatin1('0'));
  };

  const auto stdName   = QString::fromWCharArray(tz.StandardName);
  const auto stdOffset = -(tz.Bias + tz.StandardBias);
  const auto std       = QString("%1, %2").arg(stdName).arg(offsetString(stdOffset));

  const auto dstName   = QString::fromWCharArray(tz.DaylightName);
  const auto dstOffset = -(tz.Bias + tz.DaylightBias);
  const auto dst       = QString("%1, %2").arg(dstName).arg(offsetString(dstOffset));

  QString s;

  if (r == TIME_ZONE_ID_DAYLIGHT) {
    s = dst + " (dst is active, std is " + std + ")";
  } else {
    s = std + " (std is active, dst is " + dst + ")";
  }

  return s;
}

std::unique_ptr<ModuleNotification>
Environment::onModuleLoaded(QObject* o, std::function<void(Module)> f)
{
  typedef struct _UNICODE_STRING
  {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
  } UNICODE_STRING, *PUNICODE_STRING;

  typedef const PUNICODE_STRING PCUNICODE_STRING;

  typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA
  {
    ULONG Flags;                   // Reserved.
    PCUNICODE_STRING FullDllName;  // The full path name of the DLL module.
    PCUNICODE_STRING BaseDllName;  // The base file name of the DLL module.
    PVOID DllBase;      // A pointer to the base address for the DLL in memory.
    ULONG SizeOfImage;  // The size of the DLL image, in bytes.
  } LDR_DLL_LOADED_NOTIFICATION_DATA, *PLDR_DLL_LOADED_NOTIFICATION_DATA;

  typedef struct _LDR_DLL_UNLOADED_NOTIFICATION_DATA
  {
    ULONG Flags;                   // Reserved.
    PCUNICODE_STRING FullDllName;  // The full path name of the DLL module.
    PCUNICODE_STRING BaseDllName;  // The base file name of the DLL module.
    PVOID DllBase;      // A pointer to the base address for the DLL in memory.
    ULONG SizeOfImage;  // The size of the DLL image, in bytes.
  } LDR_DLL_UNLOADED_NOTIFICATION_DATA, *PLDR_DLL_UNLOADED_NOTIFICATION_DATA;

  typedef union _LDR_DLL_NOTIFICATION_DATA
  {
    LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
    LDR_DLL_UNLOADED_NOTIFICATION_DATA Unloaded;
  } LDR_DLL_NOTIFICATION_DATA, *PLDR_DLL_NOTIFICATION_DATA;

  typedef VOID CALLBACK LDR_DLL_NOTIFICATION_FUNCTION(
      ULONG NotificationReason, const PLDR_DLL_NOTIFICATION_DATA NotificationData,
      PVOID Context);

  typedef LDR_DLL_NOTIFICATION_FUNCTION* PLDR_DLL_NOTIFICATION_FUNCTION;

  typedef NTSTATUS NTAPI LdrRegisterDllNotificationType(
      ULONG Flags, PLDR_DLL_NOTIFICATION_FUNCTION NotificationFunction, PVOID Context,
      PVOID * Cookie);

  const ULONG LDR_DLL_NOTIFICATION_REASON_LOADED   = 1;
  const ULONG LDR_DLL_NOTIFICATION_REASON_UNLOADED = 2;

  // loading ntdll.dll, the function will be found with GetProcAddress()
  LibraryPtr ntdll(LoadLibraryW(L"ntdll.dll"));

  if (!ntdll) {
    log::error("failed to load ntdll.dll while registering for module notifications");
    return {};
  }

  auto* LdrRegisterDllNotification = reinterpret_cast<LdrRegisterDllNotificationType*>(
      GetProcAddress(ntdll.get(), "LdrRegisterDllNotification"));

  if (!LdrRegisterDllNotification) {
    log::error("LdrRegisterDllNotification not found in ntdll.dll");
    return {};
  }

  auto context = std::make_unique<ModuleNotification>(o, f);
  void* cookie = nullptr;

  auto OnDllLoaded = [](ULONG reason, const PLDR_DLL_NOTIFICATION_DATA data,
                        void* context) {
    if (reason == LDR_DLL_NOTIFICATION_REASON_LOADED) {
      if (data && data->Loaded.FullDllName) {
        if (context) {
          static_cast<ModuleNotification*>(context)->fire(
              QString::fromWCharArray(data->Loaded.FullDllName->Buffer,
                                      data->Loaded.FullDllName->Length /
                                          sizeof(wchar_t)),
              data->Loaded.SizeOfImage);
        }
      }
    }
  };

  const auto r = LdrRegisterDllNotification(0, OnDllLoaded, context.get(), &cookie);

  if (r != 0) {
    log::error("failed to register for module notifications, error {}", r);
    return {};
  }

  context->setCookie(cookie);

  return context;
}

void Environment::dump(const Settings& s) const
{
  log::debug("windows: {}", windowsInfo().toString());

  log::debug("time zone: {}", timezone());

  if (windowsInfo().compatibilityMode()) {
    log::warn("MO seems to be running in compatibility mode");
  }

  log::debug("security products:");

  {
    // ignore products with identical names, some AVs register themselves with
    // the same names and provider, but different guids
    std::set<QString> productNames;
    for (const auto& sp : securityProducts()) {
      productNames.insert(sp.toString());
    }

    for (auto&& name : productNames) {
      log::debug("  . {}", name);
    }
  }

  log::debug("modules loaded in process:");
  for (const auto& m : loadedModules()) {
    if (m.interesting()) {
      log::debug(" . {}", m.toString());
    }
  }

  log::debug("displays:");
  for (const auto& d : metrics().displays()) {
    log::debug(" . {}", d.toString());
  }

  const auto r = metrics().desktopGeometry();
  log::debug("desktop geometry: ({},{})-({},{})", r.left(), r.top(), r.right(),
             r.bottom());

  dumpDisks(s);
}

void Environment::dumpDisks(const Settings& s) const
{
  std::set<QString> rootPaths;

  auto dump = [&](auto&& path) {
    const QFileInfo fi(path);
    const QStorageInfo si(fi.absoluteFilePath());

    if (rootPaths.contains(si.rootPath())) {
      // already seen
      return;
    }

    // remember
    rootPaths.insert(si.rootPath());

    log::debug("  . {} free={} MB{}", si.rootPath(), (si.bytesFree() / 1000 / 1000),
               (si.isReadOnly() ? " (readonly)" : ""));
  };

  log::debug("drives:");

  dump(QStorageInfo::root().rootPath());
  dump(s.paths().base());
  dump(s.paths().downloads());
  dump(s.paths().mods());
  dump(s.paths().cache());
  dump(s.paths().profiles());
  dump(s.paths().overwrite());
  dump(QCoreApplication::applicationDirPath());
}

QString path()
{
  return get("PATH");
}

QString appendToPath(const QString& s)
{
  auto old = path();
  set("PATH", old + ";" + s);
  return old;
}

QString prependToPath(const QString& s)
{
  auto old = path();
  set("PATH", s + ";" + old);
  return old;
}

void setPath(const QString& s)
{
  set("PATH", s);
}

QString get(const QString& name)
{
  std::size_t bufferSize = 4000;
  auto buffer            = std::make_unique<wchar_t[]>(bufferSize);

  DWORD realSize = ::GetEnvironmentVariableW(name.toStdWString().c_str(), buffer.get(),
                                             static_cast<DWORD>(bufferSize));

  if (realSize > bufferSize) {
    bufferSize = realSize;
    buffer     = std::make_unique<wchar_t[]>(bufferSize);

    realSize = ::GetEnvironmentVariableW(name.toStdWString().c_str(), buffer.get(),
                                         static_cast<DWORD>(bufferSize));
  }

  if (realSize == 0) {
    const auto e = ::GetLastError();

    // don't log if not found
    if (e != ERROR_ENVVAR_NOT_FOUND) {
      log::error("failed to get environment variable '{}', {}", name,
                 formatSystemMessage(e));
    }

    return {};
  }

  return QString::fromWCharArray(buffer.get(), realSize);
}

void set(const QString& n, const QString& v)
{
  ::SetEnvironmentVariableW(n.toStdWString().c_str(), v.toStdWString().c_str());
}

Service::Service(QString name) : Service(std::move(name), StartType::None, Status::None)
{}

Service::Service(QString name, StartType st, Status s)
    : m_name(std::move(name)), m_startType(st), m_status(s)
{}

const QString& Service::name() const
{
  return m_name;
}

bool Service::isValid() const
{
  return (m_startType != StartType::None) && (m_status != Status::None);
}

Service::StartType Service::startType() const
{
  return m_startType;
}

Service::Status Service::status() const
{
  return m_status;
}

QString Service::toString() const
{
  return QString("service '%1', start=%2, status=%3")
      .arg(m_name)
      .arg(env::toString(m_startType))
      .arg(env::toString(m_status));
}

QString toString(Service::StartType st)
{
  using ST = Service::StartType;

  switch (st) {
  case ST::None:
    return "none";

  case ST::Disabled:
    return "disabled";

  case ST::Enabled:
    return "enabled";

  default:
    return QString("unknown %1").arg(static_cast<int>(st));
  }
}

QString toString(Service::Status st)
{
  using S = Service::Status;

  switch (st) {
  case S::None:
    return "none";

  case S::Stopped:
    return "stopped";

  case S::Running:
    return "running";

  default:
    return QString("unknown %1").arg(static_cast<int>(st));
  }
}

Service::StartType getServiceStartType(SC_HANDLE s, const QString& name)
{
  DWORD needed = 0;

  if (!QueryServiceConfig(s, NULL, 0, &needed)) {
    const auto e = GetLastError();

    if (e != ERROR_INSUFFICIENT_BUFFER) {
      log::error("QueryServiceConfig() for size for '{}' failed, {}", name,
                 GetLastError());

      return Service::StartType::None;
    }
  }

  const auto size = needed;
  MallocPtr<QUERY_SERVICE_CONFIG> config(
      static_cast<QUERY_SERVICE_CONFIG*>(std::malloc(size)));

  if (!QueryServiceConfig(s, config.get(), size, &needed)) {
    const auto e = GetLastError();

    log::error("QueryServiceConfig() for '{}' failed", name, formatSystemMessage(e));

    return Service::StartType::None;
  }

  switch (config->dwStartType) {
  case SERVICE_AUTO_START:  // fall-through
  case SERVICE_BOOT_START:
  case SERVICE_DEMAND_START:
  case SERVICE_SYSTEM_START: {
    return Service::StartType::Enabled;
  }

  case SERVICE_DISABLED: {
    return Service::StartType::Disabled;
  }

  default: {
    log::error("unknown service start type {} for '{}'", config->dwStartType, name);

    return Service::StartType::None;
  }
  }
}

Service::Status getServiceStatus(SC_HANDLE s, const QString& name)
{
  DWORD needed = 0;

  if (!QueryServiceStatusEx(s, SC_STATUS_PROCESS_INFO, NULL, 0, &needed)) {
    const auto e = GetLastError();

    if (e != ERROR_INSUFFICIENT_BUFFER) {
      log::error("QueryServiceStatusEx() for size for '{}' failed, {}", name,
                 GetLastError());

      return Service::Status::None;
    }
  }

  const auto size = needed;
  MallocPtr<SERVICE_STATUS_PROCESS> status(
      static_cast<SERVICE_STATUS_PROCESS*>(std::malloc(size)));

  const auto r = QueryServiceStatusEx(
      s, SC_STATUS_PROCESS_INFO, reinterpret_cast<BYTE*>(status.get()), size, &needed);

  if (!r) {
    const auto e = GetLastError();

    log::error("QueryServiceStatusEx() failed for '{}', {}", name,
               formatSystemMessage(e));

    return Service::Status::None;
  }

  switch (status->dwCurrentState) {
  case SERVICE_START_PENDING:  // fall-through
  case SERVICE_CONTINUE_PENDING:
  case SERVICE_RUNNING: {
    return Service::Status::Running;
  }

  case SERVICE_STOPPED:  // fall-through
  case SERVICE_STOP_PENDING:
  case SERVICE_PAUSE_PENDING:
  case SERVICE_PAUSED: {
    return Service::Status::Stopped;
  }

  default: {
    log::error("unknown service status {} for '{}'", status->dwCurrentState, name);

    return Service::Status::None;
  }
  }
}

Service getService(const QString& name)
{
  // service manager
  const LocalPtr<SC_HANDLE> scm(
      OpenSCManager(NULL, NULL, SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG));

  if (!scm) {
    const auto e = GetLastError();
    log::error("OpenSCManager() failed, {}", formatSystemMessage(e));
    return Service(name);
  }

  // service
  const LocalPtr<SC_HANDLE> s(OpenService(scm.get(), name.toStdWString().c_str(),
                                          SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG));

  if (!s) {
    const auto e = GetLastError();
    log::error("OpenService() failed for '{}', {}", name, formatSystemMessage(e));
    return Service(name);
  }

  const auto startType = getServiceStartType(s.get(), name);
  const auto status    = getServiceStatus(s.get(), name);

  return {name, startType, status};
}

std::optional<QString> getAssocString(const QFileInfo& file, ASSOCSTR astr)
{
  const auto ext = L"." + file.suffix().toStdWString();

  // getting buffer size
  DWORD bufferSize = 0;
  auto r = AssocQueryStringW(ASSOCF_INIT_IGNOREUNKNOWN, astr, ext.c_str(), L"open",
                             nullptr, &bufferSize);

  // returns S_FALSE when giving back the buffer size, so that's actually the
  // expected return value

  if (r != S_FALSE || bufferSize == 0) {
    if (r == HRESULT_FROM_WIN32(ERROR_NO_ASSOCIATION)) {
      log::error("file '{}' has no associated executable", file.absoluteFilePath());
    } else {
      log::error("can't get buffer size for AssocQueryStringW(), {}",
                 formatSystemMessage(r));
    }
    return {};
  }

  // getting string
  auto buffer = std::make_unique<wchar_t[]>(bufferSize + 1);
  std::fill(buffer.get(), buffer.get() + bufferSize + 1, 0);

  r = AssocQueryStringW(ASSOCF_INIT_IGNOREUNKNOWN, astr, ext.c_str(), L"open",
                        buffer.get(), &bufferSize);

  if (FAILED(r)) {
    log::error("failed to get exe associated with '{}', {}", file.suffix(),
               formatSystemMessage(r));

    return {};
  }

  // buffer size includes the null terminator
  return QString::fromWCharArray(buffer.get(), bufferSize - 1);
}

QString formatCommandLine(const QFileInfo& targetInfo, const QString& cmd)
{
  // yeah, FormatMessage() expects at least as many arguments as there are
  // placeholders and while the command for associations should typically only
  // have %1, the user can actually enter anything in the registry
  //
  // since the maximum number of arguments is 99, this creates an array of 99
  // wchar_* where the first one (%1) points to the filename and the remaining
  // 98 to ""
  //
  // FormatMessage() actually takes a va_list* for the arguments, but by passing
  // FORMAT_MESSAGE_ARGUMENT_ARRAY, an array of DWORD_PTR can be given instead

  // 99 arguments
  std::array<DWORD_PTR, 99> args;

  // first one is the filename
  const auto wpath = targetInfo.absoluteFilePath().toStdWString();
  args[0]          = reinterpret_cast<DWORD_PTR>(wpath.c_str());

  // remaining are ""
  std::fill(args.begin() + 1, args.end(), reinterpret_cast<DWORD_PTR>(L""));

  // must be freed with LocalFree()
  wchar_t* buffer = nullptr;

  const auto wcmd = cmd.toStdWString();

  const auto n =
      ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_ARGUMENT_ARRAY |
                           FORMAT_MESSAGE_FROM_STRING,
                       wcmd.c_str(), 0, 0, reinterpret_cast<LPWSTR>(&buffer), 0,
                       reinterpret_cast<va_list*>(&args[0]));

  if (n == 0 || !buffer) {
    const auto e = GetLastError();

    log::error("failed to format command line '{}' with path '{}', {}", cmd,
               targetInfo.absoluteFilePath(), formatSystemMessage(e));

    return {};
  }

  auto s = QString::fromWCharArray(buffer, n);
  ::LocalFree(buffer);

  return s.trimmed();
}

std::pair<QString, QString> splitExeAndArguments(const QString& cmd)
{
  int exeBegin = 0;
  int exeEnd   = -1;

  if (cmd[0] == '"') {
    // surrounded by double-quotes, so find the next one
    exeBegin = 1;
    exeEnd   = cmd.indexOf('"', exeBegin);

    if (exeEnd == -1) {
      log::error("missing terminating double-quote in command line '{}'", cmd);
      return {};
    }
  } else {
    // no double-quotes, find the first whitespace
    exeEnd = cmd.indexOf(QRegularExpression("\\s"));
    if (exeEnd == -1) {
      exeEnd = cmd.size();
    }
  }

  QString exe  = cmd.mid(exeBegin, exeEnd - exeBegin).trimmed();
  QString args = cmd.mid(exeEnd + 1).trimmed();

  return {std::move(exe), std::move(args)};
}

Association getAssociation(const QFileInfo& targetInfo)
{
  log::debug("getting association for '{}', extension is '.{}'",
             targetInfo.absoluteFilePath(), targetInfo.suffix());

  const auto cmd = getAssocString(targetInfo, ASSOCSTR_COMMAND);
  if (!cmd) {
    return {};
  }

  log::debug("raw cmd is '{}'", *cmd);

  QString formattedCmd = formatCommandLine(targetInfo, *cmd);
  if (formattedCmd.isEmpty()) {
    log::error("command line associated with '{}' is empty",
               targetInfo.absoluteFilePath());

    return {};
  }

  log::debug("formatted cmd is '{}'", formattedCmd);

  const auto p = splitExeAndArguments(formattedCmd);
  if (p.first.isEmpty()) {
    return {};
  }

  log::debug("split into exe='{}' and cmd='{}'", p.first, p.second);

  return {QFileInfo(p.first), *cmd, p.second};
}

struct RegistryKeyCloser
{
  using pointer = HKEY;

  void operator()(HKEY key)
  {
    if (key != 0) {
      ::RegCloseKey(key);
    }
  }
};

using RegistryKeyPtr = std::unique_ptr<HKEY, RegistryKeyCloser>;

RegistryKeyPtr openRegistryKey(HKEY parent, const wchar_t* name)
{
  HKEY subkey = 0;

  auto r = ::RegOpenKeyExW(parent, name, 0,
                           KEY_SET_VALUE | KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE,
                           &subkey);

  if (r != ERROR_SUCCESS) {
    return {};
  }

  return RegistryKeyPtr(subkey);
}

bool keyHasValues(HKEY key)
{
  auto name      = std::make_unique<wchar_t[]>(1000 + 1);
  DWORD nameSize = 1000;

  // note that RegEnumValueW() also enumerates the default value if it exists
  auto r = ::RegEnumValueW(key, 0, name.get(), &nameSize, nullptr, nullptr, nullptr,
                           nullptr);

  if (r != ERROR_NO_MORE_ITEMS) {
    return true;
  }

  // no values, no default, it's empty
  return false;
}

bool forEachSubKey(HKEY key, std::function<bool(const wchar_t* name)> f)
{
  auto name      = std::make_unique<wchar_t[]>(1000 + 1);
  DWORD nameSize = 1000;

  DWORD i = 0;

  // something would be really wrong if it had more than 100 keys
  while (i < 100) {
    auto r = ::RegEnumKeyExW(key, i, name.get(), &nameSize, nullptr, nullptr, nullptr,
                             nullptr);

    if (r == ERROR_NO_MORE_ITEMS) {
      // no more subkeys
      break;
    }

    if (r == ERROR_SUCCESS) {
      // a subkey exists
      auto subkey = openRegistryKey(key, name.get());

      if (!subkey) {
        // can't open it, stop
        return false;
      }

      // fire callback
      if (!f(name.get())) {
        return false;
      }
    } else {
      // something went wrong, stop
      return false;
    }

    ++i;
  }

  return true;
}

bool isKeyEmpty(HKEY key)
{
  // check for any values
  if (keyHasValues(key)) {
    return false;
  }

  auto r = forEachSubKey(key, [&](const wchar_t* name) {
    // a subkey exists, recursively check if it's empty
    auto subkey = openRegistryKey(key, name);

    if (!subkey) {
      // can't open, stop
      return false;
    }

    if (!isKeyEmpty(subkey.get())) {
      // not empty, stop
      return false;
    }

    // empty, go on
    return true;
  });

  if (!r) {
    // something went wrong or some subkey has values
    return false;
  }

  // key has no values and has either no subkeys or all subkeys are empty
  return true;
}

void deleteRegistryKeyIfEmpty(const QString& name)
{
  if (name.isEmpty()) {
    return;
  }

  auto key = openRegistryKey(HKEY_CURRENT_USER, name.toStdWString().c_str());
  if (!key) {
    return;
  }

  if (!isKeyEmpty(key.get())) {
    return;
  }

  ::RegDeleteTreeW(HKEY_CURRENT_USER, name.toStdWString().c_str());
}

bool registryValueExists(const QString& keyName, const QString& valueName)
{
  auto key = openRegistryKey(HKEY_CURRENT_USER, keyName.toStdWString().c_str());

  if (!key) {
    return false;
  }

  DWORD type = 0;

  auto r = ::RegQueryValueExW(key.get(), valueName.toStdWString().c_str(), nullptr,
                              &type, nullptr, nullptr);

  return (r == ERROR_SUCCESS);
}

// returns the filename of the given process or the current one
//
std::filesystem::path processPath(HANDLE process = INVALID_HANDLE_VALUE)
{
  // double the buffer size 10 times
  const int MaxTries = 10;

  DWORD bufferSize = MAX_PATH;

  for (int tries = 0; tries < MaxTries; ++tries) {
    auto buffer = std::make_unique<wchar_t[]>(bufferSize + 1);
    std::fill(buffer.get(), buffer.get() + bufferSize + 1, 0);

    DWORD writtenSize = 0;

    if (process == INVALID_HANDLE_VALUE) {
      // query this process
      writtenSize = GetModuleFileNameW(0, buffer.get(), bufferSize);
    } else {
      // query another process
      writtenSize = GetModuleBaseNameW(process, 0, buffer.get(), bufferSize);
    }

    if (writtenSize == 0) {
      // hard failure
      const auto e = GetLastError();
      std::wcerr << formatSystemMessage(e) << L"\n";
      break;
    } else if (writtenSize >= bufferSize) {
      // buffer is too small, try again
      bufferSize *= 2;
    } else {
      // if GetModuleFileName() works, `writtenSize` does not include the null
      // terminator
      const std::wstring s(buffer.get(), writtenSize);
      const std::filesystem::path path(s);

      return path;
    }
  }

  // something failed or the path is way too long to make sense

  std::wstring what;
  if (process == INVALID_HANDLE_VALUE) {
    what = L"the current process";
  } else {
    what = L"pid " + std::to_wstring(reinterpret_cast<std::uintptr_t>(process));
  }

  std::wcerr << L"failed to get filename for " << what << L"\n";
  return {};
}

std::wstring processFilename(HANDLE process = INVALID_HANDLE_VALUE)
{
  const auto p = processPath(process);
  if (p.empty()) {
    return {};
  }

  return p.filename().native();
}

std::filesystem::path thisProcessPath()
{
  return processPath();
}

DWORD findOtherPid()
{
  const std::wstring defaultName = L"ModOrganizer.exe";

  std::wclog << L"looking for the other process...\n";

  // used to skip the current process below
  const auto thisPid = GetCurrentProcessId();
  std::wclog << L"this process id is " << thisPid << L"\n";

  // getting the filename for this process, assumes the other process has the
  // same one
  auto filename = processFilename();
  if (filename.empty()) {
    std::wcerr << L"can't get current process filename, defaulting to " << defaultName
               << L"\n";

    filename = defaultName;
  } else {
    std::wclog << L"this process filename is " << filename << L"\n";
  }

  // getting all running processes
  const auto processes = getRunningProcesses();
  std::wclog << L"there are " << processes.size() << L" processes running\n";

  // going through processes, trying to find one with the same name and a
  // different pid than this process has
  for (const auto& p : processes) {
    if (p.name().toStdWString() == filename) {
      if (p.pid() != thisPid) {
        return p.pid();
      }
    }
  }

  std::wclog << L"no process with this filename\n"
             << L"MO may not be running, or it may be running as administrator\n"
             << L"you can try running this again as administrator\n";

  return 0;
}

std::wstring tempDir()
{
  const DWORD bufferSize         = MAX_PATH + 1;
  wchar_t buffer[bufferSize + 1] = {};

  const auto written = GetTempPathW(bufferSize, buffer);
  if (written == 0) {
    const auto e = GetLastError();

    std::wcerr << L"failed to get temp path, " << formatSystemMessage(e) << L"\n";

    return {};
  }

  // `written` does not include the null terminator
  return std::wstring(buffer, buffer + written);
}

std::wstring safeVersion()
{
  try {
    // this can throw
    return MOShared::createVersionInfo().string().toStdWString() + L"-";
  } catch (...) {
    return {};
  }
}

HandlePtr tempFile(const std::wstring dir)
{
  // maximum tries of incrementing the counter
  const int MaxTries = 100;

  // UTC time and date will be in the filename
  const auto now = std::time(0);
  const auto tm  = std::gmtime(&now);

  // "ModOrganizer-YYYYMMDDThhmmss.dmp", with a possible "-i" appended, where
  // i can go until MaxTries
  std::wostringstream oss;
  oss << L"ModOrganizer-" << safeVersion() << std::setw(4) << (1900 + tm->tm_year)
      << std::setw(2) << std::setfill(L'0') << (tm->tm_mon + 1) << std::setw(2)
      << std::setfill(L'0') << tm->tm_mday << "T" << std::setw(2) << std::setfill(L'0')
      << tm->tm_hour << std::setw(2) << std::setfill(L'0') << tm->tm_min << std::setw(2)
      << std::setfill(L'0') << tm->tm_sec;

  const std::wstring prefix = oss.str();
  const std::wstring ext    = L".dmp";

  // first path to try, without counter in it
  std::wstring path = dir + L"\\" + prefix + ext;

  for (int i = 0; i < MaxTries; ++i) {
    std::wclog << L"trying file '" << path << L"'\n";

    HandlePtr h(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                            FILE_ATTRIBUTE_NORMAL, nullptr));

    if (h.get() != INVALID_HANDLE_VALUE) {
      // worked
      return h;
    }

    const auto e = GetLastError();

    if (e != ERROR_FILE_EXISTS) {
      // probably no write access
      std::wcerr << L"failed to create dump file, " << formatSystemMessage(e) << L"\n";

      return {};
    }

    // try again with "-i"
    path = dir + L"\\" + prefix + L"-" + std::to_wstring(i + 1) + ext;
  }

  std::wcerr << L"can't create dump file, ran out of filenames\n";
  return {};
}

HandlePtr dumpFile(const wchar_t* dir)
{
  // try the given directory, if any
  if (dir) {
    HandlePtr h = tempFile(dir);
    if (h.get() != INVALID_HANDLE_VALUE) {
      return h;
    }
  }

  // try the current directory
  HandlePtr h = tempFile(L".");
  if (h.get() != INVALID_HANDLE_VALUE) {
    return h;
  }

  std::wclog << L"cannot write dump file in current directory\n";

  // try the temp directory
  const auto temp = tempDir();

  if (!temp.empty()) {
    h = tempFile(temp.c_str());
    if (h.get() != INVALID_HANDLE_VALUE) {
      return h;
    }
  }

  return {};
}

CoreDumpTypes coreDumpTypeFromString(const std::string& s)
{
  if (s == "data")
    return env::CoreDumpTypes::Data;
  else if (s == "full")
    return env::CoreDumpTypes::Full;
  else
    return env::CoreDumpTypes::Mini;
}

std::string toString(CoreDumpTypes type)
{
  switch (type) {
  case CoreDumpTypes::Mini:
    return "mini";

  case CoreDumpTypes::Data:
    return "data";

  case CoreDumpTypes::Full:
    return "full";

  default:
    return "?";
  }
}

bool createMiniDump(const wchar_t* dir, HANDLE process, CoreDumpTypes type)
{
  const DWORD pid = GetProcessId(process);

  const HandlePtr file = dumpFile(dir);
  if (!file) {
    std::wcerr << L"nowhere to write the dump file\n";
    return false;
  }

  auto flags =
      _MINIDUMP_TYPE(MiniDumpNormal | MiniDumpWithHandleData |
                     MiniDumpWithUnloadedModules | MiniDumpWithProcessThreadData);

  if (type == CoreDumpTypes::Data) {
    std::wclog << L"writing minidump with data\n";
    flags = _MINIDUMP_TYPE(flags | MiniDumpWithDataSegs);
  } else if (type == CoreDumpTypes::Full) {
    std::wclog << L"writing full minidump\n";
    flags = _MINIDUMP_TYPE(flags | MiniDumpWithFullMemory);
  } else {
    std::wclog << L"writing mini minidump\n";
  }

  const auto ret =
      MiniDumpWriteDump(process, pid, file.get(), flags, nullptr, nullptr, nullptr);

  if (!ret) {
    const auto e = GetLastError();

    std::wcerr << L"failed to write mini dump, " << formatSystemMessage(e) << L"\n";

    return false;
  }

  std::wclog << L"minidump written correctly\n";
  return true;
}

bool coredump(const wchar_t* dir, CoreDumpTypes type)
{
  std::wclog << L"creating minidump for the current process\n";
  return createMiniDump(dir, GetCurrentProcess(), type);
}

bool coredumpOther(CoreDumpTypes type)
{
  std::wclog << L"creating minidump for a running process\n";

  const auto pid = findOtherPid();
  if (pid == 0) {
    std::wcerr << L"no other process found\n";
    return false;
  }

  std::wclog << L"found other process with pid " << pid << L"\n";

  HandlePtr handle(
      OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid));

  if (!handle) {
    const auto e = GetLastError();

    std::wcerr << L"failed to open process " << pid << L", " << formatSystemMessage(e)
               << L"\n";

    return false;
  }

  return createMiniDump(nullptr, handle.get(), type);
}

}  // namespace env
