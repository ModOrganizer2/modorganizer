#ifndef ENV_METRICS_H
#define ENV_METRICS_H

#include <QString>
#include <vector>

namespace env
{

// information about a monitor
//
class Display
{
public:
  Display(QString adapter, QString monitorDevice, bool primary);

  // display name of the adapter running the monitor
  //
  const QString& adapter() const;

  // internal device name of the monitor, this is not a display name
  //
  const QString& monitorDevice() const;

  // whether this monitor is the primary
  //
  bool primary();

  // resolution
  //
  int resX() const;
  int resY() const;

  // dpi
  //
  int dpi();

  // refresh rate in hz
  //
  int refreshRate() const;

  // string representation
  //
  QString toString() const;

private:
  QString m_adapter;
  QString m_monitorDevice;
  bool m_primary;
  int m_resX, m_resY;
  int m_dpi;
  int m_refreshRate;

  void getSettings();
};

// holds various information about Windows metrics
//
class Metrics
{
public:
  Metrics();

  // list of displays on the system
  //
  const std::vector<Display>& displays() const;

  // full resolution
  //
  QRect desktopGeometry() const;

private:
  std::vector<Display> m_displays;

  void getDisplays();
};

}  // namespace env

#endif  // ENV_METRICS_H
