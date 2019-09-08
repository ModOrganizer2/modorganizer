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

#include "loadmechanism.h"
#include <questionboxmemory.h>
#include <log.h>
#include <usvfsparameters.h>

#ifdef interface
  #undef interface
#endif

namespace MOBase {
  class IPlugin;
  class IPluginGame;
}

class QSplitter;

class ServerList;
class Settings;
class ExpanderWidget;


// helper class that calls restoreGeometry() in the constructor and
// saveGeometry() in the destructor
//
class GeometrySaver
{
public:
  GeometrySaver(Settings& s, QDialog* dialog);
  ~GeometrySaver();

private:
  Settings& m_settings;
  QDialog* m_dialog;
};


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

  // load mechanism type
  //
  LoadMechanism::EMechanism loadMechanismType() const;
  void setLoadMechanism(LoadMechanism::EMechanism m);

  // load mechanism object
  //
  const LoadMechanism& loadMechanism() const;
  LoadMechanism& loadMechanism();

  // whether the user wants unchecked plugins (esp, esm) to be hidden from
  // the virtual data directory
  //
  bool hideUncheckedPlugins() const;
  void setHideUncheckedPlugins(bool b);

private:
  QSettings& m_Settings;
  const MOBase::IPluginGame* m_GamePlugin;
  LoadMechanism m_LoadMechanism;
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


  void saveGeometry(const QWidget* w);
  bool restoreGeometry(QWidget* w) const;

  void saveState(const QMainWindow* window);
  bool restoreState(QMainWindow* window) const;

  void saveState(const QHeaderView* header);
  bool restoreState(QHeaderView* header) const;

  void saveState(const QSplitter* splitter);
  bool restoreState(QSplitter* splitter) const;

  void saveState(const ExpanderWidget* expander);
  bool restoreState(ExpanderWidget* expander) const;

  void saveVisibility(const QWidget* w);
  bool restoreVisibility(QWidget* w, std::optional<bool> def={}) const;

  void saveToolbars(const QMainWindow* w);
  void restoreToolbars(QMainWindow* w) const;

  void saveDocks(const QMainWindow* w);
  void restoreDocks(QMainWindow* w) const;

  // this should be a generic "tab order" setting, but it only happens for the
  // mod info dialog right now
  //
  QStringList modInfoTabOrder() const;
  void setModInfoTabOrder(const QString& names);

  // assumes the given widget is a top-level
  //
  void centerOnMainWindowMonitor(QWidget* w);

  // saves the monitor number of the given window
  //
  void saveMainWindowMonitor(const QMainWindow* w);

private:
  QSettings& m_Settings;
  bool m_Reset;
};


// widget settings that should stay valid regardless of UI changes or when users
// change display settings (resolution, monitors, etc.); see GeometrySettings
// for the counterpart
//
class WidgetSettings
{
public:
  WidgetSettings(QSettings& s);

  // selected index for a combobox
  //
  std::optional<int> index(const QComboBox* cb) const;
  void saveIndex(const QComboBox* cb);
  void restoreIndex(QComboBox* cb, std::optional<int> def={}) const;

  // selected tab index for a tab widget
  //
  std::optional<int> index(const QTabWidget* w) const;
  void saveIndex(const QTabWidget* w);
  void restoreIndex(QTabWidget* w, std::optional<int> def={}) const;

  // check state for a checkable button
  //
  std::optional<bool> checked(const QAbstractButton* w) const;
  void saveChecked(const QAbstractButton* w);
  void restoreChecked(QAbstractButton* w, std::optional<bool> def={}) const;

  // returns the remembered button for a question dialog, or NoButton if the
  // user hasn't saved the choice
  //
  MOBase::QuestionBoxMemory::Button questionButton(
    const QString& windowName, const QString& filename) const;

  // sets the button to be remembered for the given window
  //
  void setQuestionWindowButton(
    const QString& windowName, MOBase::QuestionBoxMemory::Button button);

