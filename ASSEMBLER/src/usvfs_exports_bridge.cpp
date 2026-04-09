#define private public
#include "hookmanager.h"
#undef private
#include "loghelpers.h"
#include "logging.h"
#include "redirectiontree.h"
#include "hookcontext.h"
#include "usvfs.h"
#include "usvfs_version.h"
#include "usvfsparametersprivate.h"

#ifdef USVFS_TARGET_V244
#include <sinks/stdout_sinks.h>
#include <sinks/null_sink.h>
#include <fmt/fmt.h>
#define STDOUT_SINK_MT_T spdlog::sinks::stdout_sink_mt
#define NULL_SINK_MT_T spdlog::sinks::null_sink_mt
#define SHM_SINK_T spdlog::sinks::shm_sink
#define DisconnectVFS ::DisconnectVFS
#define VirtualLinkDirectoryStatic ::VirtualLinkDirectoryStatic
#define VirtualLinkFile ::VirtualLinkFile
#define CreateMiniDump ::CreateMiniDump
#else
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/fmt/fmt.h>
#define DisconnectVFS ::usvfsDisconnectVFS
#define VirtualLinkDirectoryStatic ::usvfsVirtualLinkDirectoryStatic
#define VirtualLinkFile ::usvfsVirtualLinkFile
#define CreateMiniDump ::usvfsCreateMiniDump
#define SHM_SINK_T usvfs::sinks::shm_sink
#define STDOUT_SINK_MT_T spdlog::sinks::stdout_sink_mt
#define NULL_SINK_MT_T spdlog::sinks::null_sink_mt
#endif

#include <directory_tree.h>
#include <shmlogger.h>
#include <ttrampolinepool.h>
#include <stringcast.h>
#include <stringutils.h>
#include <winapi.h>
#include <inject.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace bfs = boost::filesystem;
namespace ush = usvfs::shared;
using usvfs::log::ConvertLogLevel;

usvfs::HookManager* manager = nullptr;
usvfs::HookContext* context = nullptr;
HMODULE dllModule = nullptr;
PVOID exceptionHandler = nullptr;
CrashDumpsType usvfs_dump_type = CrashDumpsType::None;
std::wstring usvfs_dump_path;

#ifdef USVFS_TARGET_V244
// V244 defines sinks in headers we already included
#else
// Newer spdlog patterns
#endif

namespace {

class HookManagerProxy : public usvfs::HookManager {
public:
  using usvfs::HookManager::HookManager;
  ~HookManagerProxy() = default;
  using usvfs::HookManager::detour;
  using usvfs::HookManager::removeHook;
  using usvfs::HookManager::logStubInt;
  using usvfs::HookManager::installHook;
  using usvfs::HookManager::installStub;
  using usvfs::HookManager::initHooks;
  using usvfs::HookManager::removeHooks;
};

template <typename T>
HookManagerProxy* asHookManager(T* self) {
  return reinterpret_cast<HookManagerProxy*>(self);
}

template <typename T>
usvfs::HookContext* asHookContext(T* self) {
  return reinterpret_cast<usvfs::HookContext*>(self);
}

bool extensionMatchesCI(std::string_view name, std::string_view extension)
{
  const std::size_t extensionLengthWithDot = extension.size() + 1;
  if (name.size() < extensionLengthWithDot) {
    return false;
  }

  const auto dotIndex = name.size() - extensionLengthWithDot;
  if (name[dotIndex] != '.') {
    return false;
  }

  const char* p = name.data() + dotIndex + 1;
  for (std::size_t i = 0; i < extension.size(); ++i) {
    const auto expected = extension[i];
    const auto upper = static_cast<char>(std::toupper(
        static_cast<unsigned char>(expected)));
    if (p[i] != expected && p[i] != upper) {
      return false;
    }
  }

  return true;
}

bool exceptionInUSVFS(PEXCEPTION_POINTERS exceptionPtrs)
{
  if (!dllModule) {
    return true;
  }

  const auto range = winapi::ex::getSectionRange(dllModule);
  const auto exceptionAddress = reinterpret_cast<uintptr_t>(
      exceptionPtrs->ExceptionRecord->ExceptionAddress);

  return range.first <= exceptionAddress && exceptionAddress < range.second;
}

} // namespace

