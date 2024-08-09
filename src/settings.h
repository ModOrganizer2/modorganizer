/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SETTINGS_H
#define SETTINGS_H

#include "envdump.h"
#include <lootcli/lootcli.h>
#include <questionboxmemory.h>
#include <uibase/filterwidget.h>
#include <uibase/log.h>
#include <usvfs/usvfsparameters.h>

#ifdef interface
#undef interface
#endif

namespace MOBase
{
class IPlugin;
class IPluginGame;
class ExpanderWidget;
}  // namespace MOBase

class QSplitter;

class ServerList;
class Settings;

// setting for the currently managed game
//
class GameSettings
{
public:
  GameSettings(QSettings& setting);

  // game plugin
  //
  const MOBase::IPluginGame* plugin();
  void setPlugin(const MOBase::IPluginGame* gamePlugin);

  // whether files of the core game are forced-enabled so the user can't
  // accidentally disable them
  //
  bool forceEnableCoreFiles() const;
  void setForceEnableCoreFiles(bool b);

  // the directory where the managed game is stored
  //
  std::optional<QString> directory() const;
  void setDirectory(const QString& path);

  // the name of the managed game
  //
  std::optional<QString> name() const;
  void setName(const QString& name);

  // the edition of the managed game
  //
  std::optional<QString> edition() const;
  void setEdition(const QString& name);

  // the current profile name
  //
  std::optional<QString> selectedProfileName() const;
  void setSelectedProfileName(const QString& name);

private:
  QSettings& m_Settings;
  const MOBase::IPluginGame* m_GamePlugin;
};

// geometry settings for various widgets; this should contain any setting that
// can get invalid through UI changes or when users change display settings
// (resolution, monitors, etc.); see WidgetSettings for the counterpart
//
// all these settings are stored under [Geometry] and get wiped when the
// "reset geometry settings" button is clicked in the settings
//
// saveGeometry(), restoreGeometry(), saveState() and restoreState() call the
// same functions on the given widget
//
class GeometrySettings
{
public:
  GeometrySettings(QSettings& s);

  // asks the settings to get reset
  //
  // this gets called from the settings dialog and gets picked up in
  // resetIfNeeded(), called from runApplication() just before exiting
  //
  void requestReset();
  void resetIfNeeded();

  void saveGeometry(const QMainWindow* w);
  bool restoreGeometry(QMainWindow* w) const;

  void saveGeometry(const QDialog* d);
  bool restoreGeometry(QDialog* d) const;

  void saveState(const QMainWindow* window);
  bool restoreState(QMainWindow* window) const;

  void saveState(const QHeaderView* header);
  bool restoreState(QHeaderView* header) const;

  void saveState(const QSplitter* splitter);
  bool restoreState(QSplitter* splitter) const;

  void saveState(const MOBase::ExpanderWidget* expander);
  bool restoreState(MOBase::ExpanderWidget* expander) const;

  void saveVisibility(const QWidget* w);
  bool restoreVisibility(QWidget* w, std::optional<bool> def = {}) const;

  void saveToolbars(const QMainWindow* w);
  void restoreToolbars(QMainWindow* w) const;

  void saveDocks(const QMainWindow* w);
  void restoreDocks(QMainWindow* w) const;

  // this should be a generic "tab order" setting, but it only happens for the
  // mod info dialog right now
  //
  QStringList modInfoTabOrder() const;
  void setModInfoTabOrder(const QString& names);

  // whether dialogs should be centered on their parent
  //
  bool centerDialogs() const;
  void setCenterDialogs(bool b);

  // assumes the given widget is a top-level
  //
  void centerOnMainWindowMonitor(QWidget* w) const;

  // saves the monitor number of the given window
  //
  void saveMainWindowMonitor(const QMainWindow* w);

private:
  QSettings& m_Settings;
  bool m_Reset;

  void saveWindowGeometry(const QWidget* w);
  bool restoreWindowGeometry(QWidget* w) const;

  void ensureWindowOnScreen(QWidget* w) const;
  static void centerOnMonitor(QWidget* w, int monitor);
  static void centerOnParent(QWidget* w, QWidget* parent = nullptr);
};

