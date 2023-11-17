#ifndef SETTINGSDIALOGGENERAL_H
#define SETTINGSDIALOGGENERAL_H

#include "plugincontainer.h"
#include "settings.h"
#include "settingsdialog.h"
#include "translationmanager.h"

class GeneralSettingsTab : public SettingsTab
{
public:
  GeneralSettingsTab(Settings& settings, TranslationManager const& translationManager,
                     SettingsDialog& dialog);

  void update();

private:
  void addLanguages(TranslationManager const& translationManager);
  void selectLanguage();

  void resetDialogs();

  void onEditCategories();
  void onResetDialogs();
};

#endif  // SETTINGSDIALOGGENERAL_H