bool shouldAddToInverseTree(std::string_view name)
{
  return extensionMatchesCI(name, "exe") || extensionMatchesCI(name, "dll");
}

void InitLoggingInternal(bool toConsole, bool connectExistingSHM)
{
  try {
    if (!toConsole && !SHMLogger::isInstantiated()) {
      if (connectExistingSHM) {
        SHMLogger::open("usvfs");
      } else {
        SHMLogger::create("usvfs");
      }
    }

    spdlog::drop("usvfs");
#pragma message("need a customized name for the shm")
    auto logger = spdlog::get("usvfs");
    if (logger.get() == nullptr) {
      logger = toConsole ? spdlog::create<STDOUT_SINK_MT_T>("usvfs")
                         : spdlog::create<SHM_SINK_T>("usvfs", "usvfs");
      logger->set_pattern("%H:%M:%S.%e [%L] %v");
    }
    logger->set_level(spdlog::level::debug);

    spdlog::drop("hooks");
    logger = spdlog::get("hooks");
    if (logger.get() == nullptr) {
      logger = toConsole ? spdlog::create<STDOUT_SINK_MT_T>("hooks")
                         : spdlog::create<SHM_SINK_T>("hooks", "usvfs");
      logger->set_pattern("%H:%M:%S.%e <%P:%t> [%L] %v");
    }
    logger->set_level(spdlog::level::debug);
  } catch (const std::exception&) {
    if (spdlog::get("usvfs").get() == nullptr) {
      spdlog::create<NULL_SINK_MT_T>("usvfs");
    }
    if (spdlog::get("hooks").get() == nullptr) {
      spdlog::create<NULL_SINK_MT_T>("hooks");
    }
  }

  spdlog::get("usvfs")->info("usvfs dll {} initialized in process {}",
                             USVFS_VERSION_STRING, GetCurrentProcessId());
}

void SetLogLevel(LogLevel level)
{
  spdlog::get("usvfs")->set_level(ConvertLogLevel(level));
  spdlog::get("hooks")->set_level(ConvertLogLevel(level));
}

std::wstring generate_minidump_name(const wchar_t* dumpPath)
{
  const DWORD pid = GetCurrentProcessId();
  wchar_t pname[100];
  if (GetModuleBaseName(GetCurrentProcess(), NULL, pname, _countof(pname))
      == 0) {
    return std::wstring();
  }

  wchar_t dmpFile[MAX_PATH];
  int count = 0;
  _snwprintf_s(dmpFile, _TRUNCATE, L"%s\\%s-%lu.dmp", dumpPath, pname, pid);
  while (winapi::ex::wide::fileExists(dmpFile)) {
    if (++count > 99) {
      return std::wstring();
    }
    _snwprintf_s(dmpFile, _TRUNCATE, L"%s\\%s-%lu_%02d.dmp", dumpPath, pname,
                 pid, count);
  }

  return dmpFile;
}

int createMiniDumpImpl(PEXCEPTION_POINTERS exceptionPtrs, CrashDumpsType type,
                       const wchar_t* dumpPath, HMODULE dbgDLL)
{
  typedef BOOL(WINAPI * FuncMiniDumpWriteDump)(
      HANDLE process, DWORD pid, HANDLE file, MINIDUMP_TYPE dumpType,
      const PMINIDUMP_EXCEPTION_INFORMATION exceptionParam,
      const PMINIDUMP_USER_STREAM_INFORMATION userStreamParam,
      const PMINIDUMP_CALLBACK_INFORMATION callbackParam);

  winapi::ex::wide::createPath(dumpPath);

  const auto dmpName = generate_minidump_name(dumpPath);
  if (dmpName.empty()) {
    return 4;
  }

  const auto funcDump = reinterpret_cast<FuncMiniDumpWriteDump>(
      GetProcAddress(dbgDLL, "MiniDumpWriteDump"));
  if (!funcDump) {
    return 5;
  }

  HANDLE dumpFile = winapi::wide::createFile(dmpName)
                        .createAlways()
                        .access(GENERIC_WRITE)
                        .share(FILE_SHARE_WRITE)();
  if (dumpFile == INVALID_HANDLE_VALUE) {
    return 6;
  }

  DWORD dumpType = MiniDumpNormal | MiniDumpWithHandleData |
                   MiniDumpWithUnloadedModules | MiniDumpWithProcessThreadData;
  if (type == CrashDumpsType::Data) {
    dumpType |= MiniDumpWithDataSegs;
  }
  if (type == CrashDumpsType::Full) {
    dumpType |= MiniDumpWithFullMemory;
  }

  _MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
  exceptionInfo.ThreadId = GetCurrentThreadId();
  exceptionInfo.ExceptionPointers = exceptionPtrs;
  exceptionInfo.ClientPointers = FALSE;

  const BOOL success = funcDump(GetCurrentProcess(), GetCurrentProcessId(),
                                dumpFile, static_cast<MINIDUMP_TYPE>(dumpType),
                                &exceptionInfo, nullptr, nullptr);

  CloseHandle(dumpFile);
  return success ? 0 : 7;
}