// widget settings that should stay valid regardless of UI changes or when users
// change display settings (resolution, monitors, etc.); see GeometrySettings
// for the counterpart
//
class WidgetSettings
{
public:
  // globalInstance is forwarded from the Settings constructor; WidgetSettings
  // has the callbacks used by QuestionBoxMemory and those are global, so they
  // should only be set by the global instance
  //
  WidgetSettings(QSettings& s, bool globalInstance);

  // tree item check - this saves the list of expanded items based on the given role
  //
  void saveTreeCheckState(const QTreeView* tv, int role = Qt::CheckStateRole);
  void restoreTreeCheckState(QTreeView* tv, int role = Qt::CheckStateRole) const;

  // tree state - this saves the list of expanded items based on the given role
  //
  void saveTreeExpandState(const QTreeView* tv, int role = Qt::DisplayRole);
  void restoreTreeExpandState(QTreeView* tv, int role = Qt::DisplayRole) const;

  // selected index for a combobox
  //
  std::optional<int> index(const QComboBox* cb) const;
  void saveIndex(const QComboBox* cb);
  void restoreIndex(QComboBox* cb, std::optional<int> def = {}) const;

  // selected tab index for a tab widget
  //
  std::optional<int> index(const QTabWidget* w) const;
  void saveIndex(const QTabWidget* w);
  void restoreIndex(QTabWidget* w, std::optional<int> def = {}) const;

  // check state for a checkable button
  //
  std::optional<bool> checked(const QAbstractButton* w) const;
  void saveChecked(const QAbstractButton* w);
  void restoreChecked(QAbstractButton* w, std::optional<bool> def = {}) const;

  // returns the remembered button for a question dialog, or NoButton if the
  // user hasn't saved the choice
  //
  MOBase::QuestionBoxMemory::Button questionButton(const QString& windowName,
                                                   const QString& filename) const;

  // sets the button to be remembered for the given window
  //
  void setQuestionWindowButton(const QString& windowName,
                               MOBase::QuestionBoxMemory::Button button);

  // sets the button to be remembered for the given file
  //
  void setQuestionFileButton(const QString& windowName, const QString& filename,
                             MOBase::QuestionBoxMemory::Button choice);

  // wipes all the remembered buttons
  //
  void resetQuestionButtons();

private:
  QSettings& m_Settings;
};

// various color settings
//
class ColorSettings
{
public:
  ColorSettings(QSettings& s);

  QColor modlistOverwrittenLoose() const;
  void setModlistOverwrittenLoose(const QColor& c);

  QColor modlistOverwritingLoose() const;
  void setModlistOverwritingLoose(const QColor& c);

  QColor modlistOverwrittenArchive() const;
  void setModlistOverwrittenArchive(const QColor& c);

  QColor modlistOverwritingArchive() const;
  void setModlistOverwritingArchive(const QColor& c);

  QColor modlistContainsPlugin() const;
  void setModlistContainsPlugin(const QColor& c);

  QColor pluginListContained() const;
  void setPluginListContained(const QColor& c);

  std::optional<QColor> previousSeparatorColor() const;
  void setPreviousSeparatorColor(const QColor& c) const;
  void removePreviousSeparatorColor();

  // whether the scrollbar of the mod list should have colors for custom
  // separator colors
  //
  bool colorSeparatorScrollbar() const;
  void setColorSeparatorScrollbar(bool b);

  // returns a color with a good contrast for the given background
  //
  static QColor idealTextColor(const QColor& rBackgroundColor);

private:
  QSettings& m_Settings;
};

// settings about plugins
//
class PluginSettings : public QObject
{
  Q_OBJECT

public:
  PluginSettings(QSettings& settings);

  // forgets all the plugins
  //
  void clearPlugins();

  // adds/removes the given plugin to the list and loads all of its settings
  //
  void registerPlugin(MOBase::IPlugin* plugin);
  void unregisterPlugin(MOBase::IPlugin* plugin);

  // returns all the registered plugins
  //
  std::vector<MOBase::IPlugin*> plugins() const;

  // returns the plugin setting for the given key
  //
  QVariant setting(const QString& pluginName, const QString& key) const;

  // sets the plugin setting for the given key
  //
  void setSetting(const QString& pluginName, const QString& key, const QVariant& value);

  // returns all settings
  //
  QVariantMap settings(const QString& pluginName) const;

  // overwrites all settings
  //
  void setSettings(const QString& pluginName, const QVariantMap& map);

  // returns all descriptions
  //
  QVariantMap descriptions(const QString& pluginName) const;

