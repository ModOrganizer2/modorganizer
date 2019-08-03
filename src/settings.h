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
#include <log.h>

namespace MOBase {
  class IPlugin;
  class IPluginGame;
}

class PluginContainer;
struct ServerInfo;


class GeometrySettings
{
public:
  GeometrySettings(QSettings& s);

  std::optional<QByteArray> getMainWindow() const;
  std::optional<QByteArray> getMainWindowState() const;
  std::optional<QSize> getToolbarSize() const;
  std::optional<Qt::ToolButtonStyle> getToolbarButtonStyle() const;
  std::optional<bool> getMenubarVisible() const;
  std::optional<bool> getStatusbarVisible() const;
  std::optional<QByteArray> getMainSplitterState() const;
  std::optional<bool> getFiltersVisible() const;

  std::optional<QByteArray> getExecutablesDialog() const;
  void setExecutablesDialog(const QByteArray& v);

  std::optional<QByteArray> getProfilesDialog() const;
  void setProfilesDialog(const QByteArray& v);

  std::optional<QByteArray> getOverwriteDialog() const;
  void setOverwriteDialog(const QByteArray& v);

  std::optional<QByteArray> getModInfoDialog() const;
  void setModInfoDialog(const QByteArray& v) const;

  QStringList getModInfoTabOrder() const;
  void setModInfoTabOrder(const QString& names);

  std::optional<int> getMainWindowMonitor() const;
  void setDockSize(const QString& name, int size);

  std::optional<int> getDockSize(const QString& name) const;

  std::optional<bool> isCategoryListVisible() const;

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

  void processUpdates(
    const QVersionNumber& currentVersion, const QVersionNumber& lastVersion);

  QString getFilename() const;

  /**
   * unregister all plugins from settings
   */
  void clearPlugins();

  /**
   * @brief register plugin to be configurable
   * @param plugin the plugin to register
   * @return true if the plugin may be registered, false if it is blacklisted
   */
  void registerPlugin(MOBase::IPlugin *plugin);

  /**
   * set up the settings for the specified plugins
   **/
  void addPluginSettings(const std::vector<MOBase::IPlugin*> &plugins);

  /**
   * @return true if the user wants unchecked plugins (esp, esm) should be hidden from
   *         the virtual dat adirectory
   **/
  bool hideUncheckedPlugins() const;

  /**
   * @return true if files of the core game are forced-enabled so the user can't accidentally disable them
   */
  bool forceEnableCoreFiles() const;

  /**
   * @return true if the GUI should be locked when running executables
   */
  bool lockGUI() const;

  /**
   * @brief register download speed
   * @param url complete download url
   * @param bytesPerSecond download size in bytes per second
   */
  void setDownloadSpeed(const QString &serverName, int bytesPerSecond);

  /**
   * the steam appid is assigned by the steam platform to each product sold there.
   * The appid may differ between different versions of a game so it may be impossible
   * for Mod Organizer to automatically recognize it, though usually it does
   * @return the steam appid for the game
   **/
  QString getSteamAppID() const;

  /**
   * retrieves the base directory under which the other directories usually
   * reside
   */
  QString getBaseDirectory() const;

  /**
   * retrieve the directory where downloads are stored (with native separators)
   **/
  QString getDownloadDirectory(bool resolve = true) const;

  /**
   * retrieve a sorted list of preferred servers
   */
  std::map<QString, int> getPreferredServers();

  /**
   * retrieve the directory where mods are stored (with native separators)
   **/
  QString getModDirectory(bool resolve = true) const;

  /**
   * retrieve the directory where the web cache is stored (with native separators)
   **/
  QString getCacheDirectory(bool resolve = true) const;

  /**
   * retrieve the directory where the managed game is stored (with native separators)
   **/
  std::optional<QString> getManagedGameDirectory() const;
  void setManagedGameDirectory(const QString& path);