LONG WINAPI VEHandler(PEXCEPTION_POINTERS exceptionPtrs)
{
  if ((exceptionPtrs->ExceptionRecord->ExceptionCode < 0x80000000) ||
      (exceptionPtrs->ExceptionRecord->ExceptionCode == 0xe06d7363)) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  if (!exceptionInUSVFS(exceptionPtrs)) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  HookLib::TrampolinePool& trampPool = HookLib::TrampolinePool::instance();
  if (&trampPool) {
    trampPool.forceUnlockBarrier();
    trampPool.setBlock(true);
  }

  CreateMiniDump(exceptionPtrs, usvfs_dump_type, usvfs_dump_path.c_str());
  return EXCEPTION_CONTINUE_SEARCH;
}

bool processStillActive(DWORD pid)
{
  HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (proc == nullptr) {
    return false;
  }

  DWORD exitCode = 0;
  const BOOL ok = GetExitCodeProcess(proc, &exitCode);
  const DWORD lastError = ok ? ERROR_SUCCESS : GetLastError();
  CloseHandle(proc);

  if (!ok) {
    spdlog::get("usvfs")->warn("failed to query exit code on process {}: {}",
                               pid, lastError);
    return false;
  }

  return exitCode == STILL_ACTIVE;
}

bool assertPathExists(usvfs::RedirectionTreeContainer& table, LPCWSTR path)
{
  bfs::path p(path);
  p = p.parent_path();

  usvfs::RedirectionTree::NodeT* current = table.get();

  for (auto iter = p.begin(); iter != p.end();
       iter = ush::nextIter(iter, p.end())) {
    if (current->exists(iter->string().c_str())) {
      auto found = current->node(iter->string().c_str());
      current = found.get().get();
      continue;
    }

    bfs::path targetPath = current->data().linkTarget.size() > 0
                               ? bfs::path(current->data().linkTarget.c_str()) /
                                     *iter
                               : *iter / "\\";

    if (is_directory(targetPath) || is_symlink(targetPath) ||
        status(targetPath).type() == bfs::file_type::reparse_file) {
      auto newNode = table.addDirectory(current->path() / *iter,
                                        targetPath.string().c_str(),
                                        ush::FLAG_DUMMY, false);
      current = newNode.get().get();
    } else {
      spdlog::get("usvfs")->info("{} doesn't exist", targetPath.string());
      return false;
    }
  }

  return true;
}

extern "C" std::ostream& __cdecl usvfsAsmStreamRedirectionDataImpl(
    std::ostream& stream, const usvfs::RedirectionData& data)
{
  stream << data.linkTarget;
  return stream;
}

#ifndef _WIN64
namespace usvfs {
std::ostream& operator<<(std::ostream& stream, const RedirectionData& data)
{
  return usvfsAsmStreamRedirectionDataImpl(stream, data);
}
}
#endif

extern "C" void __cdecl usvfsAsmRecursiveBenaphoreOwnerDiedLogImpl(
    DWORD ownerId)
{
  spdlog::get("usvfs")->error("thread {} never released the mutex", ownerId);
}