  // overwrites all descriptions
  //
  void setDescriptions(const QString& pluginName, const QVariantMap& map);

  // ?
  QVariant persistent(const QString& pluginName, const QString& key,
                      const QVariant& def) const;
  void setPersistent(const QString& pluginName, const QString& key,
                     const QVariant& value, bool sync);

  // adds the given plugin to the blacklist
  //
  void addBlacklist(const QString& fileName);

  // returns whether the given plugin is blacklisted
  //
  bool blacklisted(const QString& fileName) const;

  // overwrites the whole blacklist
  //
  void setBlacklist(const QStringList& pluginNames);

  // returns the blacklist
  //
  const QSet<QString>& blacklist() const;

  // commits all the settings to the ini
  //
  void save();

Q_SIGNALS:

  /**
   * Emitted when a plugin setting changes.
   */
  void pluginSettingChanged(QString const& pluginName, const QString& key,
                            const QVariant& oldValue, const QVariant& newValue);

private:
  QSettings& m_Settings;
  std::vector<MOBase::IPlugin*> m_Plugins;
  QMap<QString, QVariantMap> m_PluginSettings;
  QMap<QString, QVariantMap> m_PluginDescriptions;
  QSet<QString> m_PluginBlacklist;

  // commits the blacklist to the ini
  //
  void writeBlacklist();

  // reads the blacklist from the ini
  //
  QSet<QString> readBlacklist() const;
};

// paths for the game and various components
//
// if the 'resolve' parameter is true, %BASE_DIR% is expanded; it's set to
// false mostly in the settings dialog
//
class PathSettings
{
public:
  // %BASE_DIR%
  static const QString BaseDirVariable;

  PathSettings(QSettings& settings);

  QString base() const;
  void setBase(const QString& path);

  QString downloads(bool resolve = true) const;
  void setDownloads(const QString& path);

  QString mods(bool resolve = true) const;
  void setMods(const QString& path);

  QString cache(bool resolve = true) const;
  void setCache(const QString& path);

  QString profiles(bool resolve = true) const;
  void setProfiles(const QString& path);

  QString overwrite(bool resolve = true) const;
  void setOverwrite(const QString& path);

  // map of names to directories, used to remember the last directory used in
  // various file pickers
  //
  std::map<QString, QString> recent() const;
  void setRecent(const std::map<QString, QString>& map);

  // resolves %BASE_DIR%
  //
  static QString resolve(const QString& path, const QString& baseDir);

  // returns %BASE_DIR%/dirName
  //
  static QString makeDefaultPath(const QString dirName);

private:
  QSettings& m_Settings;

  QString getConfigurablePath(const QString& key, const QString& def,
                              bool resolve) const;
  void setConfigurablePath(const QString& key, const QString& path);
};

class NetworkSettings
{
public:
  // globalInstance is forwarded from the Settings constructor; NetworkSettings
  // will set the global URL custom command
  //
  NetworkSettings(QSettings& settings, bool globalInstance);

  // whether the user has disabled online features
  //
  bool offlineMode() const;
  void setOfflineMode(bool b);

  // whether the user wants to use the system proxy
  //
  bool useProxy() const;
  void setUseProxy(bool b);

  // add a new download speed to the list for the given server; each server
  // remembers the last couple of download speeds and displays the average in
  // the network settings
  //
  void setDownloadSpeed(const QString& serverName, int bytesPerSecond);

  // known servers
  //
  ServerList servers() const;

  // sets the servers
  //
  void updateServers(ServerList servers);

  // for 2.2.1 and before, rewrites the old byte array map to the new format
  //
  void updateFromOldMap();

  // whether to use a custom command to open links
  //
  bool useCustomBrowser() const;
  void setUseCustomBrowser(bool b);

  // custom command to open links
  //
  QString customBrowserCommand() const;
  void setCustomBrowserCommand(const QString& s);

  void dump() const;

private:
  QSettings& m_Settings;

  // for pre 2.2.1 ini files
  //
  ServerList serversFromOldMap() const;

  // sets the custom command in uibase, called when the settings change
  //
  void updateCustomBrowser();
};

enum class EndorsementState
{
  Accepted = 1,
  Refused,
  NoDecision
};

EndorsementState endorsementStateFromString(const QString& s);
QString toString(EndorsementState s);

