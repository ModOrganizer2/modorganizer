#include "envmetrics.h"
#include "env.h"
#include <QScreen>
#include <Windows.h>
#include <log.h>
#include <shellscalingapi.h>
#include <utility.h>

namespace env
{

using namespace MOBase;

// fallback for windows 7
//
int getDesktopDpi()
{
  // desktop DC
  DesktopDCPtr dc(GetDC(0));

  if (!dc) {
    const auto e = GetLastError();
    log::error("can't get desktop DC, {}", formatSystemMessage(e));
    return 0;
  }

  return GetDeviceCaps(dc.get(), LOGPIXELSX);
}

// finds a monitor by device name; there's no real good way to do that except
// by enumerating all the monitors and checking their name
//
HMONITOR findMonitor(const QString& name)
{
  // passed to the enumeration callback
  struct Data
  {
    QString name;
    HMONITOR hm;
  };

  Data data = {name, 0};

  // callback
  auto callback = [](HMONITOR hm, HDC, RECT*, LPARAM lp) {
    auto& data = *reinterpret_cast<Data*>(lp);

    MONITORINFOEX mi = {};
    mi.cbSize        = sizeof(mi);

    // monitor info will include the name
    if (!GetMonitorInfoW(hm, &mi)) {
      const auto e = GetLastError();
      log::error("GetMonitorInfo() failed for '{}', {}", data.name,
                 formatSystemMessage(e));

      // error for this monitor, but continue
      return TRUE;
    }

    if (QString::fromWCharArray(mi.szDevice) == data.name) {
      // found, stop
      data.hm = hm;
      return FALSE;
    }

    // not found, continue to the next monitor
    return TRUE;
  };

  // for each monitor
  EnumDisplayMonitors(0, nullptr, callback, reinterpret_cast<LPARAM>(&data));

  return data.hm;
}

// returns the dpi for the given monitor; for systems that do not support
// per-monitor dpi (such as windows 7), this is the desktop dpi
//
int getDpi(const QString& monitorDevice)
{
  using GetDpiForMonitorFunction =
      HRESULT WINAPI(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*);

  static LibraryPtr shcore;
  static GetDpiForMonitorFunction* GetDpiForMonitor = nullptr;
  static bool checked                               = false;

  if (!checked) {
    // try to find GetDpiForMonitor() from shcored.dll

    shcore.reset(LoadLibraryW(L"Shcore.dll"));

    if (shcore) {
      // windows 8.1+ only
      GetDpiForMonitor = reinterpret_cast<GetDpiForMonitorFunction*>(
          GetProcAddress(shcore.get(), "GetDpiForMonitor"));
    }

    checked = true;
  }

  if (!GetDpiForMonitor) {
    // get the desktop dpi instead
    return getDesktopDpi();
  }

  // there's no way to get an HMONITOR from a device name, so all monitors
  // will have to be enumerated and their name checked
  HMONITOR hm = findMonitor(monitorDevice);
  if (!hm) {
    log::error("can't get dpi for monitor '{}', not found", monitorDevice);
    return 0;
  }

  UINT dpiX = 0, dpiY = 0;
  const auto r = GetDpiForMonitor(hm, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);

  if (FAILED(r)) {
    log::error("GetDpiForMonitor() failed for '{}', {}", monitorDevice,
               formatSystemMessage(r));

    return 0;
  }

  // dpiX and dpiY are always identical, as per the documentation
  return dpiX;
}

Display::Display(QString adapter, QString monitorDevice, bool primary)
    : m_adapter(std::move(adapter)), m_monitorDevice(std::move(monitorDevice)),
      m_primary(primary), m_resX(0), m_resY(0), m_dpi(0), m_refreshRate(0)
{
  getSettings();
  m_dpi = getDpi(m_monitorDevice);
}

const QString& Display::adapter() const
{
  return m_adapter;
}

const QString& Display::monitorDevice() const
{
  return m_monitorDevice;
}

bool Display::primary()
{
  return m_primary;
}

int Display::resX() const
{
  return m_resX;
}

int Display::resY() const
{
  return m_resY;
}

int Display::dpi()
{
  return m_dpi;
}

int Display::refreshRate() const
{
  return m_refreshRate;
}

QString Display::toString() const
{
  return QString("%1*%2 %3hz dpi=%4 on %5%6")
      .arg(m_resX)
      .arg(m_resY)
      .arg(m_refreshRate)
      .arg(m_dpi)
      .arg(m_adapter)
      .arg(m_primary ? " (primary)" : "");
}

void Display::getSettings()
{
  DEVMODEW dm = {};
  dm.dmSize   = sizeof(dm);

  const auto wsDevice = m_monitorDevice.toStdWString();

  if (!EnumDisplaySettingsW(wsDevice.c_str(), ENUM_CURRENT_SETTINGS, &dm)) {
    log::error("EnumDisplaySettings() failed for '{}'", m_monitorDevice);
    return;
  }

  // all these fields should be available

  if (dm.dmFields & DM_DISPLAYFREQUENCY) {
    m_refreshRate = dm.dmDisplayFrequency;
  }

  if (dm.dmFields & DM_PELSWIDTH) {
    m_resX = dm.dmPelsWidth;
  }

  if (dm.dmFields & DM_PELSHEIGHT) {
    m_resY = dm.dmPelsHeight;
  }
}

Metrics::Metrics()
{
  getDisplays();
}

const std::vector<Display>& Metrics::displays() const
{
  return m_displays;
}

QRect Metrics::desktopGeometry() const
{
  QRect r;

  for (auto* s : QGuiApplication::screens()) {
    r = r.united(s->geometry());
  }

  return r;
}

void Metrics::getDisplays()
{
  // don't bother if it goes over 100
  for (int i = 0; i < 100; ++i) {
    DISPLAY_DEVICEW device = {};
    device.cb              = sizeof(device);

    if (!EnumDisplayDevicesW(nullptr, i, &device, 0)) {
      // no more
      break;
    }

    // EnumDisplayDevices() seems to be returning a lot of devices that are
    // not actually monitors, but those don't have the
    // DISPLAY_DEVICE_ATTACHED_TO_DESKTOP bit set
    if ((device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) == 0) {
      continue;
    }

    m_displays.emplace_back(QString::fromWCharArray(device.DeviceString),
                            QString::fromWCharArray(device.DeviceName),
                            (device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE));
  }
}

}  // namespace env
