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

namespace MOBase {
  class IPlugin;
  class IPluginGame;
}

class QSplitter;

class PluginContainer;
class ServerInfo;
class ServerList;
class Settings;
class ExpanderWidget;

class GeometrySaver
{
public:
  GeometrySaver(Settings& s, QDialog* dialog);
  ~GeometrySaver();

private:
  Settings& m_settings;
  QDialog* m_dialog;
};


class GeometrySettings
{
public:
  GeometrySettings(QSettings& s);

  void requestReset();
  void resetIfNeeded();


  void saveGeometry(const QWidget* w);
  bool restoreGeometry(QWidget* w) const;

  void saveState(const QMainWindow* window);
  bool restoreState(QMainWindow* window) const;

  void saveState(const QHeaderView* header);
  bool restoreState(QHeaderView* header) const;

  void saveState(const QToolBar* toolbar);
  bool restoreState(QToolBar* toolbar) const;

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

  QStringList getModInfoTabOrder() const;
  void setModInfoTabOrder(const QString& names);

  void centerOnMainWindowMonitor(QWidget* w);
  void saveMainWindowMonitor(const QMainWindow* w);

private:
  QSettings& m_Settings;
  bool m_Reset;
};


class ColorSettings
{
public:
  ColorSettings(QSettings& s);

  void setCrashDumpsMax(int i) const;

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

private:
  QSettings& m_Settings;
};


class PluginSettings
{
public:
  PluginSettings(QSettings& settings);

  void clearPlugins();
  void registerPlugin(MOBase::IPlugin *plugin);
  void addPluginSettings(const std::vector<MOBase::IPlugin*> &plugins);

  QVariant pluginSetting(const QString &pluginName, const QString &key) const;
  void setPluginSetting(const QString &pluginName, const QString &key, const QVariant &value);
  QVariant pluginPersistent(const QString &pluginName, const QString &key, const QVariant &def) const;
  void setPluginPersistent(const QString &pluginName, const QString &key, const QVariant &value, bool sync);
  void addBlacklistPlugin(const QString &fileName);
  bool pluginBlacklisted(const QString &fileName) const;
  void setPluginBlacklist(const QStringList& pluginNames);
  std::vector<MOBase::IPlugin*> plugins() const { return m_Plugins; }

  QVariantMap pluginSettings(const QString &pluginName) const;
  void setPluginSettings(const QString &pluginName, const QVariantMap& map);

  QVariantMap pluginDescriptions(const QString &pluginName) const;
  void pluginDescriptions(const QString &pluginName, const QVariantMap& map);

  const QSet<QString>& pluginBlacklist() const;

  void save();

private:
  QSettings& m_Settings;
  std::vector<MOBase::IPlugin*> m_Plugins;
  QMap<QString, QVariantMap> m_PluginSettings;
  QMap<QString, QVariantMap> m_PluginDescriptions;
  QSet<QString> m_PluginBlacklist;

  void readPluginBlacklist();
  void writePluginBlacklist();
};


enum class EndorsementState
{
  Accepted = 1,
  Refused,
  NoDecision
};

