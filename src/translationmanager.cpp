#include "translationmanager.h"

#include <QDir>
#include <QDirIterator>
#include <QProxyStyle>

#include <log.h>
#include <utility.h>

#include "shared/appconfig.h"

using namespace MOBase;

TranslationManager::TranslationManager(QApplication* application) : m_app{application}
{
  // TODO: remove this
  addOldFormatTranslations();

  registerTranslation(std::make_shared<Translation>(
      "en_US", "English", std::vector<std::filesystem::path>{}));

  // specific translation to allow load("") to actually unload
  m_translationByLanguage[""] = nullptr;
}

// TODO: remove this
void TranslationManager::addOldFormatTranslations()
{
  const QRegularExpression mainTranslationPattern(QRegularExpression::anchoredPattern(
      QString::fromStdWString(AppConfig::translationPrefix()) +
      "_([a-z]{2,3}(_[A-Z]{2,2})?).qm"));

  // extract the main translations
  QDirIterator iter(QCoreApplication::applicationDirPath() + "/translations",
                    QDir::Files);
  while (iter.hasNext()) {
    iter.next();

    const QString file = iter.fileName();
    auto match         = mainTranslationPattern.match(file);
    if (!match.hasMatch()) {
      continue;
    }

    const QString languageCode = match.captured(1);
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

    std::vector<std::filesystem::path> qm_files{iter.fileInfo().filesystemFilePath()};

    if (auto qt_path = qm_files[0].parent_path() / ("qt_" + languageCode.toStdString());
        exists(qt_path)) {
      qm_files.push_back(qt_path);
    }

    if (auto qtbase_path =
            qm_files[0].parent_path() / ("qtbase_" + languageCode.toStdString());
        exists(qtbase_path)) {
      qm_files.push_back(qtbase_path);
    }

    registerTranslation(std::make_shared<Translation>(
        languageCode.toStdString(), languageString.toStdString(), qm_files));
  }

  // lookup each file except for main and Qt and add an extension for them
  for (auto& [code, translation] : m_translationByLanguage) {
    QDirIterator iter(QCoreApplication::applicationDirPath() + "/translations",
                      {ToQString("*_" + code + ".qm")}, QDir::Files);

    while (iter.hasNext()) {
      iter.next();
      const auto filename = iter.fileName();

      // skip main files
      if (filename.startsWith(AppConfig::translationPrefix()) ||
          filename.startsWith("qt")) {
        continue;
      }

      m_translationExtensions[code].push_back(std::make_shared<TranslationAddition>(
          code, std::vector{iter.fileInfo().filesystemFilePath()}));
    }
  }
}

bool TranslationManager::load(std::shared_ptr<const Translation> translation)
{
  // no translation -> abort
  if (!translation) {
    unload();
    return true;
  }

  // do not reload the current translation
  if (translation == m_currentTranslation) {
    return true;
  }

  // unload previous translations
  unload();

  // set the current translation
  m_currentTranslation = translation;

  // retrieve all files
  std::vector<std::filesystem::path> qm_files = translation->files();
  {
    auto it = m_translationExtensions.find(m_currentTranslation->identifier());
    if (it != m_translationExtensions.end()) {
      for (auto&& translationExtension : it->second) {
        const auto& ext_files = translationExtension->files();
        qm_files.insert(qm_files.end(), ext_files.begin(), ext_files.end());
      }
    }
  }

  // add translators
  for (const auto& qm_file : qm_files) {
    auto translator = std::make_unique<QTranslator>();
    if (translator->load(ToQString(absolute(qm_file).native()))) {
      m_app->installTranslator(translator.get());
      m_translators.push_back(std::move(translator));
    } else {
      log::warn("failed to load translation from '{}'", qm_file.native());
    }
  }

  return true;
}

bool TranslationManager::load(std::string_view language)
{
  auto it = m_translationByLanguage.find(language);
  if (it == m_translationByLanguage.end()) {
    log::error("translation for '{}' not found", language);
    return false;
  }

  return load(it->second);
}

void TranslationManager::unload()
{
  // remove translators from application
  for (auto&& translator : m_translators) {
    m_app->removeTranslator(translator.get());
  }
  m_translators.clear();

  // unset current translation
  m_currentTranslation = nullptr;
}

void TranslationManager::registerTranslation(
    std::shared_ptr<const Translation> const& translation)
{
  // two translations with same identifier, skip (+ warn)
  auto it = m_translationByLanguage.find(translation->identifier());
  if (it != m_translationByLanguage.end()) {
    log::warn("found existing translation with identifier '{}', skipping",
              translation->identifier());
    return;
  }

  m_translations.push_back(translation);
  m_translationByLanguage[translation->identifier()] = translation;
}

void TranslationManager::extensionLoaded(IExtension const& extension)
{
  for (const auto& translation : extension.translations()) {
    registerTranslation(translation);
  }

  for (const auto& translationExtension : extension.translationAdditions()) {
    const auto identifier = translationExtension->baseIdentifier();
    m_translationExtensions[identifier].push_back(translationExtension);
  }
}

void TranslationManager::extensionUnloaded(IExtension const& extension)
{
  // remove translation, unload if needed
  for (const auto& translation : extension.translations()) {
    if (m_currentTranslation == translation) {
      unload();
    }

    if (std::erase(m_translations, translation) > 0) {
      m_translationByLanguage.erase(translation->identifier());
    }
  }

  for (const auto& translationExtension : extension.translationAdditions()) {
    if (m_translationExtensions.contains(translationExtension->baseIdentifier())) {
      std::erase(m_translationExtensions[translationExtension->baseIdentifier()],
                 translationExtension);
    }
  }
}

void TranslationManager::extensionEnabled(IExtension const& extension)
{
  extensionLoaded(extension);
}

void TranslationManager::extensionDisabled(IExtension const& extension)
{
  extensionUnloaded(extension);
}
