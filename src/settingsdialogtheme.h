#ifndef SETTINGSDIALOGTHEME_H
#define SETTINGSDIALOGTHEME_H

#include <QCheckBox>

#include "settings.h"
#include "settingsdialog.h"
#include "thememanager.h"

class ThemeSettingsTab : public SettingsTab
{
public:
  ThemeSettingsTab(Settings& settings, ThemeManager const& manager,
                   SettingsDialog& dialog);

  void update() override;

private:
  void addStyles(ThemeManager const& manager);
  void selectStyle();
};

#endif  // SETTINGSDIALOGGENERAL_H
