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

#include "settings.h"
#include "serverinfo.h"
#include "executableslist.h"
#include "appconfig.h"
#include <utility.h>
#include <iplugingame.h>
#include <usvfsparameters.h>

using namespace MOBase;

template <class T>
std::optional<T> getOptional(
  const QSettings& s, const QString& name, std::optional<T> def={})
{
  if (s.contains(name)) {
    return s.value(name).value<T>();
  }

  return def;
}


QString widgetNameWithTopLevel(const QWidget* widget)
{
  QStringList components;

  auto* tl = widget->window();

  if (tl == widget) {
    // this is a top level widget, such as a dialog
    components.push_back(widget->objectName());
  } else {
    // this is a widget
    const auto toplevelName = tl->objectName();
    if (!toplevelName.isEmpty()) {
      components.push_back(toplevelName);
    }

    const auto widgetName = widget->objectName();
    if (!widgetName.isEmpty()) {
      components.push_back(widgetName);
    }
  }

  if (components.isEmpty()) {
    // can't do much
    return "unknown_widget";
  }

  return components.join("_");
}

QString widgetName(const QMainWindow* w)
{
  return w->objectName();
}

QString widgetName(const QHeaderView* w)
{
  return widgetNameWithTopLevel(w->parentWidget());
}

QString widgetName(const QWidget* w)
{
  return widgetNameWithTopLevel(w);
}

template <class Widget>
QString geoSettingName(const Widget* widget)
{
  return "geometry/" + widgetName(widget) + "_geometry";
}

template <class Widget>
QString stateSettingName(const Widget* widget)
{
  return "geometry/" + widgetName(widget) + "_state";
}

template <class Widget>
QString visibilitySettingName(const Widget* widget)
{
  return "geometry/" + widgetName(widget) + "_visibility";
}

QString dockSettingName(const QDockWidget* dock)
{
  return "geometry/MainWindow_docks_" + dock->objectName() + "_size";
}

QString indexSettingName(const QWidget* widget)
{
  return widgetNameWithTopLevel(widget) + "_index";
}


Settings *Settings::s_Instance = nullptr;

Settings::Settings(const QString& path)
  : m_Settings(path, QSettings::IniFormat), m_Geometry(m_Settings)
{
  if (s_Instance != nullptr) {
    throw std::runtime_error("second instance of \"Settings\" created");
  } else {
    s_Instance = this;
  }
}

Settings::~Settings()
{
  s_Instance = nullptr;
}

Settings &Settings::instance()
{
  if (s_Instance == nullptr) {
    throw std::runtime_error("no instance of \"Settings\"");
  }
  return *s_Instance;
}

void Settings::processUpdates(
  const QVersionNumber& currentVersion, const QVersionNumber& lastVersion)
{
  if (getFirstStart()) {
    return;
  }

  if (lastVersion < QVersionNumber(2, 2, 0)) {
    m_Settings.beginGroup("Settings");
    m_Settings.remove("steam_password");
    m_Settings.remove("nexus_username");
    m_Settings.remove("nexus_password");
    m_Settings.remove("nexus_login");
    m_Settings.remove("nexus_api_key");
    m_Settings.remove("ask_for_nexuspw");
    m_Settings.remove("nmm_version");
    m_Settings.endGroup();

    m_Settings.beginGroup("Servers");
    m_Settings.remove("");
    m_Settings.endGroup();
  }

  if (lastVersion < QVersionNumber(2, 2, 1)) {
    m_Settings.remove("mod_info_tabs");
    m_Settings.remove("mod_info_conflict_expanders");
    m_Settings.remove("mod_info_conflicts");
    m_Settings.remove("mod_info_advanced_conflicts");
    m_Settings.remove("mod_info_conflicts_overwrite");
    m_Settings.remove("mod_info_conflicts_noconflict");
    m_Settings.remove("mod_info_conflicts_overwritten");
  }

  if (lastVersion < QVersionNumber(2, 2, 2)) {
    // log splitter is gone, it's a dock now
    m_Settings.remove("log_split");
  }

  //save version in all case
  m_Settings.setValue("version", currentVersion.toString());
}

QString Settings::getFilename() const
{
  return m_Settings.fileName();
}