  std::optional<QString> getManagedGameName() const;
  void setManagedGameName(const QString& name);

  std::optional<QString> getManagedGameEdition() const;
  void setManagedGameEdition(const QString& name);

  std::optional<QString> getSelectedProfileName() const;
  void setSelectedProfileName(const QString& name);

  std::optional<QString> getStyleName() const;
  void setStyleName(const QString& name);

  std::optional<int> getSelectedExecutable() const;
  std::optional<bool> getUseProxy() const;

  std::optional<QVersionNumber> getVersion() const;
  bool getFirstStart() const;

  std::optional<QColor> getPreviousSeparatorColor() const;
  void setPreviousSeparatorColor(const QColor& c) const;
  void removePreviousSeparatorColor();

  GeometrySettings& geometry();
  const GeometrySettings& geometry() const;


  /**
   * retrieve the directory where profiles stored (with native separators)
   **/
  QString getProfileDirectory(bool resolve = true) const;

  /**
   * retrieve the directory were new files are stored that can't be assigned
   * to a mod (with native separators)
   */
  QString getOverwriteDirectory(bool resolve = true) const;

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
  bool getNexusApiKey(QString &apiKey) const;

  /**
  * @brief set the nexus login information
  *
  * @param username username
  * @param password password
  */
  bool setNexusApiKey(const QString& apiKey);

  /**
  * @brief clears the nexus login information
  */
  bool clearNexusApiKey();

  /**
   * @brief returns whether an API key is currently stored
   */
  bool hasNexusApiKey() const;

  /**
   * @brief retrieve the login information for steam
   *
   * @param username (out) receives the user name for nexus
   * @param password (out) received the password for nexus
   * @return true if a username has been specified, false otherwise
   **/
  bool getSteamLogin(QString &username, QString &password) const;

  /**
   * @return true if the user disabled internet features
   */
  bool offlineMode() const;

  /**
   * @return true if the user chose compact downloads
   */
  bool compactDownloads() const;

  /**
   * @return true if the user chose meta downloads
   */
  bool metaDownloads() const;

  /**
   * @return the configured log level
   */
  MOBase::log::Levels logLevel() const;

  /**
   * sets the log level setting
   */
  void setLogLevel(MOBase::log::Levels level);

  /**
  * @return the configured crash dumps type
  */
  int crashDumpsType() const;

  /**
  * @return the configured crash dumps max
  */
  int crashDumpsMax() const;

  QColor modlistOverwrittenLooseColor() const;

  QColor modlistOverwritingLooseColor() const;

  QColor modlistOverwrittenArchiveColor() const;

  QColor modlistOverwritingArchiveColor() const;

  QColor modlistContainsPluginColor() const;

  QColor pluginListContainedColor() const;

  QString executablesBlacklist() const;

  /**
   * @brief set the steam login information
   *
   * @param username username
   * @param password password
   */
  void setSteamLogin(QString username, QString password);

  /**
   * @return the load mechanism to be used
   **/
  LoadMechanism::EMechanism getLoadMechanism() const;

  /**
   * @brief activate the load mechanism selected by the user
   **/
  void setupLoadMechanism();

  /**
   * @return true if the user configured the use of a network proxy
   */
  bool useProxy() const;

  /**
   * @return true if endorsement integration is enabled
   */
  bool endorsementIntegration() const;

  /**
   * @return true if the API counter should be hidden
   */
  bool hideAPICounter() const;

  /**
   * @return true if the user wants to see non-official plugins installed outside MO in his mod list
   */
  bool displayForeign() const;

  /**
   * @brief sets the new motd hash
   **/
  void setMotDHash(uint hash);

  /**
  * @return true if the user wants to have archives being parsed to show conflicts and contents
  */
  bool archiveParsing() const;

  /**
   * @return hash of the last displayed message of the day
   **/
  uint getMotDHash() const;

