#include "settingsdialoggeneral.h"
#include "ui_settingsdialog.h"
#include "shared/appconfig.h"
#include "categoriesdialog.h"
#include "colortable.h"
#include <utility.h>
#include <questionboxmemory.h>

using namespace MOBase;

GeneralSettingsTab::GeneralSettingsTab(Settings& s, SettingsDialog& d)
  : SettingsTab(s, d)
{
  addLanguages();
  selectLanguage();

  addStyles();
  selectStyle();

  ui->colorTable->load(s);

  ui->centerDialogs->setChecked(settings().geometry().centerDialogs());
  ui->changeGameConfirmation->setChecked(settings().interface().showChangeGameConfirmation());
  ui->doubleClickPreviews->setChecked(settings().interface().doubleClicksOpenPreviews());
  ui->compactBox->setChecked(settings().interface().compactDownloads());
  ui->showMetaBox->setChecked(settings().interface().metaDownloads());
  ui->checkForUpdates->setChecked(settings().checkForUpdates());
  ui->usePrereleaseBox->setChecked(settings().usePrereleases());
  ui->colorSeparatorsBox->setChecked(settings().colors().colorSeparatorScrollbar());

  QObject::connect(ui->exploreStyles, &QPushButton::clicked, [&]{ onExploreStyles(); });

  QObject::connect(
    ui->categoriesBtn, &QPushButton::clicked, [&]{ onEditCategories(); });

  QObject::connect(
    ui->resetColorsBtn, &QPushButton::clicked, [&]{ onResetColors(); });

  QObject::connect(
    ui->resetDialogsButton, &QPushButton::clicked, [&]{ onResetDialogs(); });
}

void GeneralSettingsTab::update()
{
  const QString oldLanguage = settings().interface().language();
  const QString newLanguage = ui->languageBox->itemData(
    ui->languageBox->currentIndex()).toString();

  if (newLanguage != oldLanguage) {
    settings().interface().setLanguage(newLanguage);
    emit settings().languageChanged(newLanguage);
  }

  const QString oldStyle = settings().interface().styleName().value_or("");
  const QString newStyle = ui->styleBox->itemData(
    ui->styleBox->currentIndex()).toString();

  if (oldStyle != newStyle) {
    settings().interface().setStyleName(newStyle);
    emit settings().styleChanged(newStyle);
  }

  ui->colorTable->commitColors();

  settings().geometry().setCenterDialogs(ui->centerDialogs->isChecked());
  settings().interface().setShowChangeGameConfirmation(ui->changeGameConfirmation->isChecked());
  settings().interface().setDoubleClicksOpenPreviews(ui->doubleClickPreviews->isChecked());
  settings().interface().setCompactDownloads(ui->compactBox->isChecked());
  settings().interface().setMetaDownloads(ui->showMetaBox->isChecked());
  settings().setCheckForUpdates(ui->checkForUpdates->isChecked());
  settings().setUsePrereleases(ui->usePrereleaseBox->isChecked());
  settings().colors().setColorSeparatorScrollbar(ui->colorSeparatorsBox->isChecked());
}

void GeneralSettingsTab::addLanguages()
{
  // matches the end of filenames for something like "_en.qm" or "_zh_CN.qm"
  const QString pattern =
    QString::fromStdWString(AppConfig::translationPrefix()) +
    "_([a-z]{2,3}(_[A-Z]{2,2})?).qm";

  const QRegExp exp(pattern);

  QString translationsPath = qApp->applicationDirPath()
    + "/" + QString::fromStdWString(AppConfig::translationsPath());
  QDirIterator iter(translationsPath, QDir::Files);

  std::vector<std::pair<QString, QString>> languages;

  while (iter.hasNext()) {
    iter.next();

    const QString file = iter.fileName();
    if (!exp.exactMatch(file)) {
      continue;
    }

    const QString languageCode = exp.cap(1);
    const QLocale locale(languageCode);

    QString languageString = QString("%1 (%2)")
      .arg(locale.nativeLanguageName())
      .arg(locale.nativeCountryName());

    if (locale.language() == QLocale::Chinese) {
      if (languageCode == "zh_TW") {
        languageString = "Chinese (Traditional)";
      } else {
        languageString = "Chinese (Simplified)";
      }
    }

    languages.push_back({languageString, exp.cap(1)});
  }

  if (!ui->languageBox->findText("English")) {
    languages.push_back({QString("English"), QString("en_US")});
  }

  std::sort(languages.begin(), languages.end());

  for (const auto& lang : languages) {
    ui->languageBox->addItem(lang.first, lang.second);
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

void GeneralSettingsTab::addStyles()
{
  ui->styleBox->addItem("None", "");
  for (auto&& key : QStyleFactory::keys()) {
    ui->styleBox->addItem(key, key);
  }

  ui->styleBox->insertSeparator(ui->styleBox->count());

  QDirIterator iter(
    QCoreApplication::applicationDirPath() + "/" +
      QString::fromStdWString(AppConfig::stylesheetsPath()),
        QStringList("*.qss"),
    QDir::Files);

  while (iter.hasNext()) {
    iter.next();

    ui->styleBox->addItem(
      iter.fileInfo().completeBaseName(),
      iter.fileName());
  }
}

void GeneralSettingsTab::selectStyle()
{
  const int currentID = ui->styleBox->findData(
    settings().interface().styleName().value_or(""));

  if (currentID != -1) {
    ui->styleBox->setCurrentIndex(currentID);
  }
}

void GeneralSettingsTab::resetDialogs()
{
  settings().widgets().resetQuestionButtons();
  GlobalSettings::resetDialogs();
}

void GeneralSettingsTab::onExploreStyles()
{
  QString ssPath = QCoreApplication::applicationDirPath() + "/" + ToQString(AppConfig::stylesheetsPath());
  shell::Explore(ssPath);
}

void GeneralSettingsTab::onEditCategories()
{
  CategoriesDialog dialog(&dialog());

  if (dialog.exec() == QDialog::Accepted) {
    dialog.commitChanges();
  }
}

void GeneralSettingsTab::onResetColors()
{
  ui->colorTable->resetColors();
}

void GeneralSettingsTab::onResetDialogs()
{
  const auto r = QMessageBox::question(
    parentWidget(),
    QObject::tr("Confirm?"),
    QObject::tr(
      "This will reset all the choices you made to dialogs and make them all "
      "visible again. Continue?"),
    QMessageBox::Yes | QMessageBox::No);

  if (r == QMessageBox::Yes) {
    resetDialogs();
  }
}