EndorsementState endorsementStateFromString(const QString& s);
QString toString(EndorsementState s);


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
   * @return true if the user wants unchecked plugins (esp, esm) should be hidden from
   *         the virtual dat adirectory
   **/
  bool hideUncheckedPlugins() const;
  void setHideUncheckedPlugins(bool b);

  /**
   * @return true if files of the core game are forced-enabled so the user can't accidentally disable them
   */
  bool forceEnableCoreFiles() const;
  void setForceEnableCoreFiles(bool b);

  /**
   * @return true if the GUI should be locked when running executables
   */
  bool lockGUI() const;
  void setLockGUI(bool b);

  /**
   * the steam appid is assigned by the steam platform to each product sold there.
   * The appid may differ between different versions of a game so it may be impossible
   * for Mod Organizer to automatically recognize it, though usually it does
   * @return the steam appid for the game
   **/
  QString getSteamAppID() const;
  void setSteamAppID(const QString& id);

  QString getBaseDirectory() const;
  QString getDownloadDirectory(bool resolve = true) const;
  QString getModDirectory(bool resolve = true) const;
  QString getCacheDirectory(bool resolve = true) const;
  QString getProfileDirectory(bool resolve = true) const;
  QString getOverwriteDirectory(bool resolve = true) const;

  void setBaseDirectory(const QString& path);
  void setDownloadDirectory(const QString& path);
  void setModDirectory(const QString& path);
  void setCacheDirectory(const QString& path);
  void setProfileDirectory(const QString& path);
  void setOverwriteDirectory(const QString& path);

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

  std::optional<bool> getUseProxy() const;

  std::optional<QVersionNumber> getVersion() const;

  bool getFirstStart() const;
  void setFirstStart(bool b);

  std::optional<QColor> getPreviousSeparatorColor() const;
  void setPreviousSeparatorColor(const QColor& c) const;
  void removePreviousSeparatorColor();

  std::map<QString, QString> getRecentDirectories() const;
  void setRecentDirectories(const std::map<QString, QString>& map);

  std::vector<std::map<QString, QVariant>> getExecutables() const;
  void setExecutables(const std::vector<std::map<QString, QVariant>>& v);

  bool isTutorialCompleted(const QString& windowName) const;
  void setTutorialCompleted(const QString& windowName, bool b=true);

  bool keepBackupOnInstall() const;
  void setKeepBackupOnInstall(bool b);

  MOBase::QuestionBoxMemory::Button getQuestionButton(
    const QString& windowName, const QString& filename) const;

  void setQuestionWindowButton(
    const QString& windowName, MOBase::QuestionBoxMemory::Button button);

  void setQuestionFileButton(
    const QString& windowName, const QString& filename,
    MOBase::QuestionBoxMemory::Button choice);

  void resetQuestionButtons();

  std::optional<int> getIndex(const QComboBox* cb) const;
  void saveIndex(const QComboBox* cb);
  void restoreIndex(QComboBox* cb, std::optional<int> def={}) const;

  std::optional<int> getIndex(const QTabWidget* w) const;
  void saveIndex(const QTabWidget* w);
  void restoreIndex(QTabWidget* w, std::optional<int> def={}) const;

  std::optional<bool> getChecked(const QAbstractButton* w) const;
  void saveChecked(const QAbstractButton* w);
  void restoreChecked(QAbstractButton* w, std::optional<bool> def={}) const;

  GeometrySettings& geometry();
  const GeometrySettings& geometry() const;

  ColorSettings& colors();
  const ColorSettings& colors() const;

  PluginSettings& plugins();
  const PluginSettings& plugins() const;


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
  void setOfflineMode(bool b);

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

  MOBase::log::Levels logLevel() const;
  void setLogLevel(MOBase::log::Levels level);

  CrashDumpsType crashDumpsType() const;
  void setCrashDumpsType(CrashDumpsType type);

  int crashDumpsMax() const;
  void setCrashDumpsMax(int n);

  QString executablesBlacklist() const;
  void setExecutablesBlacklist(const QString& s);

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
  void setLoadMechanism(LoadMechanism::EMechanism m);

  /**
   * @brief activate the load mechanism selected by the user
   **/
  void setupLoadMechanism();

  /**
   * @return true if the user configured the use of a network proxy
   */
  bool useProxy() const;
  void setUseProxy(bool b);

  /**
   * @return true if endorsement integration is enabled
   */
  bool endorsementIntegration() const;
  void setEndorsementIntegration(bool b) const;

  EndorsementState endorsementState() const;
  void setEndorsementState(EndorsementState s);
  void setEndorsementState(const QString& s);

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
   * @brief sets the new motd hash
   **/
  void setMotDHash(uint hash);

  /**
  * @return true if the user wants to have archives being parsed to show conflicts and contents
  */
  bool archiveParsing() const;
  void setArchiveParsing(bool b);

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
   * @return short code of the configured language (corresponding to the translation files)
   */
  QString language();
  void setLanguage(const QString& name);

  void setDownloadSpeed(const QString &serverName, int bytesPerSecond);
  ServerList getServers() const;
  ServerList getServersFromOldMap() const;
  void updateServers(ServerList servers);

  bool usePrereleases() const;
  void setUsePrereleases(bool b);

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
  void setColorSeparatorScrollbar(bool b);

  static QColor getIdealTextColor(const QColor&  rBackgroundColor);

  MOBase::IPluginGame const *gamePlugin() { return m_GamePlugin; }
  const LoadMechanism& loadMechanism() const { return m_LoadMechanism; }

  QSettings::Status sync() const;

  void dump() const;

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
  ColorSettings m_Colors;
  PluginSettings m_Plugins;
  LoadMechanism m_LoadMechanism;

  static bool obfuscate(const QString key, const QString data);
  static QString deObfuscate(const QString key);

  QString getConfigurablePath(const QString &key, const QString &def, bool resolve) const;
  void setConfigurablePath(const QString &key, const QString& path);
};

#endif // SETTINGS_H