class NexusSettings
{
public:
  NexusSettings(Settings& parent, QSettings& settings);

  // returns whether endorsement integration is enabled
  //
  bool endorsementIntegration() const;
  void setEndorsementIntegration(bool b) const;

  // returns the endorsement state of MO itself
  //
  EndorsementState endorsementState() const;
  void setEndorsementState(EndorsementState s);

  // returns whether tracked integration is enabled
  //
  bool trackedIntegration() const;
  void setTrackedIntegration(bool b) const;

  // returns whether nexus category mappings are enabled
  //
  bool categoryMappings() const;
  void setCategoryMappings(bool b) const;

  // registers MO as the handler for nxm links
  //
  // if 'force' is true, the registration dialog will be shown even if the user
  // said earlier not to
  //
  void registerAsNXMHandler(bool force);

  std::vector<std::chrono::seconds> validationTimeouts() const;

  // dumps nxmhandler stuff
  //
  void dump() const;

private:
  Settings& m_Parent;
  QSettings& m_Settings;
};

class SteamSettings
{
public:
  SteamSettings(Settings& parent, QSettings& settings);

  // the steam appid is assigned by the steam platform to each product sold
  // there.
  //
  // the appid may differ between different versions of a game so it may be
  // impossible for MO to automatically recognize it, though usually it does
  //
  QString appID() const;
  void setAppID(const QString& id);

  // the steam username is stored in the ini, but the password is in the
  // windows credentials store; both values are independent and either can be
  // empty
  //
  // if the username exists in the ini, it is assigned to `username`; if not
  // `username` is set to an empty string
  //
  // if the password exists in the credentials store, it is assigned to
  // `password`; if not, `password` is set to an empty string
  //
  // returns whether _both_ the username and password have a value
  //
  bool login(QString& username, QString& password) const;

  // sets the steam login; the username is saved in the ini file and the
  // password in the credentials store
  //
  // if a value is empty, it is removed from its backing store
  //
  void setLogin(QString username, QString password);

private:
  Settings& m_Parent;
  QSettings& m_Settings;
};

class InterfaceSettings
{
public:
  InterfaceSettings(QSettings& settings);

  // whether the GUI should be locked when running executables
  //
  bool lockGUI() const;
  void setLockGUI(bool b);

  // filename of the theme
  //
  std::optional<QString> themeName() const;
  void setThemeName(const QString& name);

  // whether to use collapsible separators when possible
  //
  bool collapsibleSeparators(Qt::SortOrder order) const;
  void setCollapsibleSeparators(bool ascending, bool descending);

  // whether to highlight mod conflicts and plugins on collapsed
  // separators
  //
  bool collapsibleSeparatorsHighlightTo() const;
  void setCollapsibleSeparatorsHighlightTo(bool b);

  // whether to highlight mod conflicts and plugins from separators
  // when selected but collapsed
  //
  bool collapsibleSeparatorsHighlightFrom() const;
  void setCollapsibleSeparatorsHighlightFrom(bool b);

  // whether to show icons on collapsed separators
  //
  bool collapsibleSeparatorsIcons(int column) const;
  void setCollapsibleSeparatorsIcons(int column, bool show);

  // whether each profile should have its own expansion state
  //
  bool collapsibleSeparatorsPerProfile() const;
  void setCollapsibleSeparatorsPerProfile(bool b);

  // whether to save/restore filter states between runs
  //
  bool saveFilters() const;
  void setSaveFilters(bool b);

  // whether to collapse groups (separators, categories, ...) after
  // a delay when hovering (similar to auto-expand)
  //
  bool autoCollapseOnHover() const;
  void setAutoCollapseOnHover(bool b);

  // whether to check for update after installing a mod
  //
  bool checkUpdateAfterInstallation() const;
  void setCheckUpdateAfterInstallation(bool b);

  // whether to show compact downloads
  //
  bool compactDownloads() const;
  void setCompactDownloads(bool b);

  // whether to show meta information for downloads
  //
  bool metaDownloads() const;
  void setMetaDownloads(bool b);

  // whether to hide downloads after installing them
  //
  bool hideDownloadsAfterInstallation() const;
  void setHideDownloadsAfterInstallation(bool b);

  // whether the API counter should be hidden
  //
  bool hideAPICounter() const;
  void setHideAPICounter(bool b);

