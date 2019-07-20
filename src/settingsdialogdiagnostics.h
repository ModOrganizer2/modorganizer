#ifndef SETTINGSDIALOGDIAGNOSTICS_H
#define SETTINGSDIALOGDIAGNOSTICS_H

#include "settings.h"
#include "settingsdialog.h"

class DiagnosticsSettingsTab : public SettingsTab
{
public:
  DiagnosticsSettingsTab(Settings *parent, SettingsDialog &dialog);

  void update();

private:
};

#endif // SETTINGSDIALOGDIAGNOSTICS_H
