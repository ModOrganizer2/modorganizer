#include <cstddef>
#include <future>
#include <map>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include "exceptionex.h"

#define private public
#include "hookcontext.h"
#include "hookmanager.h"
#undef private

#include "hooks/kernel32.h"
#include "hooks/ntdll.h"
#include "loghelpers.h"
#include "usvfs.h"
#include "../thooklib/ttrampolinepool.h"
#include "../thooklib/utility.h"

#include <VersionHelpers.h>
#include <directory_tree.h>
#include <logging.h>

#ifdef USVFS_TARGET_V244
#include <fmt/fmt.h>
#define FORMAT_STRING fmt::format
#else
#include <format>
#define FORMAT_STRING std::format
#endif
#include <shared_memory.h>
#include <sharedparameters.h>
#include <shmlogger.h>
#include <usvfsparameters.h>
#include <winapi.h>

namespace bi = boost::interprocess;
namespace bf = boost::filesystem;
namespace ush = usvfs::shared;

using namespace HookLib;
using namespace usvfs;
using usvfs::shared::SharedMemoryT;
using usvfs::shared::VoidAllocatorT;

namespace usvfs {
HookContext* HookContext::s_Instance = nullptr;
HookManager* HookManager::s_Instance = nullptr;

ForcedLibrary::ForcedLibrary(const std::string& process,
                             const std::string& path,
                             const shared::VoidAllocatorT& alloc)
    : m_processName(process.c_str(), shared::CharAllocatorT(alloc))
    , m_libraryPath(path.c_str(), shared::CharAllocatorT(alloc))
{}
ForcedLibrary::~ForcedLibrary() {}

std::string ForcedLibrary::processName() const
{
  return {m_processName.begin(), m_processName.end()};
}

std::string ForcedLibrary::libraryPath() const
{
  return {m_libraryPath.begin(), m_libraryPath.end()};
}

SharedParameters::SharedParameters(const usvfsParameters& reference,
                                   const shared::VoidAllocatorT& allocator)
    : m_instanceName(reference.instanceName, allocator)
    , m_currentSHMName(reference.currentSHMName, allocator)
    , m_currentInverseSHMName(reference.currentInverseSHMName, allocator)
    , m_debugMode(reference.debugMode)
    , m_logLevel(reference.logLevel)
    , m_crashDumpsType(reference.crashDumpsType)
    , m_crashDumpsPath(reference.crashDumpsPath, allocator)
    , m_delayProcess(reference.delayProcessMs)
    , m_userCount(1)
    , m_processBlacklist(StringAllocatorT(allocator))
    , m_processList(DWORDAllocatorT(allocator))
#ifndef USVFS_TARGET_V244
    , m_fileSuffixSkipList(StringAllocatorT(allocator))
    , m_directorySkipList(StringAllocatorT(allocator))
#endif
    , m_forcedLibraries(ForcedLibraryAllocatorT(allocator))
{}

SharedParameters::~SharedParameters() {}

usvfsParameters SharedParameters::makeLocal() const
{
  bi::scoped_lock lock(m_mutex);

  return usvfsParameters(m_instanceName.c_str(), m_currentSHMName.c_str(),
                         m_currentInverseSHMName.c_str(), m_debugMode,
                         m_logLevel, m_crashDumpsType, m_crashDumpsPath.c_str(),
                         m_delayProcess.count());
}

std::string SharedParameters::instanceName() const
{
  bi::scoped_lock lock(m_mutex);
  return {m_instanceName.begin(), m_instanceName.end()};
}

std::string SharedParameters::currentSHMName() const
{
  bi::scoped_lock lock(m_mutex);
  return {m_currentSHMName.begin(), m_currentSHMName.end()};
}

std::string SharedParameters::currentInverseSHMName() const
{
  bi::scoped_lock lock(m_mutex);
  return {m_currentInverseSHMName.begin(), m_currentInverseSHMName.end()};
}

void SharedParameters::setSHMNames(const std::string& current,
                                   const std::string& inverse)
{
  bi::scoped_lock lock(m_mutex);

  m_currentSHMName.assign(current.begin(), current.end());
  m_currentInverseSHMName.assign(inverse.begin(), inverse.end());
}

void SharedParameters::setDebugParameters(LogLevel level,
                                          CrashDumpsType dumpType,
                                          const std::string& dumpPath,
                                          std::chrono::milliseconds delayProcess)
{
  bi::scoped_lock lock(m_mutex);

  m_logLevel = level;
  m_crashDumpsType = dumpType;
  m_crashDumpsPath.assign(dumpPath.begin(), dumpPath.end());
  m_delayProcess = delayProcess;
}

std::size_t SharedParameters::userConnected()
{
  bi::scoped_lock lock(m_mutex);
  return ++m_userCount;
}

std::size_t SharedParameters::userDisconnected()
{
  bi::scoped_lock lock(m_mutex);
  return --m_userCount;
}

std::size_t SharedParameters::userCount()
{
  bi::scoped_lock lock(m_mutex);
  return m_userCount;
}

std::size_t SharedParameters::registeredProcessCount() const
{
  bi::scoped_lock lock(m_mutex);
  return m_processList.size();
}

std::vector<DWORD> SharedParameters::registeredProcesses() const
{
  bi::scoped_lock lock(m_mutex);
  return {m_processList.begin(), m_processList.end()};
}

void SharedParameters::registerProcess(DWORD pid)
{
  bi::scoped_lock lock(m_mutex);
  m_processList.insert(pid);
}

void SharedParameters::unregisterProcess(DWORD pid)
{
  {
    bi::scoped_lock lock(m_mutex);

    const auto it = m_processList.find(pid);
    if (it != m_processList.end()) {
      m_processList.erase(it);
      return;
    }
  }

  spdlog::get("usvfs")->error("cannot unregister process {}, not in list", pid);
}

void SharedParameters::blacklistExecutable(const std::string& name)
{
  bi::scoped_lock lock(m_mutex);
  m_processBlacklist.insert(shared::StringT(
      name.begin(), name.end(), m_processBlacklist.get_allocator()));
}

void SharedParameters::clearExecutableBlacklist()
{
  bi::scoped_lock lock(m_mutex);
  m_processBlacklist.clear();
}

bool SharedParameters::executableBlacklisted(const std::string& appName,
                                             const std::string& cmdLine) const
{
  bool blacklisted = false;
  std::string log;

  {
    bi::scoped_lock lock(m_mutex);

    for (const shared::StringT& sitem : m_processBlacklist) {
      const auto item = "\\" + std::string(sitem.begin(), sitem.end());

      if (!appName.empty() && boost::algorithm::iends_with(appName, item)) {
        blacklisted = true;
        log = FORMAT_STRING("application {} is blacklisted", appName);
        break;
      }

      if (!cmdLine.empty() && boost::algorithm::icontains(cmdLine, item)) {
        blacklisted = true;
        log = FORMAT_STRING("command line {} is blacklisted", cmdLine);
        break;
      }
    }
  }

  if (blacklisted) {
    spdlog::get("usvfs")->info(log);
    return true;
  }

  return false;
}

void SharedParameters::addForcedLibrary(const std::string& processName,
                                        const std::string& libraryPath)
{
  bi::scoped_lock lock(m_mutex);
  m_forcedLibraries.push_front(ForcedLibrary(
      processName, libraryPath, m_forcedLibraries.get_allocator()));
}

std::vector<std::string> SharedParameters::forcedLibraries(
    const std::string& processName)
{
  std::vector<std::string> result;

  {
    bi::scoped_lock lock(m_mutex);

    for (const auto& lib : m_forcedLibraries) {
      if (boost::algorithm::iequals(processName, lib.processName())) {
        result.push_back(lib.libraryPath());
      }
    }
  }

  return result;
}

void SharedParameters::clearForcedLibraries()
{
  bi::scoped_lock lock(m_mutex);
  m_forcedLibraries.clear();
}

#ifndef USVFS_TARGET_V244
void SharedParameters::addSkipFileSuffix(const std::string& fileSuffix)
{
  bi::scoped_lock lock(m_mutex);
  m_fileSuffixSkipList.insert(shared::StringT(
      fileSuffix.begin(), fileSuffix.end(), m_fileSuffixSkipList.get_allocator()));
}

void SharedParameters::clearSkipFileSuffixes()
{
  bi::scoped_lock lock(m_mutex);
  m_fileSuffixSkipList.clear();
}

std::vector<std::string> SharedParameters::skipFileSuffixes() const
{
  std::vector<std::string> res;
  bi::scoped_lock lock(m_mutex);
  for (const shared::StringT& s : m_fileSuffixSkipList) {
    res.emplace_back(s.begin(), s.end());
  }
  return res;
}

void SharedParameters::addSkipDirectory(const std::string& directory)
{
  bi::scoped_lock lock(m_mutex);
  m_directorySkipList.insert(shared::StringT(
      directory.begin(), directory.end(), m_directorySkipList.get_allocator()));
}

void SharedParameters::clearSkipDirectories()
{
  bi::scoped_lock lock(m_mutex);
  m_directorySkipList.clear();
}

std::vector<std::string> SharedParameters::skipDirectories() const
{
  std::vector<std::string> res;
  bi::scoped_lock lock(m_mutex);
  for (const shared::StringT& s : m_directorySkipList) {
    res.emplace_back(s.begin(), s.end());
  }
  return res;
}
#endif
} // namespace usvfs