  // whether the user wants to see non-official plugins installed outside MO in
  // the mod list
  //
  bool displayForeign() const;
  void setDisplayForeign(bool b);

  // short code of the configured language (corresponding to the translation
  // files)
  //
  QString language();
  void setLanguage(const QString& name);

  // whether the given tutorial has been completed
  //
  bool isTutorialCompleted(const QString& windowName) const;
  void setTutorialCompleted(const QString& windowName, bool b = true);

  // whether to show the confirmation when switching instances
  //
  bool showChangeGameConfirmation() const;
  void setShowChangeGameConfirmation(bool b);

  // whether to show the menubar when pressing the Alt key
  //
  bool showMenubarOnAlt() const;
  void setShowMenubarOnAlt(bool b);

  // whether double-clicks on files should try to open previews first
  //
  bool doubleClicksOpenPreviews() const;
  void setDoubleClicksOpenPreviews(bool b);

  // filter widget options
  //
  MOBase::FilterWidget::Options filterOptions() const;
  void setFilterOptions(const MOBase::FilterWidget::Options& o);

private:
  QSettings& m_Settings;
};

class DiagnosticsSettings
{
public:
  DiagnosticsSettings(QSettings& settings);

  // log level for both MO and usvfs
  //
  MOBase::log::Levels logLevel() const;
  void setLogLevel(MOBase::log::Levels level);

  // log level for loot
  lootcli::LogLevels lootLogLevel() const;
  void setLootLogLevel(lootcli::LogLevels level);

  // crash dump type for both MO and usvfs
  //
  env::CoreDumpTypes coreDumpType() const;
  void setCoreDumpType(env::CoreDumpTypes type);

  // maximum number of dump files keps, for both MO and usvfs
  //
  int maxCoreDumps() const;
  void setMaxCoreDumps(int n);

  std::chrono::seconds spawnDelay() const;
  void setSpawnDelay(std::chrono::seconds t);

private:
  QSettings& m_Settings;
};

// manages the settings for MO; the settings are accessed directly through a
// QSettings and so are not cached here
//
class Settings : public QObject
{
  Q_OBJECT;

public:
  // there is one Settings global object for MO when an instance is loaded, but
  // other Settings objects are required in several places, such as in the
  // Instance class, the instance dialogs, etc.
  //
  // any Settings object created with globalInstance==false won't set the
  // singleton
  //
  // some sub-objects like WidgetSettings and NetworkSettings need to know
  // whether they were created from a globalInstance, see their constructor
  //
  // @param path           path to an ini file
  // @param globalInsance  whether this is the global instance; creates the
  //                       singleton and asserts if it already exists
  //
  Settings(const QString& path, bool globalInstance = false);
  ~Settings();

  // throws if there is no global Settings instance
  //
  static Settings& instance();

  // returns null if there is no global Settings instance
  //
  static Settings* maybeInstance();

  // name of the ini file
  //
  QString filename() const;

  // version of MO stored in the ini; this may be different from the current
  // version if the user just updated
  //
  std::optional<QVersionNumber> version() const;

  // updates the settings to bring them up to date
  //
  void processUpdates(const QVersionNumber& current, const QVersionNumber& last);

  // whether MO has been started for the first time
  //
  bool firstStart() const;
  void setFirstStart(bool b);

  // configured executables
  //
  std::vector<std::map<QString, QVariant>> executables() const;
  void setExecutables(const std::vector<std::map<QString, QVariant>>& v);

  // whether to backup existing mods on install
  //
  bool keepBackupOnInstall() const;
  void setKeepBackupOnInstall(bool b);

  // blacklisted executables do not get hooked by usvfs; this list is managed
  // by MO but given to usvfs when starting an executable
  //
  QString executablesBlacklist() const;
  bool isExecutableBlacklisted(const QString& s) const;
  void setExecutablesBlacklist(const QString& s);

  QStringList skipFileSuffixes() const;
  void setSkipFileSuffixes(const QStringList& s);

  QStringList skipDirectories() const;
  void setSkipDirectories(const QStringList& s);

  // ? looks obsolete, only used by dead code
  //
  unsigned int motdHash() const;
  void setMotdHash(unsigned int hash);

  // whether archives should be parsed to show conflicts and contents
  //
  bool archiveParsing() const;
  void setArchiveParsing(bool b);

  // whether the user wants to check for updates
  //
  bool checkForUpdates() const;
  void setCheckForUpdates(bool b);