extern "C" void WINAPI usvfsAsmInitLoggingImpl(bool toConsole)
{
  InitLoggingInternal(toConsole, false);
}

extern "C" bool WINAPI usvfsAsmGetLogMessagesImpl(LPSTR buffer, size_t size,
                                                  bool blocking)
{
  buffer[0] = '\0';
  try {
    if (blocking) {
      SHMLogger::instance().get(buffer, size);
      return true;
    } else {
      return SHMLogger::instance().tryGet(buffer, size);
    }
  } catch (const std::exception& e) {
    _snprintf_s(buffer, size, _TRUNCATE,
                "Failed to retrieve log messages: %s", e.what());
    return false;
  }
}

extern "C" BOOL WINAPI usvfsAsmGetVFSProcessListImpl(size_t* count,
                                                     LPDWORD processIDs)
{
  if (count == nullptr) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }

  if (context == nullptr) {
    *count = 0;
  } else {
    std::vector<DWORD> pids = context->registeredProcesses();
    size_t realCount = 0;
    for (DWORD pid : pids) {
      if (processStillActive(pid)) {
        if ((realCount < *count) && (processIDs != nullptr)) {
          processIDs[realCount] = pid;
        }

        ++realCount;
      }
    }
    *count = realCount;
  }
  return TRUE;
}

extern "C" BOOL WINAPI usvfsAsmGetVFSProcessList2Impl(size_t* count,
                                                      DWORD** buffer)
{
  if (!count || !buffer) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }

  *count = 0;
  *buffer = nullptr;

  std::vector<DWORD> pids = context->registeredProcesses();
  auto last = std::remove_if(pids.begin(), pids.end(), [](DWORD id) {
    return !processStillActive(id);
  });

  pids.erase(last, pids.end());

  if (pids.empty()) {
    return TRUE;
  }

  *count = pids.size();
  *buffer = static_cast<DWORD*>(std::calloc(pids.size(), sizeof(DWORD)));

  std::copy(pids.begin(), pids.end(), *buffer);

  return TRUE;
}

extern "C" usvfs::HookContext* __cdecl usvfsAsmCreateHookContextImpl(
    const USVFSParameters& oldParams, HMODULE module)
{
  const usvfsParameters p(oldParams);
  return new usvfs::HookContext(p, module);
}

extern "C" usvfs::HookContext* WINAPI usvfsAsmUsvfsCreateHookContextImpl(
    const usvfsParameters& params, HMODULE module)
{
  return new usvfs::HookContext(params, module);
}

extern "C" int WINAPI usvfsAsmCreateMiniDumpImpl(
    PEXCEPTION_POINTERS exceptionPtrs, CrashDumpsType type,
    const wchar_t* dumpPath)
{
  if (type == CrashDumpsType::None) {
    return 0;
  }

  int res = 1;
  if (HMODULE dbgDLL = LoadLibraryW(L"dbghelp.dll")) {
    try {
      res = createMiniDumpImpl(exceptionPtrs, type, dumpPath, dbgDLL);
    } catch (...) {
      res = 2;
    }
    FreeLibrary(dbgDLL);
  }
  return res;
}