void Settings::clearPlugins()
{
  m_Plugins.clear();
  m_PluginSettings.clear();

  m_PluginBlacklist.clear();
  int count = m_Settings.beginReadArray("pluginBlacklist");
  for (int i = 0; i < count; ++i) {
    m_Settings.setArrayIndex(i);
    m_PluginBlacklist.insert(m_Settings.value("name").toString());
  }
  m_Settings.endArray();
}

bool Settings::pluginBlacklisted(const QString &fileName) const
{
  return m_PluginBlacklist.contains(fileName);
}

void Settings::registerAsNXMHandler(bool force)
{
  const auto nxmPath = QCoreApplication::applicationDirPath() + "/nxmhandler.exe";
  const auto executable = QCoreApplication::applicationFilePath();

  QString mode = force ? "forcereg" : "reg";
  QString parameters = mode + " " + m_GamePlugin->gameShortName();
  for (const QString& altGame : m_GamePlugin->validShortNames()) {
    parameters += "," + altGame;
  }
  parameters += " \"" + executable + "\"";

  if (!shell::Execute(nxmPath, parameters)) {
    QMessageBox::critical(
      nullptr, tr("Failed"), tr("Failed to start the helper application"));
  }
}

bool Settings::colorSeparatorScrollbar() const
{
  return m_Settings.value("Settings/colorSeparatorScrollbars", true).toBool();
}

void Settings::managedGameChanged(IPluginGame const *gamePlugin)
{
  m_GamePlugin = gamePlugin;
}

void Settings::registerPlugin(IPlugin *plugin)
{
  m_Plugins.push_back(plugin);
  m_PluginSettings.insert(plugin->name(), QVariantMap());
  m_PluginDescriptions.insert(plugin->name(), QVariantMap());
  for (const PluginSetting &setting : plugin->settings()) {
    QVariant temp = m_Settings.value("Plugins/" + plugin->name() + "/" + setting.key, setting.defaultValue);
    if (!temp.convert(setting.defaultValue.type())) {
      log::warn(
        "failed to interpret \"{}\" as correct type for \"{}\" in plugin \"{}\", using default",
        temp.toString(), setting.key, plugin->name());
      temp = setting.defaultValue;
    }
    m_PluginSettings[plugin->name()][setting.key] = temp;
    m_PluginDescriptions[plugin->name()][setting.key] = QString("%1 (default: %2)").arg(setting.description).arg(setting.defaultValue.toString());
  }
}

bool Settings::obfuscate(const QString key, const QString data)
{
  QString finalKey("ModOrganizer2_" + key);
  wchar_t* keyData = new wchar_t[finalKey.size()+1];
  finalKey.toWCharArray(keyData);
  keyData[finalKey.size()] = L'\0';
  bool result = false;
  if (data.isEmpty()) {
    result = CredDeleteW(keyData, CRED_TYPE_GENERIC, 0);
    if (!result)
      if (GetLastError() == ERROR_NOT_FOUND)
        result = true;
  } else {
    wchar_t* charData = new wchar_t[data.size()];
    data.toWCharArray(charData);

    CREDENTIALW cred = {};
    cred.Flags = 0;
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = keyData;
    cred.CredentialBlob = (LPBYTE)charData;
    cred.CredentialBlobSize = sizeof(wchar_t) * data.size();
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

    result = CredWriteW(&cred, 0);
    delete[] charData;
  }
  delete[] keyData;
  return result;
}

QString Settings::deObfuscate(const QString key)
{
  QString result;
  QString finalKey("ModOrganizer2_" + key);
  wchar_t* keyData = new wchar_t[finalKey.size()+1];
  finalKey.toWCharArray(keyData);
  keyData[finalKey.size()] = L'\0';
  PCREDENTIALW creds;
  if (CredReadW(keyData, 1, 0, &creds)) {
    wchar_t *charData = (wchar_t *)creds->CredentialBlob;
    result = QString::fromWCharArray(charData, creds->CredentialBlobSize / sizeof(wchar_t));
    CredFree(creds);
  } else {
    const auto e = GetLastError();
    if (e != ERROR_NOT_FOUND) {
      log::error("Retrieving encrypted data failed: {}", formatSystemMessage(e));
    }
  }
  delete[] keyData;
  return result;
}

QColor Settings::getIdealTextColor(const QColor& rBackgroundColor)
{
  if (rBackgroundColor.alpha() == 0)
    return QColor(Qt::black);

  const int THRESHOLD = 106 * 255.0f / rBackgroundColor.alpha();
  int BackgroundDelta = (rBackgroundColor.red() * 0.299) + (rBackgroundColor.green() * 0.587) + (rBackgroundColor.blue() * 0.114);
  return QColor((255 - BackgroundDelta <= THRESHOLD) ? Qt::black : Qt::white);
}


