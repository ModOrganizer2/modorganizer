#ifndef ENV_WINDOWS_H
#define ENV_WINDOWS_H

#include <QString>
#include <optional>

namespace env
{

// a variety of information on windows
//
class WindowsInfo
{
public:
  struct Version
  {
    DWORD major = 0, minor = 0, build = 0;

    QString toString() const
    {
      return QString("%1.%2.%3").arg(major).arg(minor).arg(build);
    }

    friend bool operator==(const Version& a, const Version& b)
    {
      return a.major == b.major && a.minor == b.minor && a.build == b.build;
    }

    friend bool operator!=(const Version& a, const Version& b) { return !(a == b); }
  };

  struct Release
  {
    // the BuildLab entry from the registry, may be empty
    QString buildLab;

    // release ID such as 1809, may be empty
    QString ID;

    // some sub-build number, undocumented, may be empty
    DWORD UBR;

    Release() : UBR(0) {}
  };

  WindowsInfo();

  // tries to guess whether this process is running in compatibility mode
  //
  bool compatibilityMode() const;

  // returns the Windows version, may not correspond to the actual version
  // if the process is running in compatibility mode
  //
  const Version& reportedVersion() const;

  // tries to guess the real Windows version that's running, can be empty
  //
  const Version& realVersion() const;

  // various information about the current release
  //
  const Release& release() const;

  // whether this process is running as administrator, may be empty if the
  // information is not available
  std::optional<bool> isElevated() const;

  // returns a string with all the above information on one line
  //
  QString toString() const;

private:
  Version m_reported, m_real;
  Release m_release;
  std::optional<bool> m_elevated;

  // uses RtlGetVersion() to get the version number as reported by Windows
  //
  Version getReportedVersion(HINSTANCE ntdll) const;

  // uses RtlGetNtVersionNumbers() to get the real version number
  //
  Version getRealVersion(HINSTANCE ntdll) const;

  // gets various information from the registry
  //
  Release getRelease() const;

  // gets whether the process is elevated
  //
  std::optional<bool> getElevated() const;
};

}  // namespace env

#endif  // ENV_WINDOWS_H