namespace {

struct HookContextProxy
{
  using ConstPtr =
      std::unique_ptr<const usvfs::HookContext, void (*)(const usvfs::HookContext*)>;
  using Ptr =
      std::unique_ptr<usvfs::HookContext, void (*)(usvfs::HookContext*)>;
  using DataIDT = usvfs::HookContext::DataIDT;

  HookContextProxy(const usvfsParameters& params, HMODULE module)
      : m_ConfigurationSHM(bi::open_or_create, params.instanceName, 64 * 1024)
      , m_Parameters(retrieveParameters(params))
      , m_Tree(m_Parameters->currentSHMName(), 4 * 1024 * 1024)
      , m_InverseTree(m_Parameters->currentInverseSHMName(), 128 * 1024)
      , m_DebugMode(params.debugMode)
      , m_DLLModule(module)
  {
    if (usvfs::HookContext::s_Instance != nullptr) {
      throw std::runtime_error("singleton duplicate instantiation (HookContext)");
    }

    const auto userCount = m_Parameters->userConnected();

    spdlog::get("usvfs")->debug(
        "context current shm: {0} (now {1} connections)",
        m_Parameters->currentSHMName(), userCount);

    usvfs::HookContext::s_Instance =
        reinterpret_cast<usvfs::HookContext*>(this);

    if (m_Tree.get() == nullptr) {
      USVFS_THROW_EXCEPTION(usage_error() << ex_msg("shm not found")
                                          << ex_msg(params.instanceName));
    }
  }

