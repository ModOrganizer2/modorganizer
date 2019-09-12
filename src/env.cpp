#include "env.h"
#include "envmetrics.h"
#include "envmodule.h"
#include "envsecurity.h"
#include "envshortcut.h"
#include "envwindows.h"
#include "settings.h"
#include <log.h>
#include <utility.h>

namespace env
{

using namespace MOBase;

Console::Console()
  : m_hasConsole(false), m_in(nullptr), m_out(nullptr), m_err(nullptr)
{
  // open a console
  if (!AllocConsole()) {
    // failed, ignore
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


Environment::Environment()
{
}

// anchor
Environment::~Environment() = default;

const std::vector<Module>& Environment::loadedModules() const
{
  if (m_modules.empty()){
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

void Environment::dump(const Settings& s) const
{
  log::debug("windows: {}", windowsInfo().toString());

  if (windowsInfo().compatibilityMode()) {
    log::warn("MO seems to be running in compatibility mode");
  }

  log::debug("security products:");
  for (const auto& sp : securityProducts()) {
    log::debug("  . {}", sp.toString());
  }

  log::debug("modules loaded in process:");
  for (const auto& m : loadedModules()) {
    log::debug(" . {}", m.toString());
  }

  log::debug("displays:");
  for (const auto& d : metrics().displays()) {
    log::debug(" . {}", d.toString());
  }

  const auto r = metrics().desktopGeometry();
  log::debug(
    "desktop geometry: ({},{})-({},{})",
    r.left(), r.top(), r.right(), r.bottom());

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

    log::debug(
      "  . {} free={} MB{}",
      si.rootPath(),
      (si.bytesFree() / 1000 / 1000),
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


// returns the filename of the given process or the current one
//
std::wstring processFilename(HANDLE process=INVALID_HANDLE_VALUE)
{
  // double the buffer size 10 times
  const int MaxTries = 10;

  DWORD bufferSize = MAX_PATH;

  for (int tries=0; tries<MaxTries; ++tries)
  {
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

      return path.filename().native();
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
    std::wcerr
      << L"can't get current process filename, defaulting to "
      << defaultName << L"\n";

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
    if (p.name() == filename) {
      if (p.pid() != thisPid) {
        return p.pid();
      }
    }
  }

  std::wclog
    << L"no process with this filename\n"
    << L"MO may not be running, or it may be running as administrator\n"
    << L"you can try running this again as administrator\n";

  return 0;
}

std::wstring tempDir()
{
  const DWORD bufferSize = MAX_PATH + 1;
  wchar_t buffer[bufferSize + 1] = {};

  const auto written = GetTempPathW(bufferSize, buffer);
  if (written == 0) {
    const auto e = GetLastError();

    std::wcerr
      << L"failed to get temp path, " << formatSystemMessage(e) << L"\n";

    return {};
  }

  // `written` does not include the null terminator
  return std::wstring(buffer, buffer + written);
}

HandlePtr tempFile(const std::wstring dir)
{
  // maximum tries of incrementing the counter
  const int MaxTries = 100;

  // UTC time and date will be in the filename
  const auto now = std::time(0);
  const auto tm = std::gmtime(&now);

  // "ModOrganizer-YYYYMMDDThhmmss.dmp", with a possible "-i" appended, where
  // i can go until MaxTries
  std::wostringstream oss;
  oss
    << L"ModOrganizer-"
    << std::setw(4) << (1900 + tm->tm_year)
    << std::setw(2) << std::setfill(L'0') << (tm->tm_mon + 1)
    << std::setw(2) << std::setfill(L'0') << tm->tm_mday << "T"
    << std::setw(2) << std::setfill(L'0') << tm->tm_hour
    << std::setw(2) << std::setfill(L'0') << tm->tm_min
    << std::setw(2) << std::setfill(L'0') << tm->tm_sec;

  const std::wstring prefix = oss.str();
  const std::wstring ext = L".dmp";

  // first path to try, without counter in it
  std::wstring path = dir + L"\\" + prefix + ext;

  for (int i=0; i<MaxTries; ++i) {
    std::wclog << L"trying file '" << path << L"'\n";

    HandlePtr h (CreateFileW(
      path.c_str(), GENERIC_WRITE, 0, nullptr,
      CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));

    if (h.get() != INVALID_HANDLE_VALUE) {
      // worked
      return h;
    }

    const auto e = GetLastError();

    if (e != ERROR_FILE_EXISTS) {
      // probably no write access
      std::wcerr
        << L"failed to create dump file, " << formatSystemMessage(e) << L"\n";

      return {};
    }

    // try again with "-i"
    path = dir + L"\\" + prefix + L"-" + std::to_wstring(i + 1) + ext;
  }

  std::wcerr << L"can't create dump file, ran out of filenames\n";
  return {};
}

HandlePtr dumpFile()
{
  // try the current directory
  HandlePtr h = tempFile(L".");
  if (h.get() != INVALID_HANDLE_VALUE) {
    return h;
  }

  std::wclog << L"cannot write dump file in current directory\n";

  // try the temp directory
  const auto dir = tempDir();

  if (!dir.empty()) {
    h = tempFile(dir.c_str());
    if (h.get() != INVALID_HANDLE_VALUE) {
      return h;
    }
  }

  return {};
}

bool createMiniDump(HANDLE process, CoreDumpTypes type)
{
  const DWORD pid = GetProcessId(process);

  const HandlePtr file = dumpFile();
  if (!file) {
    std::wcerr << L"nowhere to write the dump file\n";
    return false;
  }

  auto flags = _MINIDUMP_TYPE(
    MiniDumpNormal |
    MiniDumpWithHandleData |
    MiniDumpWithUnloadedModules |
    MiniDumpWithProcessThreadData);

  if (type == CoreDumpTypes::Data) {
    std::wclog << L"writing minidump with data\n";
    flags = _MINIDUMP_TYPE(flags | MiniDumpWithDataSegs);
  } else if (type ==  CoreDumpTypes::Full) {
    std::wclog << L"writing full minidump\n";
    flags = _MINIDUMP_TYPE(flags | MiniDumpWithFullMemory);
  } else {
    std::wclog << L"writing mini minidump\n";
  }

  const auto ret = MiniDumpWriteDump(
    process, pid, file.get(), flags, nullptr, nullptr, nullptr);

  if (!ret) {
    const auto e = GetLastError();

    std::wcerr
      << L"failed to write mini dump, " << formatSystemMessage(e) << L"\n";

    return false;
  }

  std::wclog << L"minidump written correctly\n";
  return true;
}


bool coredump(CoreDumpTypes type)
{
  std::wclog << L"creating minidump for the current process\n";
  return createMiniDump(GetCurrentProcess(), type);
}

bool coredumpOther(CoreDumpTypes type)
{
  std::wclog << L"creating minidump for an running process\n";

  const auto pid = findOtherPid();
  if (pid == 0) {
    std::wcerr << L"no other process found\n";
    return false;
  }

  std::wclog << L"found other process with pid " << pid << L"\n";

  HandlePtr handle(OpenProcess(
    PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid));

  if (!handle) {
    const auto e = GetLastError();

    std::wcerr
      << L"failed to open process " << pid << L", "
      << formatSystemMessage(e) << L"\n";

    return false;
  }

  return createMiniDump(handle.get(), type);
}

} // namespace
