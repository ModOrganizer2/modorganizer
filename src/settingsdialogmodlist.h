#ifndef SETTINGSDIALOGMODLIST_H
#define SETTINGSDIALOGMODLIST_H

#include <QCheckBox>

#include "settings.h"
#include "settingsdialog.h"

class ModListSettingsTab : public SettingsTab
{
public:
  ModListSettingsTab(Settings& settings, SettingsDialog& dialog);

  void update() override;

protected slots:

  // enable/disable the collapsible separators group depending on
  // the checkbox states
  void updateCollapsibleSeparatorsGroup();

private:
  const std::map<int, QCheckBox*> m_columnToBox;
};

#endif  // SETTINGSDIALOGGENERAL_H