  ~HookContextProxy()
  {
    spdlog::get("usvfs")->info("releasing hook context");

    usvfs::HookContext::s_Instance = nullptr;
    const auto userCount = m_Parameters->userDisconnected();

    if (userCount == 0) {
      spdlog::get("usvfs")->info("removing tree {}", m_Parameters->instanceName());
      bi::shared_memory_object::remove(m_Parameters->instanceName().c_str());
    } else {
      spdlog::get("usvfs")->info("{} users left", userCount);
    }
  }

  SharedParameters* retrieveParameters(const usvfsParameters& params)
  {
    std::pair<SharedParameters*, SharedMemoryT::size_type> res =
        m_ConfigurationSHM.find<SharedParameters>("parameters");

    if (res.first == nullptr) {
      spdlog::get("usvfs")->info("create config in {}", ::GetCurrentProcessId());

      res.first = m_ConfigurationSHM.construct<SharedParameters>("parameters")(
          params, VoidAllocatorT(m_ConfigurationSHM.get_segment_manager()));

      if (res.first == nullptr) {
        USVFS_THROW_EXCEPTION(bi::bad_alloc());
      }
    } else {
      spdlog::get("usvfs")->info(
          "access existing config in {}", ::GetCurrentProcessId());
    }

    spdlog::get("usvfs")->info(
        "{} processes", res.first->registeredProcessCount());

    return res.first;
  }

