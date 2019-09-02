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
#include "expanderwidget.h"
#include <utility.h>
#include <iplugingame.h>

using namespace MOBase;

class ScopedGroup
{
public:
  ScopedGroup(QSettings& s, const QString& name)
    : m_settings(s)
  {
    m_settings.beginGroup(name);
  }

  ~ScopedGroup()
  {
    m_settings.endGroup();
  }

  ScopedGroup(const ScopedGroup&) = delete;
  ScopedGroup& operator=(const ScopedGroup&) = delete;

private:
  QSettings& m_settings;
};


class ScopedReadArray
{
public:
  ScopedReadArray(QSettings& s, const QString& name)
    : m_settings(s), m_count(0)
  {
    m_count = m_settings.beginReadArray(name);
  }

  ~ScopedReadArray()
  {
    m_settings.endArray();
  }

  ScopedReadArray(const ScopedReadArray&) = delete;
  ScopedReadArray& operator=(const ScopedReadArray&) = delete;

  int count() const
  {
    return m_count;
  }

private:
  QSettings& m_settings;
  int m_count;
};


class ScopedWriteArray
{
public:
  ScopedWriteArray(QSettings& s, const QString& name)
    : m_settings(s)
  {
    m_settings.beginWriteArray(name);
  }

  ~ScopedWriteArray()
  {
    m_settings.endArray();
  }

  ScopedWriteArray(const ScopedWriteArray&) = delete;
  ScopedWriteArray& operator=(const ScopedWriteArray&) = delete;

private:
  QSettings& m_settings;
};


template <class T>
std::optional<T> getOptional(
  const QSettings& s, const QString& name, std::optional<T> def={})
{
  if (s.contains(name)) {
    const auto v = s.value(name);

    if constexpr (std::is_enum_v<T>) {
      return static_cast<T>(v.value<std::underlying_type_t<T>>());
    } else {
      return v.value<T>();
    }
  }

  return def;
}


EndorsementState endorsementStateFromString(const QString& s)
{
  if (s == "Endorsed") {
    return EndorsementState::Accepted;
  } else if (s == "Abstained") {
    return EndorsementState::Refused;
  } else {
    return EndorsementState::NoDecision;
  }
}

