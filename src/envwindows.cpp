#include "envwindows.h"
#include "env.h"
#include "envmodule.h"
#include <log.h>
#include <utility.h>

namespace env
{

using namespace MOBase;

WindowsInfo::WindowsInfo()
{
  // loading ntdll.dll, the functions will be found with GetProcAddress()
  LibraryPtr ntdll(LoadLibraryW(L"ntdll.dll"));

  if (!ntdll) {
    log::error("failed to load ntdll.dll while getting version");
    return;
  } else {
    m_reported = getReportedVersion(ntdll.get());
    m_real     = getRealVersion(ntdll.get());
  }

  m_release  = getRelease();
  m_elevated = getElevated();
}

bool WindowsInfo::compatibilityMode() const
{
  if (m_real == Version()) {
    // don't know the real version, can't guess compatibility mode
    return false;
  }

  return (m_real != m_reported);
}

const WindowsInfo::Version& WindowsInfo::reportedVersion() const
{
  return m_reported;
}

const WindowsInfo::Version& WindowsInfo::realVersion() const
{
  return m_real;
}

const WindowsInfo::Release& WindowsInfo::release() const
{
  return m_release;
}

std::optional<bool> WindowsInfo::isElevated() const
{
  return m_elevated;
}

QString WindowsInfo::toString() const
{
  QStringList sl;

  const QString reported = m_reported.toString();
  const QString real     = m_real.toString();

  // version
  sl.push_back("version " + reported);

  // real version if different
  if (compatibilityMode()) {
    sl.push_back("real version " + real);
  }

  // build.UBR, such as 17763.557
  if (m_release.UBR != 0) {
    DWORD build = 0;

    if (compatibilityMode()) {
      build = m_real.build;
    } else {
      build = m_reported.build;
    }

    sl.push_back(QString("%1.%2").arg(build).arg(m_release.UBR));
  }

  // release ID
  if (!m_release.ID.isEmpty()) {
    sl.push_back("release " + m_release.ID);
  }

  // buildlab string
  if (!m_release.buildLab.isEmpty()) {
    sl.push_back(m_release.buildLab);
  }

  // elevated
  QString elevated = "?";
  if (m_elevated.has_value()) {
    elevated = (*m_elevated ? "yes" : "no");
  }

  sl.push_back("elevated: " + elevated);

  return sl.join(", ");
}

WindowsInfo::Version WindowsInfo::getReportedVersion(HINSTANCE ntdll) const
{
  // windows has been deprecating pretty much all the functions having to do
  // with getting version information because apparently, people keep misusing
  // them for feature detection
  //
  // there's still RtlGetVersion() though

  using RtlGetVersionType = NTSTATUS(NTAPI)(PRTL_OSVERSIONINFOW);

  auto* RtlGetVersion =
      reinterpret_cast<RtlGetVersionType*>(GetProcAddress(ntdll, "RtlGetVersion"));

  if (!RtlGetVersion) {
    log::error("RtlGetVersion() not found in ntdll.dll");
    return {};
  }

  OSVERSIONINFOEX vi     = {};
  vi.dwOSVersionInfoSize = sizeof(vi);

  // this apparently never fails
  RtlGetVersion((RTL_OSVERSIONINFOW*)&vi);

  return {vi.dwMajorVersion, vi.dwMinorVersion, vi.dwBuildNumber};
}

WindowsInfo::Version WindowsInfo::getRealVersion(HINSTANCE ntdll) const
{
  // getting the actual windows version is more difficult because all the
  // functions are lying when running in compatibility mode
  //
  // RtlGetNtVersionNumbers() is an undocumented function that seems to work
  // fine, but it might not in the future

  using RtlGetNtVersionNumbersType = void(NTAPI)(DWORD*, DWORD*, DWORD*);

  auto* RtlGetNtVersionNumbers = reinterpret_cast<RtlGetNtVersionNumbersType*>(
      GetProcAddress(ntdll, "RtlGetNtVersionNumbers"));

  if (!RtlGetNtVersionNumbers) {
    log::error("RtlGetNtVersionNumbers not found in ntdll.dll");
    return {};
  }

  DWORD major = 0, minor = 0, build = 0;
  RtlGetNtVersionNumbers(&major, &minor, &build);

  // for whatever reason, the build number has 0xf0000000 set
  build = 0x0fffffff & build;

  return {major, minor, build};
}

WindowsInfo::Release WindowsInfo::getRelease() const
{
  // there are several interesting items in the registry, but most of them
  // are undocumented, not always available, and localizable
  //
  // most of them are used to provide as much information as possible in case
  // any of the other versions fail to work

  QSettings settings(
      R"(HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion)",
      QSettings::NativeFormat);

  Release r;

  // buildlab seems to be an internal name from the build system
  r.buildLab = settings.value("BuildLabEx", "").toString();
  if (r.buildLab.isEmpty()) {
    r.buildLab = settings.value("BuildLab", "").toString();
    if (r.buildLab.isEmpty()) {
      r.buildLab = settings.value("BuildBranch", "").toString();
    }
  }

  // release ID, such as 1803
  r.ID = settings.value("DisplayVersion", "").toString();
  if (r.ID.isEmpty()) {
    r.ID = settings.value("ReleaseId", "").toString();
  }

  // some other build number, shown in winver.exe
  r.UBR = settings.value("UBR", 0).toUInt();

  return r;
}

std::optional<bool> WindowsInfo::getElevated() const
{
  HandlePtr token;

  {
    HANDLE rawToken = 0;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &rawToken)) {
      const auto e = GetLastError();

      log::error("while trying to check if process is elevated, "
                 "OpenProcessToken() failed: {}",
                 formatSystemMessage(e));

      return {};
    }

    token.reset(rawToken);
  }

  TOKEN_ELEVATION e = {};
  DWORD size        = sizeof(TOKEN_ELEVATION);

  if (!GetTokenInformation(token.get(), TokenElevation, &e, sizeof(e), &size)) {
    const auto e = GetLastError();

    log::error("while trying to check if process is elevated, "
               "GetTokenInformation() failed: {}",
               formatSystemMessage(e));

    return {};
  }

  return (e.TokenIsElevated != 0);
}

}  // namespace env
