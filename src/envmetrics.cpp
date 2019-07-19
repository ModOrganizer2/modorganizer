#include "envmetrics.h"
#include "env.h"
#include <Windows.h>
#include <shellscalingapi.h>
#include <log.h>
#include <utility.h>

namespace env
{

using namespace MOBase;

class DisplayEnumerator
{
public:
  DisplayEnumerator()
    : m_GetDpiForMonitor(nullptr)
  {
    m_shcore.reset(LoadLibraryW(L"Shcore.dll"));

    if (m_shcore) {
      // windows 8.1+ only
      m_GetDpiForMonitor = reinterpret_cast<GetDpiForMonitorFunction*>(
        GetProcAddress(m_shcore.get(), "GetDpiForMonitor"));
    }

    // gets all monitors and the device they're running on
    getDisplayDevices();
  }

  std::vector<Metrics::Display>&& displays() &&
  {
    return std::move(m_displays);
  }

  const std::vector<Metrics::Display>& displays() const &
  {
    return m_displays;
  }

private:
  using GetDpiForMonitorFunction =
    HRESULT WINAPI (HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*);

  std::unique_ptr<HINSTANCE, LibraryFreer> m_shcore;
  GetDpiForMonitorFunction* m_GetDpiForMonitor;
  std::vector<Metrics::Display> m_displays;

  void getDisplayDevices()
  {
    // don't bother if it goes over 100
    for (int i=0; i<100; ++i) {
      DISPLAY_DEVICEW device = {};
      device.cb = sizeof(device);

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

      m_displays.push_back(createDisplay(device));
    }
  }

  Metrics::Display createDisplay(const DISPLAY_DEVICEW& device)
  {
    Metrics::Display d;

    d.adapter = QString::fromWCharArray(device.DeviceString);
    d.monitor = QString::fromWCharArray(device.DeviceName);
    d.primary = (device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE);

    getDisplaySettings(device.DeviceName, d);
    getDpi(d);

    return d;
  }

  void getDisplaySettings(const wchar_t* monitorName, Metrics::Display& d)
  {
    DEVMODEW dm = {};
    dm.dmSize = sizeof(dm);

    if (!EnumDisplaySettingsW(monitorName, ENUM_CURRENT_SETTINGS, &dm)) {
      log::error("EnumDisplaySettings() failed for '{}'", d.monitor);
      return;
    }

    // all these fields should be available

    if (dm.dmFields & DM_DISPLAYFREQUENCY) {
      d.refreshRate = dm.dmDisplayFrequency;
    }

    if (dm.dmFields & DM_PELSWIDTH) {
      d.resX = dm.dmPelsWidth;
    }

    if (dm.dmFields & DM_PELSHEIGHT) {
      d.resY = dm.dmPelsHeight;
    }
  }

  void getDpi(Metrics::Display& d)
  {
    if (!m_GetDpiForMonitor) {
      // this happens on windows 7, get the desktop dpi instead
      getDesktopDpi(d);
      return;
    }

    // there's no way to get an HMONITOR from a device name, so all monitors
    // will have to be enumerated and their name checked
    HMONITOR hm = findMonitor(d.monitor);
    if (!hm) {
      log::error("can't get dpi for monitor '{}', not found", d.monitor);
      return;
    }

    UINT dpiX=0, dpiY=0;
    const auto r = m_GetDpiForMonitor(hm, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);

    if (FAILED(r)) {
      log::error(
        "GetDpiForMonitor() failed for '{}', {}",
        d.monitor, formatSystemMessageQ(r));

      return;
    }

    // dpiX and dpiY are always identical, as per the documentation
    d.dpi = dpiX;
  }

  void getDesktopDpi(Metrics::Display& d)
  {
    // desktop dc
    HDC dc = GetDC(0);

    if (!dc) {
      const auto e = GetLastError();
      log::error("can't get desktop DC, {}", formatSystemMessageQ(e));
      return;
    }

    d.dpi = GetDeviceCaps(dc, LOGPIXELSX);

    ReleaseDC(0, dc);
  }

  HMONITOR findMonitor(const QString& name)
  {
    // passed to the enumeration callback
    struct Data
    {
      DisplayEnumerator* self;
      QString name;
      HMONITOR hm;
    };

    Data data = {this, name, 0};

    // for each monitor
    EnumDisplayMonitors(0, nullptr, [](HMONITOR hm, HDC, RECT*, LPARAM lp) {
      auto& data = *reinterpret_cast<Data*>(lp);

      MONITORINFOEX mi = {};
      mi.cbSize = sizeof(mi);

      // monitor info will include the name
      if (!GetMonitorInfoW(hm, &mi)) {
        const auto e = GetLastError();
        log::error(
          "GetMonitorInfo() failed for '{}', {}",
          data.name, formatSystemMessageQ(e));

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
      }, reinterpret_cast<LPARAM>(&data));

    return data.hm;
  }
};


Metrics::Metrics()
{
  m_displays = DisplayEnumerator().displays();
}

const std::vector<Metrics::Display>& Metrics::displays() const
{
  return m_displays;
}

QString Metrics::Display::toString() const
{
  return QString("%1*%2 %3hz dpi=%4 on %5%6")
    .arg(resX)
    .arg(resY)
    .arg(refreshRate)
    .arg(dpi)
    .arg(adapter)
    .arg(primary ? " (primary)" : "");
}

} // namespace
