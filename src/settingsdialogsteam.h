#ifndef SETTINGSDIALOGSTEAM_H
#define SETTINGSDIALOGSTEAM_H

#include "settings.h"
#include "settingsdialog.h"

class SteamSettingsTab : public SettingsTab
{
public:
  SteamSettingsTab(Settings *m_parent, SettingsDialog &m_dialog);

  void update();

private:
};

#endif // SETTINGSDIALOGSTEAM_H
