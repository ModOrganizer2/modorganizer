#ifndef TRANSLATIONMANAGER_H
#define TRANSLATIONMANAGER_H

#include <QApplication>
#include <QTranslator>

#include <translation.h>

#include "extensionwatcher.h"

class TranslationManager : public ExtensionWatcher
{
public:
  TranslationManager(QApplication* application);

  // retrieve the list of available translations
  //
  const auto& translations() const { return m_translations; }

  // load the given translation
  //
  bool load(std::shared_ptr<const MOBase::Translation> translation);
  bool load(std::string_view language);

  // unload the current translation
  //
  void unload();

  // retrieve the current language, if there is one
  //
  auto currentTranslation() const { return m_currentTranslation; }

public:  // ExtensionWatcher
  void extensionLoaded(MOBase::IExtension const& extension) override;
  void extensionUnloaded(MOBase::IExtension const& extension) override;
  void extensionEnabled(MOBase::IExtension const& extension) override;
  void extensionDisabled(MOBase::IExtension const& extension) override;

private:
  // register a translation
  //
  void
  registerTranslation(std::shared_ptr<const MOBase::Translation> const& translation);

  // [deprecated] add themes for the translations folder
  //
  [[deprecated]] void addOldFormatTranslations();

private:
  // TODO: move these two elsewhere
  struct string_equal : std::equal_to<std::string_view>
  {
    using is_transparent = std::true_type;
  };

  struct string_hash : std::hash<std::string_view>
  {
    using is_transparent = std::true_type;
  };

  // application
  QApplication* m_app;

  // installed translators
  std::vector<std::unique_ptr<QTranslator>> m_translators;

  // the current translation
  std::shared_ptr<const MOBase::Translation> m_currentTranslation;

  // the list of base translations
  std::vector<std::shared_ptr<const MOBase::Translation>> m_translations;
  std::unordered_map<std::string, std::shared_ptr<const MOBase::Translation>,
                     string_hash, string_equal>
      m_translationByLanguage;

  // the list of translations extensions
  std::unordered_map<std::string,
                     std::vector<std::shared_ptr<const MOBase::TranslationAddition>>,
                     string_hash, string_equal>
      m_translationExtensions;
};

#endif
