#ifndef SETTINGSDIALOGWORKAROUNDS_H
#define SETTINGSDIALOGWORKAROUNDS_H

#include "settings.h"
#include "settingsdialog.h"

class WorkaroundsSettingsTab : public SettingsTab
{
public:
  WorkaroundsSettingsTab(Settings *m_parent, SettingsDialog &m_dialog);

  void update();

private:
  QString m_ExecutableBlacklist;

  void on_bsaDateBtn_clicked();
  void on_execBlacklistBtn_clicked();
  void on_resetGeometryBtn_clicked();

  QString getExecutableBlacklist() { return m_ExecutableBlacklist; }
  void setExecutableBlacklist(QString blacklist) { m_ExecutableBlacklist = blacklist; }
};

#endif // SETTINGSDIALOGWORKAROUNDS_H
