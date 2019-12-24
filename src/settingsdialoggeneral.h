#ifndef SETTINGSDIALOGGENERAL_H
#define SETTINGSDIALOGGENERAL_H

#include "settingsdialog.h"
#include "settings.h"
#include "plugincontainer.h"

class GeneralSettingsTab : public SettingsTab
{
public:
  GeneralSettingsTab(Settings& settings, PluginContainer *pluginContainer, SettingsDialog& dialog);

  void update();

private:
  void addLanguages();
  void selectLanguage();

  void addStyles();
  void selectStyle();

  void resetDialogs();

  void onExploreStyles();
  void onEditCategories();
  void onResetColors();
  void onResetDialogs();

private:
  PluginContainer* m_PluginContainer;

};

#endif // SETTINGSDIALOGGENERAL_H
