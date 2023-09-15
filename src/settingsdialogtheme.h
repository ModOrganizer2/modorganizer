#ifndef SETTINGSDIALOGTHEME_H
#define SETTINGSDIALOGTHEME_H

#include <QCheckBox>

#include "settings.h"
#include "settingsdialog.h"

class ThemeSettingsTab : public SettingsTab
{
public:
  ThemeSettingsTab(Settings& settings, SettingsDialog& dialog);

  void update() override;

private:
  void addStyles();
  void selectStyle();
  void onExploreStyles();
};

#endif  // SETTINGSDIALOGGENERAL_H