extern "C" void __cdecl usvfsAsmInitHooksImpl(LPVOID parameters, size_t)
{
  InitLoggingInternal(false, true);

  const usvfsParameters* params =
      reinterpret_cast<usvfsParameters*>(parameters);
  usvfs_dump_type = params->crashDumpsType;
  usvfs_dump_path =
      ush::string_cast<std::wstring>(params->crashDumpsPath,
                                     ush::CodePage::UTF8);

  if (params->delayProcessMs > 0) {
    ::Sleep(static_cast<unsigned long>(params->delayProcessMs));
  }

  SetLogLevel(params->logLevel);

  if (exceptionHandler == nullptr) {
    if (usvfs_dump_type != CrashDumpsType::None) {
      exceptionHandler = ::AddVectoredExceptionHandler(0, VEHandler);
    }
  } else {
    spdlog::get("usvfs")->info("vectored exception handler already active");
  }

  spdlog::get("usvfs")
      ->info("inithooks called {0} in process {1}:{2} (log level {3}, dump "
             "type {4}, dump path {5})",
             params->instanceName, winapi::ansi::getModuleFileName(nullptr),
             ::GetCurrentProcessId(), static_cast<int>(params->logLevel),
             static_cast<int>(params->crashDumpsType),
             params->crashDumpsPath);

  try {
    manager = new usvfs::HookManager(*params, dllModule);

    auto localContext = manager->context();
    auto exePath = boost::dll::program_location();
    auto libraries = localContext->librariesToForceLoad(exePath.filename().c_str());
    for (auto library : libraries) {
      if (std::filesystem::exists(library)) {
        const auto ret = LoadLibraryExW(library.c_str(), NULL, 0);
        if (ret) {
          spdlog::get("usvfs")->info(
              "inithooks succeeded to force load {0}",
              ush::string_cast<std::string>(library).c_str());
        } else {
          spdlog::get("usvfs")->critical(
              "inithooks failed to force load {0}",
              ush::string_cast<std::string>(library).c_str());
        }
      }
    }

    spdlog::get("usvfs")
        ->info("inithooks in process {0} successful", ::GetCurrentProcessId());
  } catch (const std::exception& e) {
    spdlog::get("usvfs")->debug("failed to initialise hooks: {0}", e.what());
  }
}

extern "C" void WINAPI usvfsAsmUSVFSUpdateParamsImpl(LogLevel level,
                                                     CrashDumpsType type)
{
  auto* p = usvfsCreateParameters();

  usvfsSetLogLevel(p, level);
  usvfsSetCrashDumpType(p, type);

  usvfsUpdateParameters(p);
  usvfsFreeParameters(p);
}

extern "C" void usvfsAsmHookManagerRemoveHooksImpl(usvfs::HookManager* self)
{
  asHookManager(self)->removeHooks();
}

void usvfs::HookManager::removeHooks() {
    // Process usually exits, so we can ignore explicit unhooking for now
}

namespace usvfs::shared {
std::string windows_error::constructMessage(const std::string& msg, int code) {
    return msg + " (" + std::to_string(code) + ")";
}
}

#ifndef _WIN64
void usvfsParameters::setCrashDumpPath(const char* path) {
    strncpy_s(this->crashDumpsPath, path, _TRUNCATE);
}
#endif

extern "C" void WINAPI usvfsAsmUsvfsUpdateParametersImpl(usvfsParameters* p)
{
  spdlog::get("usvfs")->info(
      "updating parameters:\n"
      " . debugMode: {}\n"
      " . log level: {}\n"
      " . dump type: {}\n"
      " . dump path: {}\n"
      " . delay process: {}ms",
      p->debugMode, usvfsLogLevelToString(p->logLevel),
      usvfsCrashDumpTypeToString(p->crashDumpsType), p->crashDumpsPath,
      p->delayProcessMs);

  usvfs_dump_type = p->crashDumpsType;
  usvfs_dump_path = ush::string_cast<std::wstring>(p->crashDumpsPath,
                                                   ush::CodePage::UTF8);
  SetLogLevel(p->logLevel);

  context->setDebugParameters(p->logLevel, p->crashDumpsType, p->crashDumpsPath,
                              std::chrono::milliseconds(p->delayProcessMs));
}

extern "C" void WINAPI usvfsAsmGetCurrentVFSNameImpl(char* buffer, size_t size)
{
  ush::strncpy_sz(buffer, context->callParameters().currentSHMName, size);
}

extern "C" BOOL WINAPI usvfsAsmCreateVFSImpl(const USVFSParameters* oldParams)
{
  const usvfsParameters p(*oldParams);
  const auto r = usvfsCreateVFS(&p);

  return r;
}

extern "C" BOOL WINAPI usvfsAsmUsvfsCreateVFSImpl(const usvfsParameters* p)
{
  usvfs::HookContext::remove(p->instanceName);
  return usvfsConnectVFS(p);
}

extern "C" BOOL WINAPI usvfsAsmConnectVFSImpl(const USVFSParameters* oldParams)
{
  const usvfsParameters p(*oldParams);
  const auto r = usvfsConnectVFS(&p);

  return r;
}

