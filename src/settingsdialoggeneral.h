#ifndef SETTINGSDIALOGGENERAL_H
#define SETTINGSDIALOGGENERAL_H

#include "settings.h"
#include "settingsdialog.h"
#include "plugincontainer.h"

class GeneralSettingsTab : public SettingsTab
{
public:
  GeneralSettingsTab(Settings& settings, PluginContainer *pluginContainer, SettingsDialog& dialog);

  void update();

private:
  void addLanguages();
  void selectLanguage();

  void resetDialogs();

  void onEditCategories();
  void onResetDialogs();

private:
  PluginContainer* m_PluginContainer;

};

#endif  // SETTINGSDIALOGGENERAL_H