  // sets the button to be remembered for the given file
  //
  void setQuestionFileButton(
    const QString& windowName, const QString& filename,
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
  void setPluginListContained(const QColor& c) ;

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
class PluginSettings
{
public:
  PluginSettings(QSettings& settings);


  // forgets all the plugins
  //
  void clearPlugins();

  // adds the given plugin to the list and loads all of its settings
  //
  void registerPlugin(MOBase::IPlugin *plugin);

  // returns all the registered plugins
  //
  std::vector<MOBase::IPlugin*> plugins() const;


  // returns the plugin setting for the given key
  //
  QVariant setting(const QString &pluginName, const QString &key) const;

  // sets the plugin setting for the given key
  //
  void setSetting(const QString &pluginName, const QString &key, const QVariant &value);

  // returns all settings
  //
  QVariantMap settings(const QString &pluginName) const;

  // overwrites all settings
  //
  void setSettings(const QString &pluginName, const QVariantMap& map);

  // returns all descriptions
  //
  QVariantMap descriptions(const QString &pluginName) const;

  // overwrites all descriptions
  //
  void setDescriptions(const QString &pluginName, const QVariantMap& map);


  // ?
  QVariant persistent(const QString &pluginName, const QString &key, const QVariant &def) const;
  void setPersistent(const QString &pluginName, const QString &key, const QVariant &value, bool sync);


  // adds the given plugin to the blacklist
  //
  void addBlacklist(const QString &fileName);

  // returns whether the given plugin is blacklisted
  //
  bool blacklisted(const QString &fileName) const;

  // overwrites the whole blacklist
  //
  void setBlacklist(const QStringList& pluginNames);

  // returns the blacklist
  //
  const QSet<QString>& blacklist() const;


  // commits all the settings to the ini
  //
  void save();

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


class PathSettings
{
public:
  PathSettings(QSettings& settings);

  QString base() const;
  QString downloads(bool resolve = true) const;
  QString mods(bool resolve = true) const;
  QString cache(bool resolve = true) const;
  QString profiles(bool resolve = true) const;
  QString overwrite(bool resolve = true) const;

  void setBase(const QString& path);
  void setDownloads(const QString& path);
  void setMods(const QString& path);
  void setCache(const QString& path);
  void setProfiles(const QString& path);
  void setOverwrite(const QString& path);

  std::map<QString, QString> recent() const;
  void setRecent(const std::map<QString, QString>& map);

private:
  QSettings& m_Settings;

  QString getConfigurablePath(const QString &key, const QString &def, bool resolve) const;
  void setConfigurablePath(const QString &key, const QString& path);
};


class NetworkSettings
{
public:
  NetworkSettings(QSettings& settings);

  /**
  * @return true if the user disabled internet features
  */
  bool offlineMode() const;
  void setOfflineMode(bool b);

  /**
  * @return true if the user configured the use of a network proxy
  */
  bool useProxy() const;
  void setUseProxy(bool b);

  void setDownloadSpeed(const QString &serverName, int bytesPerSecond);
  ServerList servers() const;
  void updateServers(ServerList servers);

  void dump() const;

private:
  QSettings& m_Settings;

  ServerList serversFromOldMap() const;
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

  /**
  * @return true if the user has set up automatic login to nexus
  **/
  bool automaticLoginEnabled() const;

  /**
  * @brief retrieve the login information for nexus
  *
  * @param username (out) receives the user name for nexus
  * @param password (out) received the password for nexus
  * @return true if automatic login is active, false otherwise
  **/
  bool apiKey(QString &apiKey) const;

  /**
  * @brief set the nexus login information
  *
  * @param username username
  * @param password password
  */
  bool setApiKey(const QString& apiKey);

  /**
  * @brief clears the nexus login information
  */
  bool clearApiKey();

  /**
  * @brief returns whether an API key is currently stored
  */
  bool hasApiKey() const;

  /**
  * @return true if endorsement integration is enabled
  */
  bool endorsementIntegration() const;
  void setEndorsementIntegration(bool b) const;

  EndorsementState endorsementState() const;
  void setEndorsementState(EndorsementState s);

  /**
  * @brief register MO as the handler for nxm links
  * @param force set to true to enforce the registration dialog to show up,
  *              even if the user said earlier not to
  */
  void registerAsNXMHandler(bool force);

private:
  Settings& m_Parent;
  QSettings& m_Settings;
};


class SteamSettings
{
public:
  SteamSettings(Settings& parent, QSettings& settings);

  /**
  * the steam appid is assigned by the steam platform to each product sold there.
  * The appid may differ between different versions of a game so it may be impossible
  * for Mod Organizer to automatically recognize it, though usually it does
  * @return the steam appid for the game
  **/
  QString appID() const;
  void setAppID(const QString& id);

  /**
  * @brief retrieve the login information for steam
  *
  * @param username (out) receives the user name for nexus
  * @param password (out) received the password for nexus
  * @return true if a username has been specified, false otherwise
  **/
  bool login(QString &username, QString &password) const;

  /**
  * @brief set the steam login information
  *
  * @param username username
  * @param password password
  */
  void setLogin(QString username, QString password);

private:
  Settings& m_Parent;
  QSettings& m_Settings;
};


class InterfaceSettings
{
public:
  InterfaceSettings(QSettings& settings);

  /**
  * @return true if the GUI should be locked when running executables
  */
  bool lockGUI() const;
  void setLockGUI(bool b);

  std::optional<QString> styleName() const;
  void setStyleName(const QString& name);

  /**
  * @return true if the user chose compact downloads
  */
  bool compactDownloads() const;
  void setCompactDownloads(bool b);

  /**
  * @return true if the user chose meta downloads
  */
  bool metaDownloads() const;
  void setMetaDownloads(bool b);

  /**
  * @return true if the API counter should be hidden
  */
  bool hideAPICounter() const;
  void setHideAPICounter(bool b);

  /**
  * @return true if the user wants to see non-official plugins installed outside MO in his mod list
  */
  bool displayForeign() const;
  void setDisplayForeign(bool b);

  /**
  * @return short code of the configured language (corresponding to the translation files)
  */
  QString language();
  void setLanguage(const QString& name);

  bool isTutorialCompleted(const QString& windowName) const;
  void setTutorialCompleted(const QString& windowName, bool b=true);

private:
  QSettings& m_Settings;
};


class DiagnosticsSettings
{
public:
  DiagnosticsSettings(QSettings& settings);

  MOBase::log::Levels logLevel() const;
  void setLogLevel(MOBase::log::Levels level);

  CrashDumpsType crashDumpsType() const;
  void setCrashDumpsType(CrashDumpsType type);

  int crashDumpsMax() const;
  void setCrashDumpsMax(int n);

private:
  QSettings& m_Settings;
};


/**
 * manages the settings for Mod Organizer. The settings are not cached
 * inside the class but read/written directly from/to disc
 **/
class Settings : public QObject
{
  Q_OBJECT;

public:
  Settings(const QString& path);
  ~Settings();

  static Settings &instance();

  QString filename() const;

  std::optional<QVersionNumber> version() const;
  void processUpdates(const QVersionNumber& current, const QVersionNumber& last);

  bool firstStart() const;
  void setFirstStart(bool b);

  std::vector<std::map<QString, QVariant>> executables() const;
  void setExecutables(const std::vector<std::map<QString, QVariant>>& v);

  bool keepBackupOnInstall() const;
  void setKeepBackupOnInstall(bool b);

  QString executablesBlacklist() const;
  void setExecutablesBlacklist(const QString& s);

  /**
   * @brief sets the new motd hash
   **/
  unsigned int motdHash() const;
  void setMotdHash(unsigned int hash);

  /**
  * @return true if the user wants to have archives being parsed to show conflicts and contents
  */
  bool archiveParsing() const;
  void setArchiveParsing(bool b);

  bool usePrereleases() const;
  void setUsePrereleases(bool b);


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


  QSettings::Status sync() const;
  void dump() const;

public slots:
  void managedGameChanged(MOBase::IPluginGame const *gamePlugin);

signals:
  void languageChanged(const QString &newLanguage);
  void styleChanged(const QString &newStyle);

private:
  static Settings *s_Instance;
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

#endif // SETTINGS_H