extern "C" BOOL WINAPI usvfsAsmUsvfsConnectVFSImpl(
    const usvfsParameters* params)
{
  if (spdlog::get("usvfs").get() == nullptr) {
    spdlog::create<NULL_SINK_MT_T>("usvfs");
  }

  try {
    DisconnectVFS();
    context = new usvfs::HookContext(*params, dllModule);

    return TRUE;
  } catch (const std::exception& e) {
    spdlog::get("usvfs")->debug("failed to connect to vfs: {}", e.what());
    return FALSE;
  }
}

extern "C" void WINAPI usvfsAsmDisconnectVFSImpl()
{
  if (spdlog::get("usvfs").get() == nullptr) {
    spdlog::create<NULL_SINK_MT_T>("usvfs");
  }

  spdlog::get("usvfs")->debug("remove from process {}", GetCurrentProcessId());

  if (manager != nullptr) {
    delete manager;
    manager = nullptr;
  }

  if (context != nullptr) {
    delete context;
    context = nullptr;
    spdlog::get("usvfs")->debug("vfs unloaded");
  }
}

extern "C" void WINAPI usvfsAsmClearVirtualMappingsImpl()
{
  context->redirectionTable()->clear();
  context->inverseTable()->clear();
}

extern "C" BOOL WINAPI usvfsAsmCreateVFSDumpImpl(LPSTR buffer, size_t* size)
{
  assert(size != nullptr);
  std::ostringstream output;
  usvfs::shared::dumpTree(output, *context->redirectionTable().get());
  std::string str = output.str();
  if ((buffer != NULL) && (*size > 0)) {
    strncpy_s(buffer, *size, str.c_str(), _TRUNCATE);
  }
  bool success = *size >= str.length();
  *size = str.length();
  return success ? TRUE : FALSE;
}

static usvfs::shared::TreeFlags
usvfsAsmConvertRedirectionFlags(unsigned int flags)
{
  usvfs::shared::TreeFlags result = 0;
  if (flags & LINKFLAG_CREATETARGET) {
    result |= usvfs::shared::FLAG_CREATETARGET;
  }
  return result;
}

extern "C" BOOL WINAPI usvfsAsmVirtualLinkFileImpl(LPCWSTR source,
                                                   LPCWSTR destination,
                                                   unsigned int flags)
{
  try {
    if (!assertPathExists(context->redirectionTable(), destination)) {
      SetLastError(ERROR_PATH_NOT_FOUND);
      return FALSE;
    }

    std::string sourceU8 =
        ush::string_cast<std::string>(source, ush::CodePage::UTF8);
    auto res = context->redirectionTable().addFile(
        bfs::path(destination), usvfs::RedirectionDataLocal(sourceU8),
        !(flags & LINKFLAG_FAILIFEXISTS));

    if (shouldAddToInverseTree(sourceU8)) {
      std::string destinationU8 =
          ush::string_cast<std::string>(destination, ush::CodePage::UTF8);

      context->inverseTable().addFile(
          bfs::path(source), usvfs::RedirectionDataLocal(destinationU8), true);
    }

    context->updateParameters();

    if (res.get() == nullptr) {
      SetLastError(ERROR_FILE_EXISTS);
      return FALSE;
    } else {
      return TRUE;
    }
  } catch (const std::exception& e) {
    spdlog::get("usvfs")->error("failed to copy file {}", e.what());
    SetLastError(ERROR_INVALID_DATA);
    return FALSE;
  }
}

