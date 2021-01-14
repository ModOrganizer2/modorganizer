#ifndef SETTINGSDIALOGUSERINTERFACE_H
#define SETTINGSDIALOGUSERINTERFACE_H

#include <QCheckBox>

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

private:

  const std::map<int, QCheckBox*> m_columnToBox;
};

#endif // SETTINGSDIALOGGENERAL_H