  /**
   * @brief allows direct access to the wrapped QSettings object
   * @return the wrapped QSettings object
   */
  QSettings &directInterface() { return m_Settings; }
  const QSettings &directInterface() const { return m_Settings; }

  /**
   * @brief retrieve a setting for one of the installed plugins
   * @param pluginName name of the plugin
   * @param key name of the setting to retrieve
   * @return the requested value as a QVariant
   * @note an invalid QVariant is returned if the the plugin/setting is not declared
   */
  QVariant pluginSetting(const QString &pluginName, const QString &key) const;

  /**
   * @brief set a setting for one of the installed mods
   * @param pluginName name of the plugin
   * @param key name of the setting to change
   * @param value the new value to set
   * @throw an exception is thrown if pluginName is invalid
   */
  void setPluginSetting(const QString &pluginName, const QString &key, const QVariant &value);

  /**
   * @brief retrieve a persistent value for a plugin
   * @param pluginName name of the plugin to store data for
   * @param key id of the value to retrieve
   * @param def default value to return if the value is not set
   * @return the requested value
   */
  QVariant pluginPersistent(const QString &pluginName, const QString &key, const QVariant &def) const;

  /**
   * @brief set a persistent value for a plugin
   * @param pluginName name of the plugin to store data for
   * @param key id of the value to retrieve
   * @param value value to set
   * @throw an exception is thrown if pluginName is invalid
   */
  void setPluginPersistent(const QString &pluginName, const QString &key, const QVariant &value, bool sync);

  /**
   * @return short code of the configured language (corresponding to the translation files)
   */
  QString language();

  /**
   * @brief updates the list of known servers
   * @param list of servers from a recent query
   */
  void updateServers(const QList<ServerInfo> &servers);

  /**
   * @brief add a plugin that is to be blacklisted
   * @param fileName name of the plugin to blacklist
   */
  void addBlacklistPlugin(const QString &fileName);

  /**
   * @brief test if a plugin is blacklisted and shouldn't be loaded
   * @param fileName name of the plugin
   * @return true if the file is blacklisted
   */
  bool pluginBlacklisted(const QString &fileName) const;

  /**
   * @return all loaded MO plugins
   */
  std::vector<MOBase::IPlugin*> plugins() const { return m_Plugins; }

  bool usePrereleases() const;

  /**
   * @brief register MO as the handler for nxm links
   * @param force set to true to enforce the registration dialog to show up,
   *              even if the user said earlier not to
   */
  void registerAsNXMHandler(bool force);

  /**
   * @brief color the scrollbar of the mod list for custom separator colors?
   * @return the state of the setting
   */
  bool colorSeparatorScrollbar() const;

  static QColor getIdealTextColor(const QColor&  rBackgroundColor);

  MOBase::IPluginGame const *gamePlugin() { return m_GamePlugin; }
  const LoadMechanism& loadMechanism() const { return m_LoadMechanism; }

  QSettings::Status sync() const;

  void dump() const;

  // temp
  QMap<QString, QVariantMap> m_PluginSettings;
  QMap<QString, QVariantMap> m_PluginDescriptions;
  QSet<QString> m_PluginBlacklist;
  void writePluginBlacklist();

public slots:
  void managedGameChanged(MOBase::IPluginGame const *gamePlugin);

signals:
  void languageChanged(const QString &newLanguage);
  void styleChanged(const QString &newStyle);

private:
  static Settings *s_Instance;
  MOBase::IPluginGame const *m_GamePlugin;
  mutable QSettings m_Settings;
  GeometrySettings m_Geometry;
  LoadMechanism m_LoadMechanism;
  std::vector<MOBase::IPlugin*> m_Plugins;

  static bool obfuscate(const QString key, const QString data);
  static QString deObfuscate(const QString key);

  void readPluginBlacklist();
  QString getConfigurablePath(const QString &key, const QString &def, bool resolve) const;
};

#endif // SETTINGS_H
