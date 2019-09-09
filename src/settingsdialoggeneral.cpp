#include "settingsdialoggeneral.h"
#include "ui_settingsdialog.h"
#include "appconfig.h"
#include "categoriesdialog.h"
#include <questionboxmemory.h>

using MOBase::QuestionBoxMemory;

GeneralSettingsTab::GeneralSettingsTab(Settings& s, SettingsDialog& d)
  : SettingsTab(s, d)
{
  addLanguages();
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

  addStyles();

  {
    const int currentID = ui->styleBox->findData(
      settings().interface().styleName().value_or(""));

    if (currentID != -1) {
      ui->styleBox->setCurrentIndex(currentID);
    }
  }

  //version with stylesheet
  setButtonColor(ui->overwritingBtn, settings().colors().modlistOverwritingLoose());
  setButtonColor(ui->overwrittenBtn, settings().colors().modlistOverwrittenLoose());
  setButtonColor(ui->overwritingArchiveBtn, settings().colors().modlistOverwritingArchive());
  setButtonColor(ui->overwrittenArchiveBtn, settings().colors().modlistOverwrittenArchive());
  setButtonColor(ui->containsBtn, settings().colors().modlistContainsPlugin());
  setButtonColor(ui->containedBtn, settings().colors().pluginListContained());

  setOverwritingColor(settings().colors().modlistOverwritingLoose());
  setOverwrittenColor(settings().colors().modlistOverwrittenLoose());
  setOverwritingArchiveColor(settings().colors().modlistOverwritingArchive());
  setOverwrittenArchiveColor(settings().colors().modlistOverwrittenArchive());
  setContainsColor(settings().colors().modlistContainsPlugin());
  setContainedColor(settings().colors().pluginListContained());

  ui->compactBox->setChecked(settings().interface().compactDownloads());
  ui->showMetaBox->setChecked(settings().interface().metaDownloads());
  ui->usePrereleaseBox->setChecked(settings().usePrereleases());
  ui->colorSeparatorsBox->setChecked(settings().colors().colorSeparatorScrollbar());

  QObject::connect(ui->overwritingArchiveBtn, &QPushButton::clicked, [&]{ on_overwritingArchiveBtn_clicked(); });
  QObject::connect(ui->overwritingBtn, &QPushButton::clicked, [&]{ on_overwritingBtn_clicked(); });
  QObject::connect(ui->overwrittenArchiveBtn, &QPushButton::clicked, [&]{ on_overwrittenArchiveBtn_clicked(); });
  QObject::connect(ui->overwrittenBtn, &QPushButton::clicked, [&]{ on_overwrittenBtn_clicked(); });
  QObject::connect(ui->containedBtn, &QPushButton::clicked, [&]{ on_containedBtn_clicked(); });
  QObject::connect(ui->containsBtn, &QPushButton::clicked, [&]{ on_containsBtn_clicked(); });
  QObject::connect(ui->categoriesBtn, &QPushButton::clicked, [&]{ on_categoriesBtn_clicked(); });
  QObject::connect(ui->resetColorsBtn, &QPushButton::clicked, [&]{ on_resetColorsBtn_clicked(); });
  QObject::connect(ui->resetDialogsButton, &QPushButton::clicked, [&]{ on_resetDialogsButton_clicked(); });
}

void GeneralSettingsTab::update()
{
  const QString oldLanguage = settings().interface().language();
  const QString newLanguage = ui->languageBox->itemData(ui->languageBox->currentIndex()).toString();

  if (newLanguage != oldLanguage) {
    settings().interface().setLanguage(newLanguage);
    emit settings().languageChanged(newLanguage);
  }

  const QString oldStyle = settings().interface().styleName().value_or("");
  const QString newStyle = ui->styleBox->itemData(ui->styleBox->currentIndex()).toString();
  if (oldStyle != newStyle) {
    settings().interface().setStyleName(newStyle);
    emit settings().styleChanged(newStyle);
  }

  settings().colors().setModlistOverwritingLoose(getOverwritingColor());
  settings().colors().setModlistOverwrittenLoose(getOverwrittenColor());
  settings().colors().setModlistOverwritingArchive(getOverwritingArchiveColor());
  settings().colors().setModlistOverwrittenArchive(getOverwrittenArchiveColor());
  settings().colors().setModlistContainsPlugin(getContainsColor());
  settings().colors().setPluginListContained(getContainedColor());

  settings().interface().setCompactDownloads(ui->compactBox->isChecked());
  settings().interface().setMetaDownloads(ui->showMetaBox->isChecked());
  settings().setUsePrereleases(ui->usePrereleaseBox->isChecked());
  settings().colors().setColorSeparatorScrollbar(ui->colorSeparatorsBox->isChecked());
}

void GeneralSettingsTab::addLanguages()
{
  std::vector<std::pair<QString, QString>> languages;

  QDirIterator langIter(QCoreApplication::applicationDirPath() + "/translations", QDir::Files);
  QString pattern = QString::fromStdWString(AppConfig::translationPrefix()) +  "_([a-z]{2,3}(_[A-Z]{2,2})?).qm";
  QRegExp exp(pattern);
  while (langIter.hasNext()) {
    langIter.next();
    QString file = langIter.fileName();
    if (exp.exactMatch(file)) {
      QString languageCode = exp.cap(1);
      QLocale locale(languageCode);
      QString languageString = QString("%1 (%2)").arg(locale.nativeLanguageName()).arg(locale.nativeCountryName());  //QLocale::languageToString(locale.language());
      if (locale.language() == QLocale::Chinese) {
        if (languageCode == "zh_TW") {
          languageString = "Chinese (traditional)";
        } else {
          languageString = "Chinese (simplified)";
        }
      }
      languages.push_back(std::make_pair(QString("%1").arg(languageString), exp.cap(1)));
    }
  }
  if (!ui->languageBox->findText("English")) {
    languages.push_back(std::make_pair(QString("English"), QString("en_US")));
  }
  std::sort(languages.begin(), languages.end());
  for (const auto &lang : languages) {
    ui->languageBox->addItem(lang.first, lang.second);
  }
}

