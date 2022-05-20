#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QApplication>
#include <QFileSystemWatcher>

#include "extensionwatcher.h"

class ThemeManager : public ExtensionWatcher
{
public:
  ThemeManager(QApplication* application);

  // retrieve the list of available themes
  //
  const auto& themes() const { return m_baseThemes; }

  // load the given theme
  //
  bool load(std::shared_ptr<const MOBase::Theme> theme);
  bool load(std::string_view themeIdentifier);

  // unload the current theme
  //
  void unload();

  // retrieve the current theme, if there is one
  //
  auto currentTheme() const { return m_currentTheme; }

  // check if the given theme is a built-in Qt theme (not from an extension)
  //
  bool isBuiltIn(std::shared_ptr<const MOBase::Theme> const& theme) const
  {
    return theme->stylesheet().empty();
  }

public:  // ExtensionWatcher
  void extensionLoaded(MOBase::IExtension const& extension) override;
  void extensionUnloaded(MOBase::IExtension const& extension) override;
  void extensionEnabled(MOBase::IExtension const& extension) override;
  void extensionDisabled(MOBase::IExtension const& extension) override;

private:
  // reload the current style
  //
  void reload();

  // register a theme
  //
  void registerTheme(std::shared_ptr<const MOBase::Theme> const& theme);

  // add built-in themes
  //
  void addQtThemes();

  // load a Qt theme
  //
  void loadQtTheme(std::string_view identifier);

  // load an extension theme
  //
  void loadExtensionTheme(std::shared_ptr<const MOBase::Theme> const& theme);

  // build a stylesheet for a theme
  //
  QString buildStyleSheet(std::shared_ptr<const MOBase::Theme> const& theme) const;

  // patch the given stylesheet by replacing url() to be relative to the given folder
  //
  QString patchStyleSheet(QString stylesheet,
                          std::filesystem::path const& folder) const;

  // watch files for the given theme (can be nullptr to stop watching)
  //
  void watchThemeFiles(std::shared_ptr<const MOBase::Theme> const& theme);

  // [deprecated] add themes for the stylesheets folder
  //
  [[deprecated]] void addOldFormatThemes();

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

  // application and file system watcher
  QApplication* m_app;
  QFileSystemWatcher m_watcher;

  // the default current theme
  std::shared_ptr<const MOBase::Theme> m_defaultTheme;
  std::shared_ptr<const MOBase::Theme> m_currentTheme;

  // the list of base themes
  std::vector<std::shared_ptr<const MOBase::Theme>> m_baseThemes;
  std::unordered_map<std::string, std::shared_ptr<const MOBase::Theme>, string_hash,
                     string_equal>
      m_baseThemesByIdentifier;

  // the list of theme extensions per theme identifier, for extension that
  // have identifiers
  std::unordered_map<std::string,
                     std::vector<std::shared_ptr<const MOBase::ThemeExtension>>,
                     string_hash, string_equal>
      m_themeExtensionsByIdentifier;

  // theme extensions for all themes
  std::vector<std::shared_ptr<const MOBase::ThemeExtension>> m_globalExtensions;
};

#endif