  SharedMemoryT m_ConfigurationSHM;
  SharedParameters* m_Parameters{nullptr};
  RedirectionTreeContainer m_Tree;
  RedirectionTreeContainer m_InverseTree;
  std::vector<std::future<int>> m_Futures;
  mutable std::map<DataIDT, boost::any> m_CustomData;
  bool m_DebugMode{false};
  HMODULE m_DLLModule;
  mutable RecursiveBenaphore m_Mutex;
};

static_assert(sizeof(HookContextProxy) == sizeof(usvfs::HookContext),
              "HookContext proxy size drifted");
static_assert(alignof(HookContextProxy) == alignof(usvfs::HookContext),
              "HookContext proxy alignment drifted");
static_assert(offsetof(RecursiveBenaphore, m_Semaphore)
                  == (sizeof(void*) == 8 ? 0x10 : 0x0C),
              "RecursiveBenaphore semaphore offset drifted");
static_assert(sizeof(RecursiveBenaphore) == (sizeof(void*) == 8 ? 0x18 : 0x10),
              "RecursiveBenaphore size drifted");

struct HookManagerProxy
{
  HookManagerProxy(const usvfsParameters& params, HMODULE module)
      : m_Context(params, module)
  {
    if (usvfs::HookManager::s_Instance != nullptr) {
      throw std::runtime_error("singleton duplicate instantiation (HookManager)");
    }

    usvfs::HookManager::s_Instance =
        reinterpret_cast<usvfs::HookManager*>(this);

    m_Context.registerProcess(::GetCurrentProcessId());
    spdlog::get("usvfs")->info(
        "Process registered in shared process list : {}",
        ::GetCurrentProcessId());

    winapi::ex::OSVersion version = winapi::ex::getOSVersion();
    spdlog::get("usvfs")->info(
        "Windows version {}.{}.{} sp {} platform {} ({})", version.major,
        version.minor, version.build, version.servicpack, version.platformid,
        shared::string_cast<std::string>(winapi::ex::wide::getWindowsBuildLab(true))
            .c_str());

    initHooks();

    if (params.debugMode) {
      MessageBoxA(nullptr, "Hooks initialized", "Pause", MB_OK);
    }
  }

  ~HookManagerProxy()
  {
    spdlog::get("hooks")->debug("end hook of process {}", GetCurrentProcessId());
    removeHooks();
    m_Context.unregisterCurrentProcess();
  }

  LPVOID detour(const char* functionName)
  {
    auto iter = m_Hooks.find(functionName);
    if (iter != m_Hooks.end()) {
      return GetDetour(iter->second);
    }
    return nullptr;
  }

  void removeHook(const std::string& functionName)
  {
    auto iter = m_Hooks.find(functionName);
    if (iter != m_Hooks.end()) {
      try {
        RemoveHook(iter->second);
        m_Hooks.erase(iter);
        spdlog::get("usvfs")->info("removed hook for {}", functionName);
      } catch (const std::exception& e) {
        spdlog::get("usvfs")->critical(
            "failed to remove hook of {}: {}", functionName, e.what());
      }
    } else {
      spdlog::get("usvfs")->info("{} wasn't hooked", functionName);
    }
  }

  void logStubInt(LPVOID address)
  {
    if (m_Stubs.find(address) != m_Stubs.end()) {
      spdlog::get("hooks")->warn("{0} called", m_Stubs[address]);
    } else {
      spdlog::get("hooks")->warn("unknown function at {0} called", address);
    }
  }

  static void logStub(LPVOID address)
  {
    try {
      reinterpret_cast<HookManagerProxy*>(&usvfs::HookManager::instance())
          ->logStubInt(address);
    } catch (const std::exception& e) {
      spdlog::get("hooks")->debug(
          "function at {0} called after shutdown: {1}", address, e.what());
    }
  }

  void installHook(HMODULE module1, HMODULE module2,
                   const std::string& functionName, LPVOID hook,
                   LPVOID* fillFuncAddr = nullptr)
  {
    BOOST_ASSERT(hook != nullptr);
    HOOKHANDLE handle = INVALID_HOOK;
    HookError err = ERR_NONE;
    LPVOID funcAddr = nullptr;
    HMODULE usedModule = nullptr;
    if (module1 != nullptr) {
      funcAddr = MyGetProcAddress(module1, functionName.c_str());
      if (funcAddr != nullptr) {
        handle = InstallHook(funcAddr, hook, &err);
      }
      if (handle != INVALID_HOOK) {
        usedModule = module1;
      }
    }

    if ((handle == INVALID_HOOK) && (module2 != nullptr)) {
      funcAddr = MyGetProcAddress(module2, functionName.c_str());
      if (funcAddr != nullptr) {
        handle = InstallHook(funcAddr, hook, &err);
      }
      if (handle != INVALID_HOOK) {
        usedModule = module2;
      }
    }

    if (fillFuncAddr != nullptr) {
      *fillFuncAddr = funcAddr;
    }

    if (handle == INVALID_HOOK) {
      spdlog::get("usvfs")->error(
          "failed to hook {0}: {1}", functionName, GetErrorString(err));
    } else {
      m_Stubs.insert(make_pair(funcAddr, functionName));
      m_Hooks.insert(make_pair(std::string(functionName), handle));
      spdlog::get("usvfs")->info(
          "hooked {0} ({1}) in {2} type {3}", functionName, funcAddr,
          winapi::ansi::getModuleFileName(usedModule), GetHookType(handle));
    }
  }

