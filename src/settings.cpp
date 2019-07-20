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

#include "pluginsetting.h"
#include "serverinfo.h"
#include "settingsdialog.h"
#include "settingsdialoggeneral.h"
#include "settingsdialognexus.h"
#include "settingsdialogpaths.h"
#include "versioninfo.h"
#include "appconfig.h"
#include "organizercore.h"
#include <utility.h>
#include <iplugin.h>
#include <iplugingame.h>
#include <questionboxmemory.h>
#include <usvfsparameters.h>

#include <QCheckBox>
#include <QCoreApplication>
#include <QComboBox>
#include <QDate>
#include <QDialog>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QLineEdit>
#include <QSpinBox>
#include <QListWidgetItem>
#include <QLocale>
#include <QMessageBox>
#include <QApplication>
#include <QRegExp>
#include <QDir>
#include <QStringList>
#include <QVariantMap>
#include <QLabel>
#include <QPushButton>
#include <QPalette>

#include <Qt> // for Qt::UserRole, etc

#include <Windows.h> // For ShellExecuteW, HINSTANCE, etc
#include <wincred.h> // For storage

#include <algorithm> // for sort
#include <memory>
#include <stdexcept> // for runtime_error
#include <string>
#include <utility> // for pair, make_pair


using namespace MOBase;


SettingsTab::SettingsTab(Settings *m_parent, SettingsDialog &m_dialog)
  : m_parent(m_parent)
  , m_Settings(m_parent->settingsRef())
  , m_dialog(m_dialog)
  , ui(m_dialog.ui)
{
}

SettingsTab::~SettingsTab()
{}

QWidget* SettingsTab::parentWidget()
{
  return &m_dialog;
}


Settings *Settings::s_Instance = nullptr;


Settings::Settings(const QSettings &settingsSource)
  : m_Settings(settingsSource.fileName(), settingsSource.format())
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

