#include "settingsdialoggeneral.h"
#include "categoriesdialog.h"
#include "colortable.h"
#include "shared/appconfig.h"
#include "ui_settingsdialog.h"
#include <questionboxmemory.h>
#include <utility.h>

using namespace MOBase;

GeneralSettingsTab::GeneralSettingsTab(Settings& s,
                                       TranslationManager const& translationManager,
                                       SettingsDialog& d)
    : SettingsTab(s, d)
{
  // language
  addLanguages(translationManager);
  selectLanguage();

  // download list
  ui->compactBox->setChecked(settings().interface().compactDownloads());
  ui->showMetaBox->setChecked(settings().interface().metaDownloads());
  ui->hideDownloadInstallBox->setChecked(
      settings().interface().hideDownloadsAfterInstallation());

  // updates
  ui->checkForUpdates->setChecked(settings().checkForUpdates());
  ui->usePrereleaseBox->setChecked(settings().usePrereleases());

  // profile defaults
  ui->localINIs->setChecked(settings().profileLocalInis());
  ui->localSaves->setChecked(settings().profileLocalSaves());
  ui->automaticArchiveInvalidation->setChecked(settings().profileArchiveInvalidation());

  // miscellaneous
  ui->centerDialogs->setChecked(settings().geometry().centerDialogs());
  ui->changeGameConfirmation->setChecked(
      settings().interface().showChangeGameConfirmation());
  ui->showMenubarOnAlt->setChecked(settings().interface().showMenubarOnAlt());
  ui->doubleClickPreviews->setChecked(
      settings().interface().doubleClicksOpenPreviews());

  QObject::connect(ui->categoriesBtn, &QPushButton::clicked, [&] {
    onEditCategories();
  });

  QObject::connect(ui->resetDialogsButton, &QPushButton::clicked, [&] {
    onResetDialogs();
  });
}

void GeneralSettingsTab::update()
{
  // language
  const QString oldLanguage = settings().interface().language();
  const QString newLanguage =
      ui->languageBox->itemData(ui->languageBox->currentIndex()).toString();

  if (newLanguage != oldLanguage) {
    settings().interface().setLanguage(newLanguage);
    emit settings().languageChanged(newLanguage);
  }

  // download list
  settings().interface().setCompactDownloads(ui->compactBox->isChecked());
  settings().interface().setMetaDownloads(ui->showMetaBox->isChecked());
  settings().interface().setHideDownloadsAfterInstallation(
      ui->hideDownloadInstallBox->isChecked());

  // updates
  settings().setCheckForUpdates(ui->checkForUpdates->isChecked());
  settings().setUsePrereleases(ui->usePrereleaseBox->isChecked());

  // profile defaults
  settings().setProfileLocalInis(ui->localINIs->isChecked());
  settings().setProfileLocalSaves(ui->localSaves->isChecked());
  settings().setProfileArchiveInvalidation(
      ui->automaticArchiveInvalidation->isChecked());

  // miscellaneous
  settings().geometry().setCenterDialogs(ui->centerDialogs->isChecked());
  settings().interface().setShowChangeGameConfirmation(
      ui->changeGameConfirmation->isChecked());
  settings().interface().setShowMenubarOnAlt(ui->showMenubarOnAlt->isChecked());
  settings().interface().setDoubleClicksOpenPreviews(
      ui->doubleClickPreviews->isChecked());
}

void GeneralSettingsTab::addLanguages(TranslationManager const& manager)
{
  auto translations = manager.translations();

  std::sort(translations.begin(), translations.end(), [](auto&& lhs, auto&& rhs) {
    return std::forward_as_tuple(lhs->language(), lhs->identifier()) <
           std::forward_as_tuple(rhs->language(), rhs->identifier());
  });

  for (const auto& translation : translations) {
    ui->languageBox->addItem(ToQString(translation->language()),
                             ToQString(translation->identifier()));
  }
}

void GeneralSettingsTab::selectLanguage()
{
  QString languageCode = settings().interface().language();
  int currentID        = ui->languageBox->findData(languageCode);
  // I made a mess. :( Most languages are stored with only the iso country
  // code (2 characters like "de") but chinese
  // with the exact language variant (zh_TW) so I have to search for both
  // variants
  if (currentID == -1) {
    currentID = ui->languageBox->findData(languageCode.mid(0, 2));
  }
  if (currentID != -1) {
    ui->languageBox->setCurrentIndex(currentID);
  }
}

void GeneralSettingsTab::resetDialogs()
{
  settings().widgets().resetQuestionButtons();
  GlobalSettings::resetDialogs();
}

void GeneralSettingsTab::onEditCategories()
{
  CategoriesDialog catDialog(&dialog());

  if (catDialog.exec() == QDialog::Accepted) {
    catDialog.commitChanges();
  }
}

void GeneralSettingsTab::onResetDialogs()
{
  const auto r = QMessageBox::question(
      parentWidget(), QObject::tr("Confirm?"),
      QObject::tr(
          "This will reset all the choices you made to dialogs and make them all "
          "visible again. Continue?"),
      QMessageBox::Yes | QMessageBox::No);

  if (r == QMessageBox::Yes) {
    resetDialogs();
  }
}