bool Settings::hideUncheckedPlugins() const
{
  return m_Settings.value("Settings/hide_unchecked_plugins", false).toBool();
}

bool Settings::forceEnableCoreFiles() const
{
  return m_Settings.value("Settings/force_enable_core_files", true).toBool();
}

bool Settings::lockGUI() const
{
  return m_Settings.value("Settings/lock_gui", true).toBool();
}

bool Settings::automaticLoginEnabled() const
{
  return m_Settings.value("Settings/nexus_login", false).toBool();
}

QString Settings::getSteamAppID() const
{
  return m_Settings.value("Settings/app_id", m_GamePlugin->steamAPPId()).toString();
}

bool Settings::usePrereleases() const
{
  return m_Settings.value("Settings/use_prereleases", false).toBool();
}

void Settings::setDownloadSpeed(const QString &serverName, int bytesPerSecond)
{
  m_Settings.beginGroup("Servers");

  for (const QString &serverKey : m_Settings.childKeys()) {
    QVariantMap data = m_Settings.value(serverKey).toMap();
    if (serverKey == serverName) {
      data["downloadCount"] = data["downloadCount"].toInt() + 1;
      data["downloadSpeed"] = data["downloadSpeed"].toDouble() + static_cast<double>(bytesPerSecond);
      m_Settings.setValue(serverKey, data);
    }
  }

  m_Settings.endGroup();
  m_Settings.sync();
}

std::map<QString, int> Settings::getPreferredServers()
{
  std::map<QString, int> result;
  m_Settings.beginGroup("Servers");

  for (const QString &serverKey : m_Settings.childKeys()) {
    QVariantMap data = m_Settings.value(serverKey).toMap();
    int preference = data["preferred"].toInt();
    if (preference > 0) {
      result[serverKey] = preference;
    }
  }
  m_Settings.endGroup();

  return result;
}

QString Settings::getConfigurablePath(const QString &key,
                                      const QString &def,
                                      bool resolve) const
{
  QString result = QDir::fromNativeSeparators(
      m_Settings.value(QString("settings/") + key, QString("%BASE_DIR%/") + def)
          .toString());
  if (resolve) {
    result.replace("%BASE_DIR%", getBaseDirectory());
  }
  return result;
}

QString Settings::getBaseDirectory() const
{
  return QDir::fromNativeSeparators(m_Settings.value(
      "settings/base_directory", qApp->property("dataPath").toString()).toString());
}

QString Settings::getDownloadDirectory(bool resolve) const
{
  return getConfigurablePath("download_directory", ToQString(AppConfig::downloadPath()), resolve);
}

QString Settings::getCacheDirectory(bool resolve) const
{
  return getConfigurablePath("cache_directory", ToQString(AppConfig::cachePath()), resolve);
}

QString Settings::getModDirectory(bool resolve) const
{
  return getConfigurablePath("mod_directory", ToQString(AppConfig::modsPath()), resolve);
}

std::optional<QString> Settings::getManagedGameDirectory() const
{
  if (auto v=getOptional<QByteArray>(m_Settings, "gamePath")) {
    return QString::fromUtf8(*v);
  }

  return {};
}

void Settings::setManagedGameDirectory(const QString& path)
{
  m_Settings.setValue("gamePath", QDir::toNativeSeparators(path).toUtf8());
}

std::optional<QString> Settings::getManagedGameName() const
{
  return getOptional<QString>(m_Settings, "gameName");
}

void Settings::setManagedGameName(const QString& name)
{
  m_Settings.setValue("gameName", name);
}

std::optional<QString> Settings::getManagedGameEdition() const
{
  return getOptional<QString>(m_Settings, "game_edition");
}

void Settings::setManagedGameEdition(const QString& name)
{
  m_Settings.setValue("game_edition", name);
}

std::optional<QString> Settings::getSelectedProfileName() const
{
  if (auto v=getOptional<QByteArray>(m_Settings, "selected_profile")) {
    return QString::fromUtf8(*v);
  }

  return {};
}

void Settings::setSelectedProfileName(const QString& name)
{
  m_Settings.setValue("selected_profile", name.toUtf8());
}

std::optional<QString> Settings::getStyleName() const
{
  return getOptional<QString>(m_Settings, "Settings/style");
}