  void installStub(HMODULE module1, HMODULE module2,
                   const std::string& functionName)
  {
    HOOKHANDLE handle = INVALID_HOOK;
    HookError err = ERR_NONE;
    LPVOID funcAddr = nullptr;
    HMODULE usedModule = nullptr;
    if (module1 != nullptr) {
      funcAddr = MyGetProcAddress(module1, functionName.c_str());
      if (funcAddr != nullptr) {
        handle = InstallStub(funcAddr, logStub, &err);
      } else {
        spdlog::get("usvfs")->debug(
            "{} doesn't contain {}", winapi::ansi::getModuleFileName(module1),
            functionName);
      }
      if (handle != INVALID_HOOK) {
        usedModule = module1;
      }
    }

    if ((handle == INVALID_HOOK) && (module2 != nullptr)) {
      funcAddr = MyGetProcAddress(module2, functionName.c_str());
      if (funcAddr != nullptr) {
        handle = InstallStub(funcAddr, logStub, &err);
      } else {
        spdlog::get("usvfs")->debug(
            "{} doesn't contain {}", winapi::ansi::getModuleFileName(module2),
            functionName);
      }
      if (handle != INVALID_HOOK) {
        usedModule = module2;
      }
    }

    if (handle == INVALID_HOOK) {
      spdlog::get("usvfs")->error(
          "failed to stub {0}: {1}", functionName, GetErrorString(err));
    } else {
      m_Stubs.insert(make_pair(funcAddr, functionName));
      m_Hooks.insert(make_pair(std::string(functionName), handle));
      spdlog::get("usvfs")->info(
          "stubbed {0} ({1}) in {2} type {3}", functionName, funcAddr,
          winapi::ansi::getModuleFileName(usedModule), GetHookType(handle));
    }
  }