extern "C" BOOL WINAPI usvfsAsmVirtualLinkDirectoryStaticImpl(
    LPCWSTR source, LPCWSTR destination, unsigned int flags)
{
  try {
    if ((flags & LINKFLAG_FAILIFEXISTS)
        && winapi::ex::wide::fileExists(destination)) {
      SetLastError(ERROR_FILE_EXISTS);
      return FALSE;
    }

    if (!assertPathExists(context->redirectionTable(), destination)) {
      SetLastError(ERROR_PATH_NOT_FOUND);
      return FALSE;
    }

    std::string sourceU8 =
        ush::string_cast<std::string>(source, ush::CodePage::UTF8) + "\\";

    context->redirectionTable().addDirectory(
        destination, usvfs::RedirectionDataLocal(sourceU8),
        usvfs::shared::FLAG_DIRECTORY |
            usvfsAsmConvertRedirectionFlags(flags),
        (flags & LINKFLAG_CREATETARGET) != 0);

    if ((flags & LINKFLAG_RECURSIVE) != 0) {
      std::wstring sourceP(source);
      std::wstring sourceW = sourceP + L"\\";
      std::wstring destinationW = std::wstring(destination) + L"\\";
      if (sourceP.length() >= MAX_PATH
          && !ush::startswith(sourceP.c_str(), LR"(\\?\)")) {
        sourceP = LR"(\\?\)" + sourceP;
      }

      for (winapi::ex::wide::FileResult file :
           winapi::ex::wide::quickFindFiles(sourceP.c_str(), L"*")) {
        if (file.attributes & FILE_ATTRIBUTE_DIRECTORY) {
          if ((file.fileName != L".") && (file.fileName != L"..")) {
            VirtualLinkDirectoryStatic((sourceW + file.fileName).c_str(),
                                       (destinationW + file.fileName).c_str(),
                                       flags);
          }
        } else {
          std::string nameU8 = ush::string_cast<std::string>(
              file.fileName.c_str(), ush::CodePage::UTF8);

          context->redirectionTable().addFile(
              bfs::path(destination) / nameU8,
              usvfs::RedirectionDataLocal(sourceU8 + nameU8), true);

          if (shouldAddToInverseTree(nameU8)) {
            std::string destinationU8 = ush::string_cast<std::string>(
                                            destination, ush::CodePage::UTF8)
                                        + "\\";

            context->inverseTable().addFile(
                bfs::path(source) / nameU8,
                usvfs::RedirectionDataLocal(destinationU8 + nameU8), true);
          }
        }
      }
    }

    context->updateParameters();

    return TRUE;
  } catch (const std::exception& e) {
    spdlog::get("usvfs")->error("failed to copy file {}", e.what());
    SetLastError(ERROR_INVALID_DATA);
    return FALSE;
  }
}

extern "C" BOOL WINAPI usvfsAsmCreateProcessHookedImpl(
    LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles,
    DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory,
    LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
  BOOL susp = dwCreationFlags & CREATE_SUSPENDED;
  DWORD flags = dwCreationFlags | CREATE_SUSPENDED;

  BOOL blacklisted =
      context->executableBlacklisted(lpApplicationName, lpCommandLine);

  BOOL res = CreateProcessW(lpApplicationName, lpCommandLine,
                            lpProcessAttributes, lpThreadAttributes,
                            bInheritHandles, flags, lpEnvironment,
                            lpCurrentDirectory, lpStartupInfo,
                            lpProcessInformation);
  if (!res) {
    spdlog::get("usvfs")->error(
        "failed to spawn {}", ush::string_cast<std::string>(lpCommandLine));
    return FALSE;
  }

  if (!blacklisted) {
    std::wstring applicationDirPath = winapi::wide::getModuleFileName(dllModule);
    boost::filesystem::path p(applicationDirPath);
    try {
      usvfs::injectProcess(p.parent_path().wstring(), context->callParameters(),
                           *lpProcessInformation);
    } catch (const std::exception& e) {
      spdlog::get("usvfs")->error("failed to inject: {}", e.what());
      logExtInfo(e, LogLevel::Error);
      ::TerminateProcess(lpProcessInformation->hProcess, 1);
      ::SetLastError(ERROR_INVALID_PARAMETER);
      return FALSE;
    }
  }

  if (!susp) {
    ResumeThread(lpProcessInformation->hThread);
  }

  return TRUE;
}

extern "C" void WINAPI usvfsAsmBlacklistExecutableImpl(LPWSTR executableName)
{
  context->blacklistExecutable(executableName);
}

extern "C" void WINAPI usvfsAsmClearExecutableBlacklistImpl()
{
  context->clearExecutableBlacklist();
}

extern "C" void WINAPI usvfsAsmForceLoadLibraryImpl(LPWSTR processName,
                                                    LPWSTR libraryPath)
{
  context->forceLoadLibrary(processName, libraryPath);
}