void Settings::setStyleName(const QString& name)
{
  m_Settings.setValue("Settings/style", name);
}

std::optional<bool> Settings::getUseProxy() const
{
  return getOptional<bool>(m_Settings, "Settings/use_proxy");
}

std::optional<QVersionNumber> Settings::getVersion() const
{
  if (auto v=getOptional<QString>(m_Settings, "version")) {
    return QVersionNumber::fromString(*v).normalized();
  }

  return {};
}

bool Settings::getFirstStart() const
{
  return getOptional<bool>(m_Settings, "first_start").value_or(true);
}

std::optional<QColor> Settings::getPreviousSeparatorColor() const
{
  const auto c = getOptional<QColor>(m_Settings, "previousSeparatorColor");
  if (c && c->isValid()) {
    return c;
  }

  return {};
}

void Settings::setPreviousSeparatorColor(const QColor& c) const
{
  m_Settings.setValue("previousSeparatorColor", c);
}

void Settings::removePreviousSeparatorColor()
{
  m_Settings.remove("previousSeparatorColor");
}

QString Settings::getProfileDirectory(bool resolve) const
{
  return getConfigurablePath("profiles_directory", ToQString(AppConfig::profilesPath()), resolve);
}

QString Settings::getOverwriteDirectory(bool resolve) const
{
  return getConfigurablePath("overwrite_directory",
                             ToQString(AppConfig::overwritePath()), resolve);
}

bool Settings::getNexusApiKey(QString &apiKey) const
{
  QString tempKey = deObfuscate("APIKEY");
  if (tempKey.isEmpty())
    return false;
  apiKey = tempKey;
  return true;
}

bool Settings::setNexusApiKey(const QString& apiKey)
{
  if (!obfuscate("APIKEY", apiKey)) {
    const auto e = GetLastError();
    log::error("Storing API key failed: {}", formatSystemMessage(e));
    return false;
  }

  return true;
}

bool Settings::clearNexusApiKey()
{
  return setNexusApiKey("");
}

bool Settings::hasNexusApiKey() const
{
  return !deObfuscate("APIKEY").isEmpty();
}

bool Settings::getSteamLogin(QString &username, QString &password) const
{
  username = m_Settings.value("Settings/steam_username", "").toString();
  password = deObfuscate("steam_password");

  return !username.isEmpty() && !password.isEmpty();
}
bool Settings::compactDownloads() const
{
  return m_Settings.value("Settings/compact_downloads", false).toBool();
}

bool Settings::metaDownloads() const
{
  return m_Settings.value("Settings/meta_downloads", false).toBool();
}

bool Settings::offlineMode() const
{
  return m_Settings.value("Settings/offline_mode", false).toBool();
}

log::Levels Settings::logLevel() const
{
  return static_cast<log::Levels>(m_Settings.value("Settings/log_level").toInt());
}

void Settings::setLogLevel(log::Levels level)
{
  m_Settings.setValue("Settings/log_level", static_cast<int>(level));
}

int Settings::crashDumpsType() const
{
  return m_Settings.value("Settings/crash_dumps_type", static_cast<int>(CrashDumpsType::Mini)).toInt();
}

int Settings::crashDumpsMax() const
{
  return m_Settings.value("Settings/crash_dumps_max", 5).toInt();
}

QColor Settings::modlistOverwrittenLooseColor() const
{
  return m_Settings.value("Settings/overwrittenLooseFilesColor", QColor(0, 255, 0, 64)).value<QColor>();
}

QColor Settings::modlistOverwritingLooseColor() const
{
  return m_Settings.value("Settings/overwritingLooseFilesColor", QColor(255, 0, 0, 64)).value<QColor>();
}

QColor Settings::modlistOverwrittenArchiveColor() const
{
  return m_Settings.value("Settings/overwrittenArchiveFilesColor", QColor(0, 255, 255, 64)).value<QColor>();
}

QColor Settings::modlistOverwritingArchiveColor() const
{
  return m_Settings.value("Settings/overwritingArchiveFilesColor", QColor(255, 0, 255, 64)).value<QColor>();
}

QColor Settings::modlistContainsPluginColor() const
{
  return m_Settings.value("Settings/containsPluginColor", QColor(0, 0, 255, 64)).value<QColor>();
}

QColor Settings::pluginListContainedColor() const
{
  return m_Settings.value("Settings/containedColor", QColor(0, 0, 255, 64)).value<QColor>();
}

