#ifndef SETTINGSDIALOGUSERINTERFACE_H
#define SETTINGSDIALOGUSERINTERFACE_H

#include "settingsdialog.h"
#include "settings.h"

class UserInterfaceSettingsTab : public SettingsTab
{
public:
  UserInterfaceSettingsTab(Settings& settings, SettingsDialog& dialog);

  void update() override;
};

#endif // SETTINGSDIALOGGENERAL_H