QString Settings::getManagedGameDirectory() const
{
  return m_Settings.value("gamePath", "").toString();
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
  if (m_Settings.contains("Settings/steam_username")) {
    QString tempPass = deObfuscate("steam_password");
    if (!tempPass.isEmpty()) {
      username = m_Settings.value("Settings/steam_username").toString();
      password = tempPass;
      return true;
    }
  }
  return false;
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
  switch (m_Settings.value("Settings/load_mechanism").toInt()) {
    case LoadMechanism::LOAD_MODORGANIZER: return LoadMechanism::LOAD_MODORGANIZER;
    case LoadMechanism::LOAD_SCRIPTEXTENDER: return LoadMechanism::LOAD_SCRIPTEXTENDER;
    case LoadMechanism::LOAD_PROXYDLL: return LoadMechanism::LOAD_PROXYDLL;
  }
  throw std::runtime_error("invalid load mechanism");
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

void Settings::query(PluginContainer *pluginContainer, QWidget *parent)
{
  SettingsDialog dialog(pluginContainer, this, parent);

  std::vector<std::unique_ptr<SettingsTab>> tabs;

  tabs.push_back(std::unique_ptr<SettingsTab>(new GeneralSettingsTab(this, dialog)));
  tabs.push_back(std::unique_ptr<SettingsTab>(new PathsSettingsTab(this, dialog)));
  tabs.push_back(std::unique_ptr<SettingsTab>(new DiagnosticsTab(this, dialog)));
  tabs.push_back(std::unique_ptr<SettingsTab>(new NexusSettingsTab(this, dialog)));
  tabs.push_back(std::unique_ptr<SettingsTab>(new SteamTab(this, dialog)));
  tabs.push_back(std::unique_ptr<SettingsTab>(new PluginsTab(this, dialog)));
  tabs.push_back(std::unique_ptr<SettingsTab>(new WorkaroundsTab(this, dialog)));


  QString key = QString("geometry/%1").arg(dialog.objectName());
  if (m_Settings.contains(key)) {
    dialog.restoreGeometry(m_Settings.value(key).toByteArray());
  }

  if (dialog.exec() == QDialog::Accepted) {
    // remember settings before change
    QMap<QString, QString> before;
    m_Settings.beginGroup("Settings");
    for (auto k : m_Settings.allKeys())
      before[k] = m_Settings.value(k).toString();
    m_Settings.endGroup();

    // transfer modified settings to configuration file
    for (std::unique_ptr<SettingsTab> const &tab: tabs) {
      tab->update();
    }

    // print "changed" settings
    m_Settings.beginGroup("Settings");
    bool first_update = true;
    for (auto k : m_Settings.allKeys())
      if (m_Settings.value(k).toString() != before[k] && !k.contains("username") && !k.contains("password"))
      {
        if (first_update) {
          log::debug("Changed settings:");
          first_update = false;
        }
        log::debug("  {}={}", k, m_Settings.value(k).toString());
      }
    m_Settings.endGroup();
  }
  m_Settings.setValue(key, dialog.saveGeometry());

  // These changes happen regardless of accepted or rejected
  bool restartNeeded = false;
  if (dialog.getApiKeyChanged()) {
    restartNeeded = true;
  }
  if (dialog.getResetGeometries()) {
    restartNeeded = true;
    m_Settings.setValue("reset_geometry", true);
  }
  if (restartNeeded) {
    if (QMessageBox::question(nullptr,
      tr("Restart Mod Organizer?"),
      tr("In order to finish configuration changes, MO must be restarted.\n"
        "Restart it now?"),
      QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      qApp->exit(INT_MAX);
    }
  }

}


Settings::DiagnosticsTab::DiagnosticsTab(Settings *m_parent, SettingsDialog &m_dialog)
  : SettingsTab(m_parent, m_dialog)
  , m_logLevelBox(m_dialog.findChild<QComboBox *>("logLevelBox"))
  , m_dumpsTypeBox(m_dialog.findChild<QComboBox *>("dumpsTypeBox"))
  , m_dumpsMaxEdit(m_dialog.findChild<QSpinBox *>("dumpsMaxEdit"))
  , m_diagnosticsExplainedLabel(m_dialog.findChild<QLabel *>("diagnosticsExplainedLabel"))
{
  setLevelsBox();
  m_dumpsTypeBox->setCurrentIndex(m_parent->crashDumpsType());
  m_dumpsMaxEdit->setValue(m_parent->crashDumpsMax());
  QString logsPath = qApp->property("dataPath").toString()
    + "/" + QString::fromStdWString(AppConfig::logPath());
  m_diagnosticsExplainedLabel->setText(
    m_diagnosticsExplainedLabel->text()
    .replace("LOGS_FULL_PATH", logsPath)
    .replace("LOGS_DIR", QString::fromStdWString(AppConfig::logPath()))
    .replace("DUMPS_FULL_PATH", QString::fromStdWString(OrganizerCore::crashDumpsPath()))
    .replace("DUMPS_DIR", QString::fromStdWString(AppConfig::dumpsDir()))
  );
}

void Settings::DiagnosticsTab::update()
{
  m_Settings.setValue("Settings/log_level", m_logLevelBox->currentData().toInt());
  m_Settings.setValue("Settings/crash_dumps_type", m_dumpsTypeBox->currentIndex());
  m_Settings.setValue("Settings/crash_dumps_max", m_dumpsMaxEdit->value());
}


Settings::SteamTab::SteamTab(Settings *m_parent, SettingsDialog &m_dialog)
  : SettingsTab(m_parent, m_dialog)
  , m_steamUserEdit(m_dialog.findChild<QLineEdit *>("steamUserEdit"))
  , m_steamPassEdit(m_dialog.findChild<QLineEdit *>("steamPassEdit"))
{
  if (m_Settings.contains("Settings/steam_username")) {
    m_steamUserEdit->setText(m_Settings.value("Settings/steam_username", "").toString());
    QString password = deObfuscate("steam_password");
    if (!password.isEmpty()) {
      m_steamPassEdit->setText(password);
    }
  }
}

void Settings::SteamTab::update()
{
  //FIXME this should be inlined here?
  m_parent->setSteamLogin(m_steamUserEdit->text(), m_steamPassEdit->text());
}

Settings::PluginsTab::PluginsTab(Settings *m_parent, SettingsDialog &m_dialog)
  : SettingsTab(m_parent, m_dialog)
  , m_pluginsList(m_dialog.findChild<QListWidget *>("pluginsList"))
  , m_pluginBlacklistList(m_dialog.findChild<QListWidget *>("pluginBlacklist"))
{
  // display plugin settings
  QSet<QString> handledNames;
  for (IPlugin *plugin : m_parent->m_Plugins) {
    if (handledNames.contains(plugin->name()))
      continue;
    QListWidgetItem *listItem = new QListWidgetItem(plugin->name(), m_pluginsList);
    listItem->setData(Qt::UserRole, QVariant::fromValue((void*)plugin));
    listItem->setData(Qt::UserRole + 1, m_parent->m_PluginSettings[plugin->name()]);
    listItem->setData(Qt::UserRole + 2, m_parent->m_PluginDescriptions[plugin->name()]);
    m_pluginsList->addItem(listItem);
    handledNames.insert(plugin->name());
  }

  // display plugin blacklist
  for (const QString &pluginName : m_parent->m_PluginBlacklist) {
    m_pluginBlacklistList->addItem(pluginName);
  }
}

void Settings::PluginsTab::update()
{
  // transfer plugin settings to in-memory structure
  for (int i = 0; i < m_pluginsList->count(); ++i) {
    QListWidgetItem *item = m_pluginsList->item(i);
    m_parent->m_PluginSettings[item->text()] = item->data(Qt::UserRole + 1).toMap();
  }
  // store plugin settings on disc
  for (auto iterPlugins = m_parent->m_PluginSettings.begin(); iterPlugins != m_parent->m_PluginSettings.end(); ++iterPlugins) {
    for (auto iterSettings = iterPlugins->begin(); iterSettings != iterPlugins->end(); ++iterSettings) {
      m_Settings.setValue("Plugins/" + iterPlugins.key() + "/" + iterSettings.key(), iterSettings.value());
    }
  }

  // store plugin blacklist
  m_parent->m_PluginBlacklist.clear();
  for (QListWidgetItem *item : m_pluginBlacklistList->findItems("*", Qt::MatchWildcard)) {
    m_parent->m_PluginBlacklist.insert(item->text());
  }
  m_parent->writePluginBlacklist();
}

Settings::WorkaroundsTab::WorkaroundsTab(Settings *m_parent,
                                         SettingsDialog &m_dialog)
  : SettingsTab(m_parent, m_dialog)
  , m_appIDEdit(m_dialog.findChild<QLineEdit *>("appIDEdit"))
  , m_mechanismBox(m_dialog.findChild<QComboBox *>("mechanismBox"))
  , m_hideUncheckedBox(m_dialog.findChild<QCheckBox *>("hideUncheckedBox"))
  , m_forceEnableBox(m_dialog.findChild<QCheckBox *>("forceEnableBox"))
  , m_displayForeignBox(m_dialog.findChild<QCheckBox *>("displayForeignBox"))
  , m_lockGUIBox(m_dialog.findChild<QCheckBox *>("lockGUIBox"))
  , m_enableArchiveParsingBox(m_dialog.findChild<QCheckBox *>("enableArchiveParsingBox"))
  , m_resetGeometriesBtn(m_dialog.findChild<QPushButton *>("resetGeometryBtn"))
{
  m_appIDEdit->setText(m_parent->getSteamAppID());

  LoadMechanism::EMechanism mechanismID = m_parent->getLoadMechanism();
  int index = 0;

  if (m_parent->m_LoadMechanism.isDirectLoadingSupported()) {
    m_mechanismBox->addItem(QObject::tr("Mod Organizer"), LoadMechanism::LOAD_MODORGANIZER);
    if (mechanismID == LoadMechanism::LOAD_MODORGANIZER) {
      index = m_mechanismBox->count() - 1;
    }
  }

  if (m_parent->m_LoadMechanism.isScriptExtenderSupported()) {
    m_mechanismBox->addItem(QObject::tr("Script Extender"), LoadMechanism::LOAD_SCRIPTEXTENDER);
    if (mechanismID == LoadMechanism::LOAD_SCRIPTEXTENDER) {
      index = m_mechanismBox->count() - 1;
    }
  }

  if (m_parent->m_LoadMechanism.isProxyDLLSupported()) {
    m_mechanismBox->addItem(QObject::tr("Proxy DLL"), LoadMechanism::LOAD_PROXYDLL);
    if (mechanismID == LoadMechanism::LOAD_PROXYDLL) {
      index = m_mechanismBox->count() - 1;
    }
  }

  m_mechanismBox->setCurrentIndex(index);

  m_hideUncheckedBox->setChecked(m_parent->hideUncheckedPlugins());
  m_forceEnableBox->setChecked(m_parent->forceEnableCoreFiles());
  m_displayForeignBox->setChecked(m_parent->displayForeign());
  m_lockGUIBox->setChecked(m_parent->lockGUI());
  m_enableArchiveParsingBox->setChecked(m_parent->archiveParsing());

  m_resetGeometriesBtn->setChecked(m_parent->directInterface().value("reset_geometry", false).toBool());

  m_dialog.setExecutableBlacklist(m_parent->executablesBlacklist());

}

void Settings::WorkaroundsTab::update()
{
  if (m_appIDEdit->text() != m_parent->m_GamePlugin->steamAPPId()) {
    m_Settings.setValue("Settings/app_id", m_appIDEdit->text());
  } else {
    m_Settings.remove("Settings/app_id");
  }
  m_Settings.setValue("Settings/load_mechanism", m_mechanismBox->itemData(m_mechanismBox->currentIndex()).toInt());
  m_Settings.setValue("Settings/hide_unchecked_plugins", m_hideUncheckedBox->isChecked());
  m_Settings.setValue("Settings/force_enable_core_files", m_forceEnableBox->isChecked());
  m_Settings.setValue("Settings/display_foreign", m_displayForeignBox->isChecked());
  m_Settings.setValue("Settings/lock_gui", m_lockGUIBox->isChecked());
  m_Settings.setValue("Settings/archive_parsing_experimental", m_enableArchiveParsingBox->isChecked());

  m_Settings.setValue("Settings/executable_blacklist", m_dialog.getExecutableBlacklist());
}