QString Settings::executablesBlacklist() const
{
  return m_Settings.value("Settings/executable_blacklist", (
    QStringList()
        << "Chrome.exe"
        << "Firefox.exe"
        << "TSVNCache.exe"
        << "TGitCache.exe"
        << "Steam.exe"
        << "GameOverlayUI.exe"
        << "Discord.exe"
        << "GalaxyClient.exe"
        << "Spotify.exe"
    ).join(";")
  ).toString();
}

void Settings::setSteamLogin(QString username, QString password)
{
  if (username == "") {
    m_Settings.remove("Settings/steam_username");
    password = "";
  } else {
    m_Settings.setValue("Settings/steam_username", username);
  }
  if (!obfuscate("steam_password", password)) {
    const auto e = GetLastError();
    log::error("Storing or deleting password failed: {}", formatSystemMessage(e));
  }
}

LoadMechanism::EMechanism Settings::getLoadMechanism() const
{
  const auto i = m_Settings.value("Settings/load_mechanism").toInt();

  switch (i)
  {
    case LoadMechanism::LOAD_MODORGANIZER:
      return LoadMechanism::LOAD_MODORGANIZER;

    default:
      qCritical().nospace().noquote()
        << "invalid load mechanism " << i << ", reverting to modorganizer";

      m_Settings.setValue("Settings/load_mechanism", LoadMechanism::LOAD_MODORGANIZER);

      return LoadMechanism::LOAD_MODORGANIZER;
  }
}


void Settings::setupLoadMechanism()
{
  m_LoadMechanism.activate(getLoadMechanism());
}


bool Settings::useProxy() const
{
  return m_Settings.value("Settings/use_proxy", false).toBool();
}

bool Settings::endorsementIntegration() const
{
  return m_Settings.value("Settings/endorsement_integration", true).toBool();
}

bool Settings::hideAPICounter() const
{
  return m_Settings.value("Settings/hide_api_counter", false).toBool();
}

bool Settings::displayForeign() const
{
  return m_Settings.value("Settings/display_foreign", true).toBool();
}

void Settings::setMotDHash(uint hash)
{
  m_Settings.setValue("motd_hash", hash);
}

uint Settings::getMotDHash() const
{
  return m_Settings.value("motd_hash", 0).toUInt();
}

bool Settings::archiveParsing() const
{
  return m_Settings.value("Settings/archive_parsing_experimental", false).toBool();
}

QVariant Settings::pluginSetting(const QString &pluginName, const QString &key) const
{
  auto iterPlugin = m_PluginSettings.find(pluginName);
  if (iterPlugin == m_PluginSettings.end()) {
    return QVariant();
  }
  auto iterSetting = iterPlugin->find(key);
  if (iterSetting == iterPlugin->end()) {
    return QVariant();
  }

  return *iterSetting;
}

void Settings::setPluginSetting(const QString &pluginName, const QString &key, const QVariant &value)
{
  auto iterPlugin = m_PluginSettings.find(pluginName);
  if (iterPlugin == m_PluginSettings.end()) {
    throw MyException(tr("attempt to store setting for unknown plugin \"%1\"").arg(pluginName));
  }

  // store the new setting both in memory and in the ini
  m_PluginSettings[pluginName][key] = value;
  m_Settings.setValue("Plugins/" + pluginName + "/" + key, value);
}

QVariant Settings::pluginPersistent(const QString &pluginName, const QString &key, const QVariant &def) const
{
  if (!m_PluginSettings.contains(pluginName)) {
    return def;
  }
  return m_Settings.value("PluginPersistance/" + pluginName + "/" + key, def);
}

void Settings::setPluginPersistent(const QString &pluginName, const QString &key, const QVariant &value, bool sync)
{
  if (!m_PluginSettings.contains(pluginName)) {
    throw MyException(tr("attempt to store setting for unknown plugin \"%1\"").arg(pluginName));
  }
  m_Settings.setValue("PluginPersistance/" + pluginName + "/" + key, value);
  if (sync) {
    m_Settings.sync();
  }
}

QString Settings::language()
{
  QString result = m_Settings.value("Settings/language", "").toString();
  if (result.isEmpty()) {
    QStringList languagePreferences = QLocale::system().uiLanguages();
    if (languagePreferences.length() > 0) {
      // the users most favoritest language
      result = languagePreferences.at(0);
    } else {
      // fallback system locale
      result = QLocale::system().name();
    }
  }
  return result;
}