void GeneralSettingsTab::addStyles()
{
  ui->styleBox->addItem("None", "");
  ui->styleBox->addItem("Fusion", "Fusion");

  QDirIterator langIter(QCoreApplication::applicationDirPath() + "/" + QString::fromStdWString(AppConfig::stylesheetsPath()), QStringList("*.qss"), QDir::Files);
  while (langIter.hasNext()) {
    langIter.next();
    QString style = langIter.fileName();
    ui->styleBox->addItem(style, style);
  }
}

void GeneralSettingsTab::resetDialogs()
{
  settings().widgets().resetQuestionButtons();
}

void GeneralSettingsTab::setButtonColor(QPushButton *button, const QColor &color)
{
  button->setStyleSheet(
    QString("QPushButton {"
      "background-color: rgba(%1, %2, %3, %4);"
      "color: %5;"
      "border: 1px solid;"
      "padding: 3px;"
      "}")
    .arg(color.red())
    .arg(color.green())
    .arg(color.blue())
    .arg(color.alpha())
    .arg(ColorSettings::idealTextColor(color).name())
  );
};

void GeneralSettingsTab::on_containsBtn_clicked()
{
  QColor result = QColorDialog::getColor(m_ContainsColor, &dialog(), "Color Picker: Mod contains selected plugin", QColorDialog::ShowAlphaChannel);
  if (result.isValid()) {
    m_ContainsColor = result;
    setButtonColor(ui->containsBtn, result);
  }
}

void GeneralSettingsTab::on_containedBtn_clicked()
{
  QColor result = QColorDialog::getColor(m_ContainedColor, &dialog(), "ColorPicker: Plugin is Contained in selected Mod", QColorDialog::ShowAlphaChannel);
  if (result.isValid()) {
    m_ContainedColor = result;
    setButtonColor(ui->containedBtn, result);
  }
}

void GeneralSettingsTab::on_overwrittenBtn_clicked()
{
  QColor result = QColorDialog::getColor(m_OverwrittenColor, &dialog(), "ColorPicker: Is overwritten (loose files)", QColorDialog::ShowAlphaChannel);
  if (result.isValid()) {
    m_OverwrittenColor = result;
    setButtonColor(ui->overwrittenBtn, result);
  }
}

void GeneralSettingsTab::on_overwritingBtn_clicked()
{
  QColor result = QColorDialog::getColor(m_OverwritingColor, &dialog(), "ColorPicker: Is overwriting (loose files)", QColorDialog::ShowAlphaChannel);
  if (result.isValid()) {
    m_OverwritingColor = result;
    setButtonColor(ui->overwritingBtn, result);
  }
}

void GeneralSettingsTab::on_overwrittenArchiveBtn_clicked()
{
  QColor result = QColorDialog::getColor(m_OverwrittenArchiveColor, &dialog(), "ColorPicker: Is overwritten (archive files)", QColorDialog::ShowAlphaChannel);
  if (result.isValid()) {
    m_OverwrittenArchiveColor = result;
    setButtonColor(ui->overwrittenArchiveBtn, result);
  }
}

void GeneralSettingsTab::on_overwritingArchiveBtn_clicked()
{
  QColor result = QColorDialog::getColor(m_OverwritingArchiveColor, &dialog(), "ColorPicker: Is overwriting (archive files)", QColorDialog::ShowAlphaChannel);
  if (result.isValid()) {
    m_OverwritingArchiveColor = result;
    setButtonColor(ui->overwritingArchiveBtn, result);
  }
}

void GeneralSettingsTab::on_resetColorsBtn_clicked()
{
  m_OverwritingColor = QColor(255, 0, 0, 64);
  m_OverwrittenColor = QColor(0, 255, 0, 64);
  m_OverwritingArchiveColor = QColor(255, 0, 255, 64);
  m_OverwrittenArchiveColor = QColor(0, 255, 255, 64);
  m_ContainsColor = QColor(0, 0, 255, 64);
  m_ContainedColor = QColor(0, 0, 255, 64);

  setButtonColor(ui->overwritingBtn, m_OverwritingColor);
  setButtonColor(ui->overwrittenBtn, m_OverwrittenColor);
  setButtonColor(ui->overwritingArchiveBtn, m_OverwritingArchiveColor);
  setButtonColor(ui->overwrittenArchiveBtn, m_OverwrittenArchiveColor);
  setButtonColor(ui->containsBtn, m_ContainsColor);
  setButtonColor(ui->containedBtn, m_ContainedColor);
}

void GeneralSettingsTab::on_resetDialogsButton_clicked()
{
  if (QMessageBox::question(&dialog(), QObject::tr("Confirm?"),
    QObject::tr("This will make all dialogs show up again where you checked the \"Remember selection\"-box. Continue?"),
    QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
    resetDialogs();
  }
}

void GeneralSettingsTab::on_categoriesBtn_clicked()
{
  CategoriesDialog dialog(&dialog());
  if (dialog.exec() == QDialog::Accepted) {
    dialog.commitChanges();
  }
}
