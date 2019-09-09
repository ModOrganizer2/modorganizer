#ifndef SETTINGSDIALOGSTEAM_H
#define SETTINGSDIALOGSTEAM_H

#include "settings.h"
#include "settingsdialog.h"

class SteamSettingsTab : public SettingsTab
{
public:
  SteamSettingsTab(Settings& settings, SettingsDialog& dialog);
  void update();
};

#endif // SETTINGSDIALOGSTEAM_H