  // whether the user wants to upgrade to pre-releases
  //
  bool usePrereleases() const;
  void setUsePrereleases(bool b);

  // whether profiles should default to local INIs
  //
  bool profileLocalInis() const;
  void setProfileLocalInis(bool b);

  // whether profiles should default to local saves
  //
  bool profileLocalSaves() const;
  void setProfileLocalSaves(bool b);

  // whether profiles should default to automatic archive invalidation
  //
  bool profileArchiveInvalidation() const;
  void setProfileArchiveInvalidation(bool b);

  // whether to use spascreen or not
  //
  bool useSplash() const;
  void setUseSplash(bool b);

  // number of threads to use when refreshing
  //
  std::size_t refreshThreadCount() const;
  void setRefreshThreadCount(std::size_t n) const;

  GameSettings& game();
  const GameSettings& game() const;

  GeometrySettings& geometry();
  const GeometrySettings& geometry() const;

  WidgetSettings& widgets();
  const WidgetSettings& widgets() const;

  ColorSettings& colors();
  const ColorSettings& colors() const;

  PluginSettings& plugins();
  const PluginSettings& plugins() const;

  PathSettings& paths();
  const PathSettings& paths() const;

  NetworkSettings& network();
  const NetworkSettings& network() const;

  NexusSettings& nexus();
  const NexusSettings& nexus() const;

  SteamSettings& steam();
  const SteamSettings& steam() const;

  InterfaceSettings& interface();
  const InterfaceSettings& interface() const;

  DiagnosticsSettings& diagnostics();
  const DiagnosticsSettings& diagnostics() const;

  // makes sure the ini file is written to disk
  //
  QSettings::Status sync() const;

  // last status of the ini file
  //
  QSettings::Status iniStatus() const;

  void dump() const;

public slots:
  // this slot is connected to by various parts of MO
  //
  void managedGameChanged(MOBase::IPluginGame const* gamePlugin);

signals:
  // these are fired from outside the settings, mostly by the settings dialog
  //
  void languageChanged(const QString& newLanguage);
  void themeChanged(const QString& themeIdentifier);

private:
  static Settings* s_Instance;
  mutable QSettings m_Settings;

  GameSettings m_Game;
  GeometrySettings m_Geometry;
  WidgetSettings m_Widgets;
  ColorSettings m_Colors;
  PluginSettings m_Plugins;
  PathSettings m_Paths;
  NetworkSettings m_Network;
  NexusSettings m_Nexus;
  SteamSettings m_Steam;
  InterfaceSettings m_Interface;
  DiagnosticsSettings m_Diagnostics;
};

// manages global settings in the registry
//
class GlobalSettings
{
public:
  // migrates the old settings from the Tannin key to the new one
  static void updateRegistryKey();

  static QString currentInstance();
  static void setCurrentInstance(const QString& s);

  static bool hideCreateInstanceIntro();
  static void setHideCreateInstanceIntro(bool b);

  static bool hideTutorialQuestion();
  static void setHideTutorialQuestion(bool b);

  static bool hideCategoryReminder();
  static void setHideCategoryReminder(bool b);

  static bool hideAssignCategoriesQuestion();
  static void setHideAssignCategoriesQuestion(bool b);

  // if the key exists from the credentials store, puts it in `apiKey` and
  // returns true; otherwise, returns false and leaves `apiKey` untouched
  //
  static bool nexusApiKey(QString& apiKey);

  // sets the api key in the credentials store, removes it if empty; returns
  // false on errors
  //
  static bool setNexusApiKey(const QString& apiKey);

  // removes the api key from the credentials store; returns false on errors
  //
  static bool clearNexusApiKey();

  // returns whether an API key is currently stored
  //
  static bool hasNexusApiKey();

  // resets anything that the user can disable
  static void resetDialogs();

private:
  static QSettings settings();
};

// helper class that calls restoreGeometry() in the constructor and
// saveGeometry() in the destructor
//
template <class W>
class GeometrySaver
{
public:
  GeometrySaver(Settings& s, W* w) : m_settings(s), m_widget(w)
  {
    m_settings.geometry().restoreGeometry(m_widget);
  }

  ~GeometrySaver() { m_settings.geometry().saveGeometry(m_widget); }

private:
  Settings& m_settings;
  W* m_widget;
};

#endif  // SETTINGS_H