QString toString(EndorsementState s)
{
  switch (s)
  {
    case EndorsementState::Accepted:
      return "Endorsed";

    case EndorsementState::Refused:
      return "Abstained";

    case EndorsementState::NoDecision: // fall-through
    default:
      return {};
  }
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

QString widgetName(const ExpanderWidget* w)
{
  return widgetNameWithTopLevel(w->button());
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

QString checkedSettingName(const QAbstractButton* b)
{
  return widgetNameWithTopLevel(b) + "_checked";
}

void warnIfNotCheckable(const QAbstractButton* b)
{
  if (!b->isCheckable()) {
    log::warn(
      "button '{}' used in the settings as a checkbox or radio button "
      "but is not checkable", b->objectName());
  }
}


Settings *Settings::s_Instance = nullptr;

Settings::Settings(const QString& path) :
  m_Settings(path, QSettings::IniFormat),
  m_Geometry(m_Settings), m_Colors(m_Settings), m_Plugins(m_Settings)
{
  if (s_Instance != nullptr) {
    throw std::runtime_error("second instance of \"Settings\" created");
  } else {
    s_Instance = this;
  }

  MOBase::QuestionBoxMemory::setCallbacks(
    [this](auto&& w, auto&& f){ return getQuestionButton(w, f); },
    [this](auto&& w, auto&& b){ setQuestionWindowButton(w, b); },
    [this](auto&& w, auto&& f, auto&& b){ setQuestionFileButton(w, f, b); });
}

Settings::~Settings()
{
  MOBase::QuestionBoxMemory::setCallbacks({}, {}, {});
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
    {
      ScopedGroup sg(m_Settings, "Settings");
      m_Settings.remove("steam_password");
      m_Settings.remove("nexus_username");
      m_Settings.remove("nexus_password");
      m_Settings.remove("nexus_login");
      m_Settings.remove("nexus_api_key");
      m_Settings.remove("ask_for_nexuspw");
      m_Settings.remove("nmm_version");
    }

    {
      ScopedGroup sg(m_Settings, "Servers");
      m_Settings.remove("");
    }
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

void Settings::setColorSeparatorScrollbar(bool b)
{
  m_Settings.setValue("Settings/colorSeparatorScrollbars", b);
}

void Settings::managedGameChanged(IPluginGame const *gamePlugin)
{
  m_GamePlugin = gamePlugin;
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

void Settings::setHideUncheckedPlugins(bool b)
{
  m_Settings.setValue("Settings/hide_unchecked_plugins", b);
}

bool Settings::forceEnableCoreFiles() const
{
  return m_Settings.value("Settings/force_enable_core_files", true).toBool();
}

void Settings::setForceEnableCoreFiles(bool b)
{
  m_Settings.setValue("Settings/force_enable_core_files", b);
}

bool Settings::lockGUI() const
{
  return m_Settings.value("Settings/lock_gui", true).toBool();
}

void Settings::setLockGUI(bool b)
{
  m_Settings.setValue("Settings/lock_gui", b);
}

bool Settings::automaticLoginEnabled() const
{
  return m_Settings.value("Settings/nexus_login", false).toBool();
}

QString Settings::getSteamAppID() const
{
  return m_Settings.value("Settings/app_id", m_GamePlugin->steamAPPId()).toString();
}

void Settings::setSteamAppID(const QString& id)
{
  if (id.isEmpty()) {
    m_Settings.remove("Settings/app_id");
  } else {
    m_Settings.setValue("Settings/app_id", id);
  }
}

bool Settings::usePrereleases() const
{
  return m_Settings.value("Settings/use_prereleases", false).toBool();
}

void Settings::setUsePrereleases(bool b)
{
  m_Settings.setValue("Settings/use_prereleases", b);
}

QString Settings::getConfigurablePath(const QString &key,
                                      const QString &def,
                                      bool resolve) const
{
  const QString settingName = "Settings/" + key;

  QString result = QDir::fromNativeSeparators(
      m_Settings.value(settingName, QString("%BASE_DIR%/") + def).toString());

  if (resolve) {
    result.replace("%BASE_DIR%", getBaseDirectory());
  }

  return result;
}

void Settings::setConfigurablePath(const QString &key, const QString& path)
{
  const QString settingName = "Settings/" + key;

  if (path.isEmpty()) {
    m_Settings.remove(settingName);
  } else {
    m_Settings.setValue(settingName, path);
  }
}

QString Settings::getBaseDirectory() const
{
  return QDir::fromNativeSeparators(m_Settings.value(
    "settings/base_directory",
    qApp->property("dataPath").toString()).toString());
}

QString Settings::getDownloadDirectory(bool resolve) const
{
  return getConfigurablePath(
    "download_directory",
    ToQString(AppConfig::downloadPath()),
    resolve);
}

QString Settings::getCacheDirectory(bool resolve) const
{
  return getConfigurablePath(
    "cache_directory",
    ToQString(AppConfig::cachePath()),
    resolve);
}

QString Settings::getModDirectory(bool resolve) const
{
  return getConfigurablePath(
    "mod_directory",
    ToQString(AppConfig::modsPath()),
    resolve);
}

QString Settings::getProfileDirectory(bool resolve) const
{
  return getConfigurablePath(
    "profiles_directory",
    ToQString(AppConfig::profilesPath()),
    resolve);
}

QString Settings::getOverwriteDirectory(bool resolve) const
{
  return getConfigurablePath(
    "overwrite_directory",
    ToQString(AppConfig::overwritePath()),
    resolve);
}

void Settings::setBaseDirectory(const QString& path)
{
  if (path.isEmpty()) {
    m_Settings.remove("Settings/base_directory");
  } else {
    m_Settings.setValue("Settings/base_directory", path);
  }
}

void Settings::setDownloadDirectory(const QString& path)
{
  setConfigurablePath("download_directory", path);
}

void Settings::setModDirectory(const QString& path)
{
  setConfigurablePath("mod_directory", path);
}

void Settings::setCacheDirectory(const QString& path)
{
  setConfigurablePath("cache_directory", path);
}

void Settings::setProfileDirectory(const QString& path)
{
  setConfigurablePath("profiles_directory", path);
}

void Settings::setOverwriteDirectory(const QString& path)
{
  setConfigurablePath("overwrite_directory", path);
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

void Settings::setUseProxy(bool b)
{
  m_Settings.setValue("Settings/use_proxy", b);
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

void Settings::setFirstStart(bool b)
{
  m_Settings.setValue("first_start", b);
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

void Settings::setCompactDownloads(bool b)
{
  m_Settings.setValue("Settings/compact_downloads", b);
}

bool Settings::metaDownloads() const
{
  return m_Settings.value("Settings/meta_downloads", false).toBool();
}

void Settings::setMetaDownloads(bool b)
{
  m_Settings.setValue("Settings/meta_downloads", b);
}

bool Settings::offlineMode() const
{
  return m_Settings.value("Settings/offline_mode", false).toBool();
}

void Settings::setOfflineMode(bool b)
{
  m_Settings.setValue("Settings/offline_mode", b);
}

log::Levels Settings::logLevel() const
{
  return static_cast<log::Levels>(m_Settings.value("Settings/log_level").toInt());
}

void Settings::setLogLevel(log::Levels level)
{
  m_Settings.setValue("Settings/log_level", static_cast<int>(level));
}

CrashDumpsType Settings::crashDumpsType() const
{
  const auto v = getOptional<CrashDumpsType>(m_Settings, "Settings/crash_dumps_type");
  return v.value_or(CrashDumpsType::Mini);
}

void Settings::setCrashDumpsType(CrashDumpsType type)
{
  m_Settings.setValue("Settings/crash_dumps_type", static_cast<int>(type));
}

int Settings::crashDumpsMax() const
{
  return m_Settings.value("Settings/crash_dumps_max", 5).toInt();
}

void Settings::setCrashDumpsMax(int n)
{
  return m_Settings.setValue("Settings/crash_dumps_max", n);
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

void Settings::setExecutablesBlacklist(const QString& s)
{
  m_Settings.setValue("Settings/executable_blacklist", s);
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

void Settings::setLoadMechanism(LoadMechanism::EMechanism m)
{
  m_Settings.setValue("Settings/load_mechanism", static_cast<int>(m));
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

void Settings::setEndorsementIntegration(bool b) const
{
  m_Settings.setValue("Settings/endorsement_integration", b);
}

EndorsementState Settings::endorsementState() const
{
  const auto v = getOptional<QString>(m_Settings, "endorse_state");
  return endorsementStateFromString(v.value_or(""));
}

void Settings::setEndorsementState(EndorsementState s)
{
  const auto v = toString(s);

  if (v.isEmpty()) {
    m_Settings.remove("endorse_state");
  } else {
    m_Settings.setValue("endorse_state", v);
  }
}

bool Settings::hideAPICounter() const
{
  return m_Settings.value("Settings/hide_api_counter", false).toBool();
}

void Settings::setHideAPICounter(bool b)
{
  m_Settings.setValue("Settings/hide_api_counter", b);
}

bool Settings::displayForeign() const
{
  return m_Settings.value("Settings/display_foreign", true).toBool();
}

void Settings::setDisplayForeign(bool b)
{
  m_Settings.setValue("Settings/display_foreign", b);
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

void Settings::setArchiveParsing(bool b)
{
  m_Settings.setValue("Settings/archive_parsing_experimental", b);
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

void Settings::setLanguage(const QString& name)
{
  m_Settings.setValue("Settings/language", name);
}

void Settings::setDownloadSpeed(const QString& name, int bytesPerSecond)
{
  auto servers = getServers();

  for (auto& server : servers) {
    if (server.name() == name) {
      server.addDownload(bytesPerSecond);
      updateServers(servers);
      return;
    }
  }

  log::error(
    "server '{}' not found while trying to add a download with bps {}",
    name, bytesPerSecond);
}

ServerList Settings::getServers() const
{
  // servers used to be a map of byte arrays until 2.2.1, it's now an array of
  // individual values instead
  //
  // so post 2.2.1, only one key is returned: "size", the size of the arrays;
  // in 2.2.1, one key per server is returned

  // getting the keys
  QStringList keys;

  {
    ScopedGroup sg(m_Settings, "Servers");
    keys = m_Settings.childKeys();
  }

  if (!keys.empty() && keys[0] != "size") {
    // old format
    return getServersFromOldMap();
  }


  // post 2.2.1 format, array of values

  ServerList list;

  {
    ScopedReadArray sra(m_Settings, "Servers");

    for (int i=0; i<sra.count(); ++i) {
      m_Settings.setArrayIndex(i);

      ServerInfo::SpeedList lastDownloads;

      const auto lastDownloadsString = m_Settings.value("lastDownloads").toString();
      for (const auto& s : lastDownloadsString.split(" ")) {
        const auto bytesPerSecond = s.toInt();
        if (bytesPerSecond > 0) {
          lastDownloads.push_back(bytesPerSecond);
        }
      }

      ServerInfo server(
        m_Settings.value("name").toString(),
        m_Settings.value("premium").toBool(),
        QDate::fromString(m_Settings.value("lastSeen").toString(), Qt::ISODate),
        m_Settings.value("preferred").toInt(),
        lastDownloads);

      list.add(std::move(server));
    }
  }

  return list;
}

ServerList Settings::getServersFromOldMap() const
{
  // for 2.2.1 and before

  ServerList list;
  ScopedGroup sg(m_Settings, "Servers");

  for (const QString &serverKey : m_Settings.childKeys()) {
    QVariantMap data = m_Settings.value(serverKey).toMap();

    ServerInfo server(
      serverKey,
      data["premium"].toBool(),
      data["lastSeen"].toDate(),
      data["preferred"].toInt(),
      {});

    // ignoring download count and speed, it's now a list of values instead of
    // a total

    list.add(std::move(server));
  }

  return list;
}

void Settings::updateServers(ServerList servers)
{
  // clean up unavailable servers
  servers.cleanup();

  {
    ScopedGroup sg(m_Settings, "Servers");
    m_Settings.remove("");
  }

  {
    ScopedWriteArray swa(m_Settings, "Servers");

    int i=0;
    for (const auto& server : servers) {
      m_Settings.setArrayIndex(i);

      m_Settings.setValue("name", server.name());
      m_Settings.setValue("premium", server.isPremium());
      m_Settings.setValue("lastSeen", server.lastSeen().toString(Qt::ISODate));
      m_Settings.setValue("preferred", server.preferred());

      QString lastDownloads;
      for (const auto& speed : server.lastDownloads()) {
        if (speed > 0) {
          lastDownloads += QString("%1 ").arg(speed);
        }
      }

      m_Settings.setValue("lastDownloads", lastDownloads.trimmed());

      ++i;
    }
  }
}

std::map<QString, QString> Settings::getRecentDirectories() const
{
  std::map<QString, QString> map;

  ScopedReadArray sra(m_Settings, "recentDirectories");

  for (int i=0; i<sra.count(); ++i) {
    m_Settings.setArrayIndex(i);

    const QVariant name = m_Settings.value("name");
    const QVariant dir = m_Settings.value("directory");

    if (name.isValid() && dir.isValid()) {
      map.emplace(name.toString(), dir.toString());
    }
  }

  return map;
}

void Settings::setRecentDirectories(const std::map<QString, QString>& map)
{
  m_Settings.remove("recentDirectories");

  ScopedWriteArray swa(m_Settings, "recentDirectories");

  int index = 0;
  for (auto&& p : map) {
    m_Settings.setArrayIndex(index);
    m_Settings.setValue("name", p.first);
    m_Settings.setValue("directory", p.second);

    ++index;
  }
}

std::vector<std::map<QString, QVariant>> Settings::getExecutables() const
{
  ScopedReadArray sra(m_Settings, "customExecutables");
  std::vector<std::map<QString, QVariant>> v;

  for (int i=0; i<sra.count(); ++i) {
    m_Settings.setArrayIndex(i);

    std::map<QString, QVariant> map;

    const auto keys = m_Settings.childKeys();
    for (auto&& key : keys) {
      map[key] = m_Settings.value(key);
    }

    v.push_back(map);
  }

  return v;
}

void Settings::setExecutables(const std::vector<std::map<QString, QVariant>>& v)
{
  m_Settings.remove("customExecutables");

  ScopedWriteArray swa(m_Settings, "customExecutables");

  int i = 0;

  for (const auto& map : v) {
    m_Settings.setArrayIndex(i);

    for (auto&& p : map) {
      m_Settings.setValue(p.first, p.second);
    }

    ++i;
  }
}

bool Settings::isTutorialCompleted(const QString& windowName) const
{
  const auto v = getOptional<bool>(
    m_Settings, "CompletedWindowTutorials/" + windowName);

  return v.value_or(false);
}

void Settings::setTutorialCompleted(const QString& windowName, bool b)
{
  m_Settings.setValue("CompletedWindowTutorials/" + windowName, true);
}

bool Settings::keepBackupOnInstall() const
{
  return getOptional<bool>(m_Settings, "backup_install").value_or(false);
}

void Settings::setKeepBackupOnInstall(bool b)
{
  m_Settings.setValue("backup_install", b);
}

QuestionBoxMemory::Button Settings::getQuestionButton(
  const QString& windowName, const QString& filename) const
{
  const QString windowSetting("DialogChoices/" + windowName);

  if (!filename.isEmpty()) {
    const auto fileSetting = windowSetting + "/" + filename;

    if (auto v=getOptional<int>(m_Settings, fileSetting)) {
      return static_cast<QuestionBoxMemory::Button>(*v);
    }
  }

  if (auto v=getOptional<int>(m_Settings, windowSetting)) {
    return static_cast<QuestionBoxMemory::Button>(*v);
  }

  return QuestionBoxMemory::NoButton;
}

void Settings::setQuestionWindowButton(
  const QString& windowName, QuestionBoxMemory::Button button)
{
  const QString settingName("DialogChoices/" + windowName);

  if (button == QuestionBoxMemory::NoButton) {
    m_Settings.remove(settingName);
  } else {
    m_Settings.setValue(settingName, static_cast<int>(button));
  }
}

void Settings::setQuestionFileButton(
  const QString& windowName, const QString& filename,
  QuestionBoxMemory::Button button)
{
  const QString settingName("DialogChoices/" + windowName + "/" + filename);

  if (button == QuestionBoxMemory::NoButton) {
    m_Settings.remove(settingName);
  } else {
    m_Settings.setValue(settingName, static_cast<int>(button));
  }
}

void Settings::resetQuestionButtons()
{
  ScopedGroup sg(m_Settings, "DialogChoices");
  m_Settings.remove("");
}

std::optional<int> Settings::getIndex(const QComboBox* cb) const
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

std::optional<int> Settings::getIndex(const QTabWidget* w) const
{
  return getOptional<int>(m_Settings, indexSettingName(w));
}

void Settings::saveIndex(const QTabWidget* w)
{
  m_Settings.setValue(indexSettingName(w), w->currentIndex());
}

void Settings::restoreIndex(QTabWidget* w, std::optional<int> def) const
{
  if (auto v=getOptional<int>(m_Settings, indexSettingName(w), def)) {
    w->setCurrentIndex(*v);
  }
}

std::optional<bool> Settings::getChecked(const QAbstractButton* w) const
{
  warnIfNotCheckable(w);
  return getOptional<bool>(m_Settings, checkedSettingName(w));
}

void Settings::saveChecked(const QAbstractButton* w)
{
  warnIfNotCheckable(w);
  m_Settings.setValue(checkedSettingName(w), w->isChecked());
}

void Settings::restoreChecked(QAbstractButton* w, std::optional<bool> def) const
{
  warnIfNotCheckable(w);

  if (auto v=getOptional<bool>(m_Settings, checkedSettingName(w), def)) {
    w->setChecked(*v);
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

ColorSettings& Settings::colors()
{
  return m_Colors;
}

const ColorSettings& Settings::colors() const
{
  return m_Colors;
}

PluginSettings& Settings::plugins()
{
  return m_Plugins;
}

const PluginSettings& Settings::plugins() const
{
  return m_Plugins;
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

  {
    ScopedGroup sg(m_Settings, "Settings");

    for (auto k : m_Settings.allKeys()) {
      if (ignore.contains(k, Qt::CaseInsensitive)) {
        continue;
      }

      log::debug("  . {}={}", k, m_Settings.value(k).toString());
    }
  }

  log::debug("servers:");

  for (const auto& server : getServers()) {
    QString lastDownloads;
    for (auto speed : server.lastDownloads()) {
      lastDownloads += QString("%1 ").arg(speed);
    }

    log::debug(
      "  . {} premium={} lastSeen={} preferred={} lastDownloads={}",
      server.name(),
      server.isPremium() ? "yes" : "no",
      server.lastSeen().toString(Qt::ISODate),
      server.preferred(),
      lastDownloads.trimmed());
  }
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

  ScopedGroup sg(m_Settings, "geometry");
  m_Settings.remove("");
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

void GeometrySettings::saveState(const ExpanderWidget* expander)
{
  m_Settings.setValue(stateSettingName(expander), expander->saveState());
}

bool GeometrySettings::restoreState(ExpanderWidget* expander) const
{
  if (auto v=getOptional<QByteArray>(m_Settings, stateSettingName(expander))) {
    expander->restoreState(*v);
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


ColorSettings::ColorSettings(QSettings& s)
  : m_Settings(s)
{
}

QColor ColorSettings::modlistOverwrittenLoose() const
{
  return getOptional<QColor>(m_Settings, "Settings/overwrittenLooseFilesColor")
    .value_or(QColor(0, 255, 0, 64));
}

void ColorSettings::setModlistOverwrittenLoose(const QColor& c)
{
  m_Settings.setValue("Settings/overwrittenLooseFilesColor", c);
}

QColor ColorSettings::modlistOverwritingLoose() const
{
  return getOptional<QColor>(m_Settings, "Settings/overwritingLooseFilesColor")
    .value_or(QColor(255, 0, 0, 64));
}

void ColorSettings::setModlistOverwritingLoose(const QColor& c)
{
  m_Settings.setValue("Settings/overwritingLooseFilesColor", c);
}

QColor ColorSettings::modlistOverwrittenArchive() const
{
  return getOptional<QColor>(m_Settings, "Settings/overwrittenArchiveFilesColor")
    .value_or(QColor(0, 255, 255, 64));
}

void ColorSettings::setModlistOverwrittenArchive(const QColor& c)
{
  m_Settings.setValue("Settings/overwrittenArchiveFilesColor", c);
}

QColor ColorSettings::modlistOverwritingArchive() const
{
  return getOptional<QColor>(m_Settings, "Settings/overwritingArchiveFilesColor")
    .value_or(QColor(255, 0, 255, 64));
}

void ColorSettings::setModlistOverwritingArchive(const QColor& c)
{
  m_Settings.setValue("Settings/overwritingArchiveFilesColor", c);
}

QColor ColorSettings::modlistContainsPlugin() const
{
  return getOptional<QColor>(m_Settings, "Settings/containsPluginColor")
    .value_or(QColor(0, 0, 255, 64));
}

void ColorSettings::setModlistContainsPlugin(const QColor& c)
{
  m_Settings.setValue("Settings/containsPluginColor", c);
}

QColor ColorSettings::pluginListContained() const
{
  return getOptional<QColor>(m_Settings, "Settings/containedColor")
    .value_or(QColor(0, 0, 255, 64));
}

void ColorSettings::setPluginListContained(const QColor& c)
{
  m_Settings.setValue("Settings/containedColor", c);
}


PluginSettings::PluginSettings(QSettings& settings)
  : m_Settings(settings)
{
}

void PluginSettings::clearPlugins()
{
  m_Plugins.clear();
  m_PluginSettings.clear();

  m_PluginBlacklist.clear();

  ScopedReadArray sra(m_Settings, "pluginBlacklist");
  for (int i = 0; i < sra.count(); ++i) {
    m_Settings.setArrayIndex(i);
    m_PluginBlacklist.insert(m_Settings.value("name").toString());
  }
}

void PluginSettings::registerPlugin(IPlugin *plugin)
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

bool PluginSettings::pluginBlacklisted(const QString &fileName) const
{
  return m_PluginBlacklist.contains(fileName);
}

QVariant PluginSettings::pluginSetting(const QString &pluginName, const QString &key) const
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

void PluginSettings::setPluginSetting(const QString &pluginName, const QString &key, const QVariant &value)
{
  auto iterPlugin = m_PluginSettings.find(pluginName);
  if (iterPlugin == m_PluginSettings.end()) {
    throw MyException(QObject::tr("attempt to store setting for unknown plugin \"%1\"").arg(pluginName));
  }

  // store the new setting both in memory and in the ini
  m_PluginSettings[pluginName][key] = value;
  m_Settings.setValue("Plugins/" + pluginName + "/" + key, value);
}

QVariant PluginSettings::pluginPersistent(const QString &pluginName, const QString &key, const QVariant &def) const
{
  if (!m_PluginSettings.contains(pluginName)) {
    return def;
  }
  return m_Settings.value("PluginPersistance/" + pluginName + "/" + key, def);
}

void PluginSettings::setPluginPersistent(const QString &pluginName, const QString &key, const QVariant &value, bool sync)
{
  if (!m_PluginSettings.contains(pluginName)) {
    throw MyException(QObject::tr("attempt to store setting for unknown plugin \"%1\"").arg(pluginName));
  }
  m_Settings.setValue("PluginPersistance/" + pluginName + "/" + key, value);
  if (sync) {
    m_Settings.sync();
  }
}

void PluginSettings::addBlacklistPlugin(const QString &fileName)
{
  m_PluginBlacklist.insert(fileName);
  writePluginBlacklist();
}

void PluginSettings::writePluginBlacklist()
{
  m_Settings.remove("pluginBlacklist");

  ScopedWriteArray swa(m_Settings, "pluginBlacklist");
  int idx = 0;
  for (const QString &plugin : m_PluginBlacklist) {
    m_Settings.setArrayIndex(idx++);
    m_Settings.setValue("name", plugin);
  }
}

QVariantMap PluginSettings::pluginSettings(const QString &pluginName) const
{
  return m_PluginSettings[pluginName];
}

void PluginSettings::setPluginSettings(const QString &pluginName, const QVariantMap& map)
{
  m_PluginSettings[pluginName] = map;
}

QVariantMap PluginSettings::pluginDescriptions(const QString &pluginName) const
{
  return m_PluginDescriptions[pluginName];
}

void PluginSettings::pluginDescriptions(const QString &pluginName, const QVariantMap& map)
{
  m_PluginDescriptions[pluginName] = map;
}

const QSet<QString>& PluginSettings::pluginBlacklist() const
{
  return m_PluginBlacklist;
}

void PluginSettings::setPluginBlacklist(const QStringList& pluginNames)
{
  m_PluginBlacklist.clear();

  for (const auto& name : pluginNames) {
    m_PluginBlacklist.insert(name);
  }
}

void PluginSettings::save()
{
  for (auto iterPlugins=m_PluginSettings.begin(); iterPlugins!=m_PluginSettings.end(); ++iterPlugins) {
    for (auto iterSettings=iterPlugins->begin(); iterSettings!=iterPlugins->end(); ++iterSettings) {
      const auto key = "Plugins/" + iterPlugins.key() + "/" + iterSettings.key();
      m_Settings.setValue(key, iterSettings.value());
    }
  }

  writePluginBlacklist();
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