void Settings::updateServers(const QList<ServerInfo> &servers)
{
  m_Settings.beginGroup("Servers");
  QStringList oldServerKeys = m_Settings.childKeys();

  for (const ServerInfo &server : servers) {
    if (!oldServerKeys.contains(server.name)) {
      // not yet known server
      QVariantMap newVal;
      newVal["premium"] = server.premium;
      newVal["preferred"] = server.preferred ? 1 : 0;
      newVal["lastSeen"] = server.lastSeen;
      newVal["downloadCount"] = 0;
      newVal["downloadSpeed"] = 0.0;

      m_Settings.setValue(server.name, newVal);
    } else {
      QVariantMap data = m_Settings.value(server.name).toMap();
      data["lastSeen"] = server.lastSeen;
      data["premium"] = server.premium;

      m_Settings.setValue(server.name, data);
    }
  }

  // clean up unavailable servers
  QDate now = QDate::currentDate();
  for (const QString &key : m_Settings.childKeys()) {
    QVariantMap val = m_Settings.value(key).toMap();
    QDate lastSeen = val["lastSeen"].toDate();
    if (lastSeen.daysTo(now) > 30) {
      log::debug("removing server {} since it hasn't been available for downloads in over a month", key);
      m_Settings.remove(key);
    }
  }

  m_Settings.endGroup();

  m_Settings.sync();
}

void Settings::addBlacklistPlugin(const QString &fileName)
{
  m_PluginBlacklist.insert(fileName);
  writePluginBlacklist();
}

void Settings::writePluginBlacklist()
{
  m_Settings.remove("pluginBlacklist");
  m_Settings.beginWriteArray("pluginBlacklist");
  int idx = 0;
  for (const QString &plugin : m_PluginBlacklist) {
    m_Settings.setArrayIndex(idx++);
    m_Settings.setValue("name", plugin);
  }

  m_Settings.endArray();
}

std::map<QString, QString> Settings::getRecentDirectories() const
{
  std::map<QString, QString> map;

  const int size = m_Settings.beginReadArray("recentDirectories");

  for (int i=0; i<size; ++i) {
    m_Settings.setArrayIndex(i);

    const QVariant name = m_Settings.value("name");
    const QVariant dir = m_Settings.value("directory");

    if (name.isValid() && dir.isValid()) {
      map.emplace(name.toString(), dir.toString());
    }
  }

  m_Settings.endArray();

  return map;
}

void Settings::setRecentDirectories(const std::map<QString, QString>& map)
{
  m_Settings.remove("recentDirectories");
  m_Settings.beginWriteArray("recentDirectories");

  int index = 0;
  for (auto&& p : map) {
    m_Settings.setArrayIndex(index);
    m_Settings.setValue("name", p.first);
    m_Settings.setValue("directory", p.second);

    ++index;
  }

  m_Settings.endArray();
}

std::vector<std::map<QString, QVariant>> Settings::getExecutables() const
{
  const int count = m_Settings.beginReadArray("customExecutables");
  std::vector<std::map<QString, QVariant>> v;

  for (int i=0; i<count; ++i) {
    m_Settings.setArrayIndex(i);

    std::map<QString, QVariant> map;

    const auto keys = m_Settings.childKeys();
    for (auto&& key : keys) {
      map[key] = m_Settings.value(key);
    }

    v.push_back(map);
  }

  m_Settings.endArray();

  return v;
}

void Settings::setExecutables(const std::vector<std::map<QString, QVariant>>& v)
{
  m_Settings.remove("customExecutables");
  m_Settings.beginWriteArray("customExecutables");

  int i = 0;

  for (const auto& map : v) {
    m_Settings.setArrayIndex(i);

    for (auto&& p : map) {
      m_Settings.setValue(p.first, p.second);
    }

    ++i;
  }

  m_Settings.endArray();
}

std::optional<int> Settings::getIndex(QComboBox* cb) const
{
  return getOptional<int>(m_Settings, indexSettingName(cb));
}

void Settings::saveIndex(const QComboBox* cb)
{
  m_Settings.setValue(indexSettingName(cb), cb->currentIndex());
}

void Settings::restoreIndex(QComboBox* cb, std::optional<int> def) const
{
  if (auto v=getOptional<int>(m_Settings, indexSettingName(cb), def)) {
    cb->setCurrentIndex(*v);
  }
}