  void initHooks()
  {
    TrampolinePool::initialize();

    HookLib::TrampolinePool::instance().setBlock(true);

    HMODULE k32Mod = GetModuleHandleA("kernel32.dll");
    spdlog::get("usvfs")->debug(
        "kernel32.dll at {0:x}", reinterpret_cast<uintptr_t>(k32Mod));
    HMODULE kbaseMod = GetModuleHandleA("kernelbase.dll");
    spdlog::get("usvfs")->debug(
        "kernelbase.dll at {0:x}", reinterpret_cast<uintptr_t>(kbaseMod));

    installHook(kbaseMod, k32Mod, "GetFileAttributesExW",
                hook_GetFileAttributesExW);
    installHook(kbaseMod, k32Mod, "GetFileAttributesW", hook_GetFileAttributesW);
    installHook(kbaseMod, k32Mod, "SetFileAttributesW", hook_SetFileAttributesW);

    installHook(kbaseMod, k32Mod, "CreateDirectoryW", hook_CreateDirectoryW);
    installHook(kbaseMod, k32Mod, "RemoveDirectoryW", hook_RemoveDirectoryW);
    installHook(kbaseMod, k32Mod, "DeleteFileW", hook_DeleteFileW);
    installHook(kbaseMod, k32Mod, "GetCurrentDirectoryA",
                hook_GetCurrentDirectoryA);
    installHook(kbaseMod, k32Mod, "GetCurrentDirectoryW",
                hook_GetCurrentDirectoryW);
    installHook(kbaseMod, k32Mod, "SetCurrentDirectoryA",
                hook_SetCurrentDirectoryA);
    installHook(kbaseMod, k32Mod, "SetCurrentDirectoryW",
                hook_SetCurrentDirectoryW);

    installHook(kbaseMod, k32Mod, "ExitProcess", hook_ExitProcess);

    installHook(kbaseMod, k32Mod, "CreateProcessInternalW",
                hook_CreateProcessInternalW,
                reinterpret_cast<LPVOID*>(&CreateProcessInternalW));

    installHook(kbaseMod, k32Mod, "MoveFileA", hook_MoveFileA);
    installHook(kbaseMod, k32Mod, "MoveFileW", hook_MoveFileW);
    installHook(kbaseMod, k32Mod, "MoveFileExA", hook_MoveFileExA);
    installHook(kbaseMod, k32Mod, "MoveFileExW", hook_MoveFileExW);
    installHook(kbaseMod, k32Mod, "MoveFileWithProgressA",
                hook_MoveFileWithProgressA);
    installHook(kbaseMod, k32Mod, "MoveFileWithProgressW",
                hook_MoveFileWithProgressW);

    installHook(kbaseMod, k32Mod, "CopyFileExW", hook_CopyFileExW);
    if (IsWindows8OrGreater()) {
      installHook(kbaseMod, k32Mod, "CopyFile2", hook_CopyFile2,
                  reinterpret_cast<LPVOID*>(&usvfs::CopyFile2));
    }

    installHook(kbaseMod, k32Mod, "GetPrivateProfileStringA",
                hook_GetPrivateProfileStringA);
    installHook(kbaseMod, k32Mod, "GetPrivateProfileStringW",
                hook_GetPrivateProfileStringW);
    installHook(kbaseMod, k32Mod, "GetPrivateProfileSectionA",
                hook_GetPrivateProfileSectionA);
    installHook(kbaseMod, k32Mod, "GetPrivateProfileSectionW",
                hook_GetPrivateProfileSectionW);
    installHook(kbaseMod, k32Mod, "WritePrivateProfileStringA",
                hook_WritePrivateProfileStringA);
    installHook(kbaseMod, k32Mod, "WritePrivateProfileStringW",
                hook_WritePrivateProfileStringW);

    installHook(kbaseMod, k32Mod, "GetFullPathNameA", hook_GetFullPathNameA);
    installHook(kbaseMod, k32Mod, "GetFullPathNameW", hook_GetFullPathNameW);

    installHook(kbaseMod, k32Mod, "FindFirstFileExW", hook_FindFirstFileExW);

    HMODULE ntdllMod = GetModuleHandleA("ntdll.dll");
    spdlog::get("usvfs")->debug(
        "ntdll.dll at {0:x}", reinterpret_cast<uintptr_t>(ntdllMod));
    installHook(ntdllMod, nullptr, "NtQueryFullAttributesFile",
                hook_NtQueryFullAttributesFile);
    installHook(ntdllMod, nullptr, "NtQueryAttributesFile",
                hook_NtQueryAttributesFile);
    installHook(ntdllMod, nullptr, "NtQueryDirectoryFile",
                hook_NtQueryDirectoryFile);
    installHook(ntdllMod, nullptr, "NtQueryDirectoryFileEx",
                hook_NtQueryDirectoryFileEx);
    installHook(ntdllMod, nullptr, "NtOpenFile", hook_NtOpenFile);
    installHook(ntdllMod, nullptr, "NtCreateFile", hook_NtCreateFile);
    installHook(ntdllMod, nullptr, "NtClose", hook_NtClose);
    installHook(ntdllMod, nullptr, "NtTerminateProcess",
                hook_NtTerminateProcess);

    installHook(kbaseMod, k32Mod, "LoadLibraryExW", hook_LoadLibraryExW);
    installHook(kbaseMod, k32Mod, "GetModuleFileNameW", hook_GetModuleFileNameW);

    spdlog::get("usvfs")->debug("hooks installed");
    HookLib::TrampolinePool::instance().setBlock(false);
  }

  void removeHooks()
  {
    while (m_Hooks.size() > 0) {
      auto iter = m_Hooks.begin();
      try {
        RemoveHook(iter->second);
        spdlog::get("usvfs")->debug("removed hook {}", iter->first);
      } catch (const std::exception& e) {
        spdlog::get("usvfs")->critical("failed to remove hook: {}", e.what());
      }

      m_Hooks.erase(iter);
    }
  }

  std::map<std::string, HookLib::HOOKHANDLE> m_Hooks;
  std::map<LPVOID, std::string> m_Stubs;
  usvfs::HookContext m_Context;
};

static_assert(sizeof(HookManagerProxy) == sizeof(usvfs::HookManager),
              "HookManager proxy size drifted");
static_assert(alignof(HookManagerProxy) == alignof(usvfs::HookManager),
              "HookManager proxy alignment drifted");

inline HookContextProxy* asHookContext(usvfs::HookContext* self)
{
  return reinterpret_cast<HookContextProxy*>(self);
}

inline const HookContextProxy* asHookContext(const usvfs::HookContext* self)
{
  return reinterpret_cast<const HookContextProxy*>(self);
}

inline HookManagerProxy* asHookManager(usvfs::HookManager* self)
{
  return reinterpret_cast<HookManagerProxy*>(self);
}

} // namespace

