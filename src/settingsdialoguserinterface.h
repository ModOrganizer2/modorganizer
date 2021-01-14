#ifndef SETTINGSDIALOGUSERINTERFACE_H
#define SETTINGSDIALOGUSERINTERFACE_H

#include "settingsdialog.h"
#include "settings.h"

class UserInterfaceSettingsTab : public SettingsTab
{
public:
  UserInterfaceSettingsTab(Settings& settings, SettingsDialog& dialog);

  void update() override;

protected slots:

  // enable/disable the collapsible separators group depending on
  // the checkbox states
  void updateCollapsibleSeparatorsGroup();
};

#endif // SETTINGSDIALOGGENERAL_H