GeometrySettings& Settings::geometry()
{
  return m_Geometry;
}

const GeometrySettings& Settings::geometry() const
{
  return m_Geometry;
}

QSettings::Status Settings::sync() const
{
  m_Settings.sync();
  return m_Settings.status();
}

void Settings::dump() const
{
  static const QStringList ignore({
    "username", "password", "nexus_api_key"
    });

  log::debug("settings:");

  m_Settings.beginGroup("Settings");

  for (auto k : m_Settings.allKeys()) {
    if (ignore.contains(k, Qt::CaseInsensitive)) {
      continue;
    }

    log::debug("  . {}={}", k, m_Settings.value(k).toString());
  }

  m_Settings.endGroup();
}


GeometrySettings::GeometrySettings(QSettings& s)
  : m_Settings(s), m_Reset(false)
{
}

void GeometrySettings::requestReset()
{
  m_Reset = true;
}

void GeometrySettings::resetIfNeeded()
{
  if (!m_Reset) {
    return;
  }

  m_Settings.beginGroup("geometry");
  m_Settings.remove("");
  m_Settings.endGroup();
}

void GeometrySettings::saveGeometry(const QWidget* w)
{
  m_Settings.setValue(geoSettingName(w), w->saveGeometry());
}

bool GeometrySettings::restoreGeometry(QWidget* w) const
{
  if (auto v=getOptional<QByteArray>(m_Settings, geoSettingName(w))) {
    w->restoreGeometry(*v);
    return true;
  }

  return false;
}

void GeometrySettings::saveState(const QMainWindow* w)
{
  m_Settings.setValue(stateSettingName(w), w->saveState());
}

bool GeometrySettings::restoreState(QMainWindow* w) const
{
  if (auto v=getOptional<QByteArray>(m_Settings, stateSettingName(w))) {
    w->restoreState(*v);
    return true;
  }

  return false;
}

void GeometrySettings::saveState(const QHeaderView* w)
{
  m_Settings.setValue(stateSettingName(w), w->saveState());
}

bool GeometrySettings::restoreState(QHeaderView* w) const
{
  if (auto v=getOptional<QByteArray>(m_Settings, stateSettingName(w))) {
    w->restoreState(*v);
    return true;
  }

  return false;
}

void GeometrySettings::saveState(const QSplitter* w)
{
  m_Settings.setValue(stateSettingName(w), w->saveState());
}

bool GeometrySettings::restoreState(QSplitter* w) const
{
  if (auto v=getOptional<QByteArray>(m_Settings, stateSettingName(w))) {
    w->restoreState(*v);
    return true;
  }

  return false;
}

void GeometrySettings::saveVisibility(const QWidget* w)
{
  m_Settings.setValue(visibilitySettingName(w), w->isVisible());
}

bool GeometrySettings::restoreVisibility(QWidget* w, std::optional<bool> def) const
{
  if (auto v=getOptional<bool>(m_Settings, visibilitySettingName(w), def)) {
    w->setVisible(*v);
    return true;
  }

  return false;
}

void GeometrySettings::restoreToolbars(QMainWindow* w) const
{
  // all toolbars have the same size and button style settings
  const auto size = getOptional<QSize>(m_Settings, "toolbar_size");
  const auto style = getOptional<int>(m_Settings, "toolbar_button_style");

  for (auto* tb : w->findChildren<QToolBar*>()) {
    if (size) {
      tb->setIconSize(*size);
    }

    if (style) {
      tb->setToolButtonStyle(static_cast<Qt::ToolButtonStyle>(*style));
    }

    restoreVisibility(tb);
  }
}

void GeometrySettings::saveToolbars(const QMainWindow* w)
{
  const auto tbs = w->findChildren<QToolBar*>();

  // save visibility for all
  for (auto* tb : tbs) {
    saveVisibility(tb);
  }

  // all toolbars have the same size and button style settings, just save the
  // first one
  if (!tbs.isEmpty()) {
    const auto* tb = tbs[0];

    m_Settings.setValue("toolbar_size", tb->iconSize());
    m_Settings.setValue("toolbar_button_style", static_cast<int>(tb->toolButtonStyle()));
  }
}