extern "C" usvfs::HookContext* usvfsAsmHookContextCtorImpl(
    usvfs::HookContext* self, const usvfsParameters& params, HMODULE module)
{
  new (self) HookContextProxy(params, module);
  return self;
}

extern "C" void usvfsAsmHookContextDtorImpl(usvfs::HookContext* self)
{
  asHookContext(self)->~HookContextProxy();
}

extern "C" void usvfsAsmHookContextRemoveImpl(const char* instanceName)
{
  bi::shared_memory_object::remove(instanceName);
}

usvfs::HookContext::ConstPtr usvfs::HookContext::readAccess(const char*)
{
  BOOST_ASSERT(s_Instance != nullptr);

  s_Instance->m_Mutex.wait(200);
  return ConstPtr(s_Instance, unlockShared);
}

usvfs::HookContext::Ptr usvfs::HookContext::writeAccess(const char*)
{
  BOOST_ASSERT(s_Instance != nullptr);

  s_Instance->m_Mutex.wait(200);
  return Ptr(s_Instance, unlock);
}

usvfsParameters usvfs::HookContext::callParameters() const
{
  updateParameters();
  return m_Parameters->makeLocal();
}

std::wstring usvfs::HookContext::dllPath() const
{
  std::wstring path = winapi::wide::getModuleFileName(m_DLLModule);
  return bf::path(path).parent_path().make_preferred().wstring();
}

extern "C" void usvfsAsmHookContextRegisterProcessImpl(
    usvfs::HookContext* self, DWORD pid)
{
  self->m_Parameters->registerProcess(pid);
}

void usvfs::HookContext::setDebugParameters(
    LogLevel level, CrashDumpsType dumpType, const std::string& dumpPath,
    std::chrono::milliseconds delayProcess)
{
  m_Parameters->setDebugParameters(level, dumpType, dumpPath, delayProcess);
}

void usvfs::HookContext::updateParameters() const
{
  m_Parameters->setSHMNames(m_Tree.shmName(), m_InverseTree.shmName());
}

extern "C" void usvfsAsmHookContextUnregisterCurrentProcessImpl(
    usvfs::HookContext* self)
{
  self->m_Parameters->unregisterProcess(::GetCurrentProcessId());
}

std::vector<DWORD> usvfs::HookContext::registeredProcesses() const
{
  return m_Parameters->registeredProcesses();
}

extern "C" void usvfsAsmHookContextBlacklistExecutableImpl(
    usvfs::HookContext* self, const std::wstring& wexe)
{
  const auto exe =
      shared::string_cast<std::string>(wexe, shared::CodePage::UTF8);

  spdlog::get("usvfs")->debug("blacklisting '{}'", exe);
  self->m_Parameters->blacklistExecutable(exe);
}

extern "C" void usvfsAsmHookContextClearExecutableBlacklistImpl(
    usvfs::HookContext* self)
{
  spdlog::get("usvfs")->debug("clearing blacklist");
  self->m_Parameters->clearExecutableBlacklist();
}

extern "C" BOOL usvfsAsmHookContextExecutableBlacklistedImpl(
    const usvfs::HookContext* self, LPCWSTR wapp, LPCWSTR wcmd)
{
  std::string app;
  if (wapp) {
    app = ush::string_cast<std::string>(wapp, ush::CodePage::UTF8);
  }

  std::string cmd;
  if (wcmd) {
    cmd = ush::string_cast<std::string>(wcmd, ush::CodePage::UTF8);
  }

  return self->m_Parameters->executableBlacklisted(app, cmd);
}

extern "C" void usvfsAsmHookContextForceLoadLibraryImpl(
    usvfs::HookContext* self, const std::wstring& wprocess,
    const std::wstring& wpath)
{
  const auto process = shared::string_cast<std::string>(
      wprocess, shared::CodePage::UTF8);

  const auto path =
      shared::string_cast<std::string>(wpath, shared::CodePage::UTF8);

  spdlog::get("usvfs")->debug(
      "adding forced library '{}' for process '{}'", path, process);

  self->m_Parameters->addForcedLibrary(process, path);
}