extern "C" void WINAPI usvfsAsmClearLibraryForceLoadsImpl()
{
  context->clearLibraryForceLoads();
}

extern "C" void WINAPI usvfsAsmPrintDebugInfoImpl()
{
  spdlog::get("usvfs")
      ->warn("===== debug {} =====", context->redirectionTable().shmName());
  void* buffer = nullptr;
  size_t bufferSize = 0;
  context->redirectionTable().getBuffer(buffer, bufferSize);
  std::ostringstream temp;
  for (size_t i = 0; i < bufferSize; ++i) {
    temp << std::hex << std::setfill('0') << std::setw(2)
         << (unsigned) reinterpret_cast<char*>(buffer)[i] << " ";
    if ((i % 16) == 15) {
      spdlog::get("usvfs")->info("{}", temp.str());
      temp.str("");
      temp.clear();
    }
  }
  if (!temp.str().empty()) {
    spdlog::get("usvfs")->info("{}", temp.str());
  }
  spdlog::get("usvfs")
      ->warn("===== / debug {} =====", context->redirectionTable().shmName());
}

extern "C" void WINAPI usvfsAsmUSVFSInitParametersImpl(
    USVFSParameters* parameters, const char* instanceName, bool debugMode,
    LogLevel logLevel, CrashDumpsType crashDumpsType,
    const char* crashDumpsPath)
{
  parameters->debugMode = debugMode;
  parameters->logLevel = logLevel;
  parameters->crashDumpsType = crashDumpsType;

  strncpy_s(parameters->instanceName, instanceName, _TRUNCATE);
  if (crashDumpsPath && *crashDumpsPath &&
      strlen(crashDumpsPath) < _countof(parameters->crashDumpsPath)) {
    memcpy(parameters->crashDumpsPath, crashDumpsPath,
           strlen(crashDumpsPath) + 1);
    parameters->crashDumpsType = crashDumpsType;
  } else {
    parameters->crashDumpsPath[0] = 0;
    parameters->crashDumpsType = CrashDumpsType::None;
  }
  strncpy_s(parameters->currentSHMName, 60, instanceName, _TRUNCATE);
  memset(parameters->currentInverseSHMName, '\0',
         _countof(parameters->currentInverseSHMName));
  _snprintf(parameters->currentInverseSHMName, 60, "inv_%s", instanceName);
}

extern "C" const char* WINAPI usvfsAsmUSVFSVersionStringImpl()
{
  return USVFS_VERSION_STRING;
}

#ifdef USVFS_TARGET_V252
extern "C" void WINAPI usvfsAsmUsvfsAddSkipDirectoryImpl(LPCWSTR directory)
{
  context->addSkipDirectory(directory);
}

extern "C" void WINAPI usvfsAsmUsvfsClearSkipDirectoriesImpl()
{
  context->clearSkipDirectories();
}

extern "C" void WINAPI usvfsAsmUsvfsAddSkipFileSuffixImpl(LPCWSTR fileSuffix)
{
  context->addSkipFileSuffix(fileSuffix);
}

extern "C" void WINAPI usvfsAsmUsvfsClearSkipFileSuffixesImpl()
{
  context->clearSkipFileSuffixes();
}
#else
extern "C" void WINAPI usvfsAsmUsvfsAddSkipDirectoryImpl(LPCWSTR) {}
extern "C" void WINAPI usvfsAsmUsvfsClearSkipDirectoriesImpl() {}
extern "C" void WINAPI usvfsAsmUsvfsAddSkipFileSuffixImpl(LPCWSTR) {}
extern "C" void WINAPI usvfsAsmUsvfsClearSkipFileSuffixesImpl() {}
#endif

BOOL APIENTRY DllMain(HMODULE module, DWORD reasonForCall, LPVOID)
{
  switch (reasonForCall) {
    case DLL_PROCESS_ATTACH:
      dllModule = module;
      break;
    case DLL_PROCESS_DETACH:
      if (exceptionHandler) {
        ::RemoveVectoredExceptionHandler(exceptionHandler);
      }
      break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
      break;
  }

  return TRUE;
}