QStringList GeometrySettings::getModInfoTabOrder() const
{
  QStringList v;

  if (m_Settings.contains("mod_info_tabs")) {
    // old byte array from 2.2.0
    QDataStream stream(m_Settings.value("mod_info_tabs").toByteArray());

    int count = 0;
    stream >> count;

    for (int i=0; i<count; ++i) {
      QString s;
      stream >> s;
      v.push_back(s);
    }
  } else {
    // string list since 2.2.1
    QString string = m_Settings.value("mod_info_tab_order").toString();
    QTextStream stream(&string);

    while (!stream.atEnd()) {
      QString s;
      stream >> s;
      v.push_back(s);
    }
  }

  return v;
}

void GeometrySettings::setModInfoTabOrder(const QString& names)
{
  m_Settings.setValue("mod_info_tab_order", names);
}

void GeometrySettings::centerOnMainWindowMonitor(QWidget* w)
{
  const auto monitor = getOptional<int>(m_Settings, "geometry/MainWindow_monitor");

  QPoint center;

  if (monitor && QGuiApplication::screens().size() > *monitor) {
    center = QGuiApplication::screens().at(*monitor)->geometry().center();
  } else {
    center = QGuiApplication::primaryScreen()->geometry().center();
  }

  w->move(center - w->rect().center());
}

void GeometrySettings::saveMainWindowMonitor(const QMainWindow* w)
{
  if (auto* handle=w->windowHandle()) {
    if (auto* screen = handle->screen()) {
      const int screenId = QGuiApplication::screens().indexOf(screen);
      m_Settings.setValue("geometry/MainWindow_monitor", screenId);
    }
  }
}

Qt::Orientation dockOrientation(const QMainWindow* mw, const QDockWidget* d)
{
  // docks in these areas are horizontal
  const auto horizontalAreas =
    Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea;

  if (mw->dockWidgetArea(const_cast<QDockWidget*>(d)) & horizontalAreas) {
    return Qt::Horizontal;
  } else {
    return Qt::Vertical;
  }
}

void GeometrySettings::saveDocks(const QMainWindow* mw)
{
  // this attempts to fix https://bugreports.qt.io/browse/QTBUG-46620 where dock
  // sizes are not restored when the main window is maximized; it is used in
  // MainWindow::readSettings() and MainWindow::storeSettings()
  //
  // there's also https://stackoverflow.com/questions/44005852, which has what
  // seems to be a popular fix, but it breaks the restored size of the window
  // by setting it to the desktop's resolution, so that doesn't work
  //
  // the only fix I could find is to remember the sizes of the docks and manually
  // setting them back; saving is straightforward, but restoring is messy
  //
  // this also depends on the window being visible before the timer in restore()
  // is fired and the timer must be processed by application.exec(); therefore,
  // the splash screen _must_ be closed before readSettings() is called, because
  // it has its own event loop, which seems to interfere with this
  //
  // all of this should become unnecessary when QTBUG-46620 is fixed
  //

  // saves the size of each dock
  for (const auto* dock : mw->findChildren<QDockWidget*>()) {
    int size = 0;

    // save the width for horizontal docks, or the height for vertical
    if (dockOrientation(mw, dock) == Qt::Horizontal) {
      size = dock->size().width();
    } else {
      size = dock->size().height();
    }

    m_Settings.setValue(dockSettingName(dock), size);
  }
}

void GeometrySettings::restoreDocks(QMainWindow* mw) const
{
  struct DockInfo
  {
    QDockWidget* d;
    int size = 0;
    Qt::Orientation ori;
  };

  std::vector<DockInfo> dockInfos;

  // for each dock
  for (auto* dock : mw->findChildren<QDockWidget*>()) {
    if (auto size=getOptional<int>(m_Settings, dockSettingName(dock))) {
      // remember this dock, its size and orientation
      dockInfos.push_back({dock, *size, dockOrientation(mw, dock)});
    }
  }

  // the main window must have had time to process the settings from
  // readSettings() or it seems to override whatever is set here
  //
  // some people said a single processEvents() call is enough, but it doesn't
  // look like it
  QTimer::singleShot(5, [=] {
    for (const auto& info : dockInfos) {
      mw->resizeDocks({info.d}, {info.size}, info.ori);
    }
  });
}


GeometrySaver::GeometrySaver(Settings& s, QDialog* dialog)
  : m_settings(s), m_dialog(dialog)
{
  m_settings.geometry().restoreGeometry(m_dialog);
}

GeometrySaver::~GeometrySaver()
{
  m_settings.geometry().saveGeometry(m_dialog);
}