extern "C" void usvfsAsmHookContextClearLibraryForceLoadsImpl(
    usvfs::HookContext* self)
{
  spdlog::get("usvfs")->debug("clearing forced libraries");
  self->m_Parameters->clearForcedLibraries();
}

#ifndef USVFS_TARGET_V244
void usvfs::HookContext::addSkipFileSuffix(const std::wstring& fileSuffix)
{
  m_Parameters->addSkipFileSuffix(shared::string_cast<std::string>(fileSuffix, shared::CodePage::UTF8));
}

void usvfs::HookContext::clearSkipFileSuffixes()
{
  m_Parameters->clearSkipFileSuffixes();
}

void usvfs::HookContext::addSkipDirectory(const std::wstring& directory)
{
  m_Parameters->addSkipDirectory(shared::string_cast<std::string>(directory, shared::CodePage::UTF8));
}

void usvfs::HookContext::clearSkipDirectories()
{
  m_Parameters->clearSkipDirectories();
}
#endif

std::vector<std::wstring> usvfs::HookContext::librariesToForceLoad(
    const std::wstring& processName)
{
  const auto v = m_Parameters->forcedLibraries(
      shared::string_cast<std::string>(processName, shared::CodePage::UTF8));

  std::vector<std::wstring> wv;
  for (const auto& s : v) {
    wv.push_back(shared::string_cast<std::wstring>(s, shared::CodePage::UTF8));
  }

  return wv;
}

extern "C" void usvfsAsmHookContextRegisterDelayedImpl(
    usvfs::HookContext* self, std::future<int> delayed)
{
  self->m_Futures.push_back(std::move(delayed));
}

std::vector<std::future<int>>& usvfs::HookContext::delayed()
{
  return m_Futures;
}

void usvfs::HookContext::unlock(HookContext* self)
{
  self->m_Mutex.signal();
}

void usvfs::HookContext::unlockShared(const HookContext* self)
{
  self->m_Mutex.signal();
}

extern "C" void usvfsAsmHookContextUnlockImpl(usvfs::HookContext* self)
{
  self->m_Mutex.signal();
}

extern "C" void usvfsAsmHookContextUnlockSharedImpl(
    const usvfs::HookContext* self)
{
  self->m_Mutex.signal();
}

extern "C" SharedParameters* usvfsAsmHookContextRetrieveParametersImpl(
    usvfs::HookContext* self, const usvfsParameters& params)
{
  return asHookContext(self)->retrieveParameters(params);
}

extern "C" usvfs::HookManager* usvfsAsmHookManagerCtorImpl(
    usvfs::HookManager* self, const usvfsParameters& params, HMODULE module)
{
  new (self) HookManagerProxy(params, module);
  return self;
}

extern "C" void usvfsAsmHookManagerDtorImpl(usvfs::HookManager* self)
{
  asHookManager(self)->~HookManagerProxy();
}

usvfs::HookManager& usvfs::HookManager::instance()
{
  if (s_Instance == nullptr) {
    throw std::runtime_error("singleton not instantiated");
  }

  return *s_Instance;
}

extern "C" LPVOID usvfsAsmHookManagerDetourImpl(
    usvfs::HookManager* self, const char* functionName)
{
  return asHookManager(self)->detour(functionName);
}

extern "C" void usvfsAsmHookManagerRemoveHookImpl(
    usvfs::HookManager* self, const std::string& functionName)
{
  asHookManager(self)->removeHook(functionName);
}

extern "C" void usvfsAsmHookManagerLogStubIntImpl(
    usvfs::HookManager* self, LPVOID address)
{
  asHookManager(self)->logStubInt(address);
}

extern "C" void usvfsAsmHookManagerLogStubImpl(LPVOID address)
{
  HookManagerProxy::logStub(address);
}

extern "C" void usvfsAsmHookManagerInstallHookImpl(
    usvfs::HookManager* self, HMODULE module1, HMODULE module2,
    const std::string& functionName, LPVOID hook, LPVOID* fillFuncAddr)
{
  asHookManager(self)->installHook(module1, module2, functionName, hook,
                                   fillFuncAddr);
}

extern "C" void usvfsAsmHookManagerInstallStubImpl(
    usvfs::HookManager* self, HMODULE module1, HMODULE module2,
    const std::string& functionName)
{
  asHookManager(self)->installStub(module1, module2, functionName);
}

extern "C" void usvfsAsmHookManagerInitHooksImpl(usvfs::HookManager* self)
{
  asHookManager(self)->initHooks();
}
