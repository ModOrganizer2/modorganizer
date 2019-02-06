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
#include <QtDebug> // for qDebug, qWarning

#include <Windows.h> // For ShellExecuteW, HINSTANCE, etc
#include <wincred.h> // For storage

#include <algorithm> // for sort
#include <memory>
#include <stdexcept> // for runtime_error
#include <string>
#include <utility> // for pair, make_pair


using namespace MOBase;

template <typename T>
class QListWidgetItemEx : public QListWidgetItem {
public:
  QListWidgetItemEx(const QString &text, int sortRole = Qt::DisplayRole, QListWidget *parent = 0, int type = Type)
    : QListWidgetItem(text, parent, type), m_SortRole(sortRole) {}

  virtual bool operator< ( const QListWidgetItem & other ) const {
    return this->data(m_SortRole).value<T>() < other.data(m_SortRole).value<T>();
  }
private:
  int m_SortRole;
};

Settings *Settings::s_Instance = nullptr;


Settings::Settings(const QSettings &settingsSource)
  : m_Settings(settingsSource.fileName(), settingsSource.format())
{
  if (s_Instance != nullptr) {
    throw std::runtime_error("second instance of \"Settings\" created");
  } else {
    s_Instance = this;
  }

  qRegisterMetaType<Settings::NexusUpdateStrategy>("NexusUpdateStrategy");
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
  std::wstring nxmPath = ToWString(QCoreApplication::applicationDirPath() + "/nxmhandler.exe");
  std::wstring executable = ToWString(QCoreApplication::applicationFilePath());
  std::wstring mode = force ? L"forcereg" : L"reg";
  std::wstring parameters = mode + L" " + m_GamePlugin->gameShortName().toStdWString();
  for (QString altGame : m_GamePlugin->validShortNames()) {
    parameters += L"," + altGame.toStdWString();
  }
  parameters += L" \"" + executable + L"\"";
  HINSTANCE res = ::ShellExecuteW(nullptr, L"open", nxmPath.c_str(), parameters.c_str(), nullptr, SW_SHOWNORMAL);
  if ((INT_PTR)res <= 32) {
    QMessageBox::critical(nullptr, tr("Failed"),
                          tr("Sorry, failed to start the helper application"));
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
      qWarning("failed to interpret \"%s\" as correct type for \"%s\" in plugin \"%s\", using default",
               qUtf8Printable(temp.toString()), qUtf8Printable(setting.key), qUtf8Printable(plugin->name()));
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
    if (GetLastError() != ERROR_NOT_FOUND) {
      wchar_t buffer[256];
      FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buffer, (sizeof(buffer) / sizeof(wchar_t)), NULL);
      qCritical() << "Retrieving encrypted data failed:" << buffer;
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

QString Settings::getNMMVersion() const
{
  static const QString MIN_NMM_VERSION = "0.65.2";
  QString result = m_Settings.value("Settings/nmm_version", MIN_NMM_VERSION).toString();
  if (VersionInfo(result) < VersionInfo(MIN_NMM_VERSION)) {
    result = MIN_NMM_VERSION;
  }
  return result;
}

bool Settings::getNexusApiKey(QString &apiKey) const
{
  QString tempKey = deObfuscate("APIKEY");
  if (tempKey.isEmpty())
    return false;
  apiKey = tempKey;
  return true;
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

int Settings::logLevel() const
{
  return m_Settings.value("Settings/log_level", static_cast<int>(LogLevel::Info)).toInt();
}

int Settings::crashDumpsType() const
{
  return m_Settings.value("Settings/crash_dumps_type", static_cast<int>(CrashDumpsType::Mini)).toInt();
}

int Settings::crashDumpsMax() const
{
  return m_Settings.value("Settings/crash_dumps_max", 5).toInt();
}

Settings::NexusUpdateStrategy Settings::nexusUpdateStrategy() const
{
  return static_cast<NexusUpdateStrategy>(m_Settings.value("Settings/nexus_update_strategy", std::rand() / ((RAND_MAX + 1u) / 2)).toInt());
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

void Settings::setNexusApiKey(QString apiKey)
{
  if (!obfuscate("APIKEY", apiKey)) {
    wchar_t buffer[256];
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      buffer, (sizeof(buffer) / sizeof(wchar_t)), NULL);
    qCritical() << "Storing API key failed:" << buffer;
  }
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
    wchar_t buffer[256];
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      buffer, (sizeof(buffer) / sizeof(wchar_t)), NULL);
    qCritical() << "Storing or deleting password failed:" << buffer;
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
      qDebug("removing server %s since it hasn't been available for downloads in over a month", qUtf8Printable(key));
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

void Settings::addLanguages(QComboBox *languageBox)
{
  std::vector<std::pair<QString, QString>> languages;

  QDirIterator langIter(QCoreApplication::applicationDirPath() + "/translations", QDir::Files);
  QString pattern = ToQString(AppConfig::translationPrefix()) +  "_([a-z]{2,3}(_[A-Z]{2,2})?).qm";
  QRegExp exp(pattern);
  while (langIter.hasNext()) {
    langIter.next();
    QString file = langIter.fileName();
    if (exp.exactMatch(file)) {
      QString languageCode = exp.cap(1);
      QLocale locale(languageCode);
      QString languageString = QString("%1 (%2)").arg(locale.nativeLanguageName()).arg(locale.nativeCountryName());  //QLocale::languageToString(locale.language());
      if (locale.language() == QLocale::Chinese) {
        if (languageCode == "zh_TW") {
          languageString = "Chinese (traditional)";
        } else {
          languageString = "Chinese (simplified)";
        }
      }
      languages.push_back(std::make_pair(QString("%1").arg(languageString), exp.cap(1)));
      //languageBox->addItem(QString("%1").arg(languageString), exp.cap(1));
    }
  }
  if (!languageBox->findText("English")) {
    languages.push_back(std::make_pair(QString("English"), QString("en_US")));
    //languageBox->addItem("English", "en_US");
  }
  std::sort(languages.begin(), languages.end());
  for (const auto &lang : languages) {
    languageBox->addItem(lang.first, lang.second);
  }
}

void Settings::addStyles(QComboBox *styleBox)
{
  styleBox->addItem("None", "");
  styleBox->addItem("Fusion", "Fusion");

  QDirIterator langIter(QCoreApplication::applicationDirPath() + "/" + ToQString(AppConfig::stylesheetsPath()), QStringList("*.qss"), QDir::Files);
  while (langIter.hasNext()) {
    langIter.next();
    QString style = langIter.fileName();
    styleBox->addItem(style, style);
  }
}

void Settings::resetDialogs()
{
  QuestionBoxMemory::resetDialogs();
}

void Settings::processApiKey(const QString &apiKey)
{
  if (!obfuscate("APIKEY", apiKey)) {
    wchar_t buffer[256];
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      buffer, (sizeof(buffer) / sizeof(wchar_t)), NULL);
    qCritical() << "Storing or deleting API key failed:" << buffer;
  }
}

void Settings::clearApiKey(QPushButton *nexusButton)
{
  obfuscate("APIKEY", "");
  nexusButton->setEnabled(true);
  nexusButton->setText("Connect to Nexus");
}

void Settings::checkApiKey(QPushButton *nexusButton)
{
  if (deObfuscate("APIKEY").isEmpty()) {
    nexusButton->setEnabled(true);
    nexusButton->setText("Connect to Nexus");
    QMessageBox::warning(qApp->activeWindow(), tr("Error"),
      tr("Failed to retrieve a Nexus API key! Please try again."
        "A browser window should open asking you to authorize."));
  }
}

void Settings::query(PluginContainer *pluginContainer, QWidget *parent)
{
  SettingsDialog dialog(pluginContainer, parent);

  connect(&dialog, SIGNAL(resetDialogs()), this, SLOT(resetDialogs()));
  connect(&dialog, SIGNAL(processApiKey(const QString &)), this, SLOT(processApiKey(const QString &)));
  connect(&dialog, SIGNAL(closeApiConnection(QPushButton *)), this, SLOT(checkApiKey(QPushButton *)));
  connect(&dialog, SIGNAL(revokeApiKey(QPushButton *)), this, SLOT(clearApiKey(QPushButton *)));

  std::vector<std::unique_ptr<SettingsTab>> tabs;

  tabs.push_back(std::unique_ptr<SettingsTab>(new GeneralTab(this, dialog)));
  tabs.push_back(std::unique_ptr<SettingsTab>(new PathsTab(this, dialog)));
  tabs.push_back(std::unique_ptr<SettingsTab>(new DiagnosticsTab(this, dialog)));
  tabs.push_back(std::unique_ptr<SettingsTab>(new NexusTab(this, dialog)));
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
          qDebug("Changed settings:");
          first_update = false;
        }
        qDebug("  %s=%s", k.toUtf8().data(), m_Settings.value(k).toString().toUtf8().data());
      }
    m_Settings.endGroup();
  }
  m_Settings.setValue(key, dialog.saveGeometry());

}

Settings::SettingsTab::SettingsTab(Settings *m_parent, SettingsDialog &m_dialog)
  : m_parent(m_parent)
  , m_Settings(m_parent->m_Settings)
  , m_dialog(m_dialog)
{
}

Settings::SettingsTab::~SettingsTab()
{}

Settings::GeneralTab::GeneralTab(Settings *m_parent, SettingsDialog &m_dialog)
  : Settings::SettingsTab(m_parent, m_dialog)
  , m_languageBox(m_dialog.findChild<QComboBox *>("languageBox"))
  , m_styleBox(m_dialog.findChild<QComboBox *>("styleBox"))
  , m_compactBox(m_dialog.findChild<QCheckBox *>("compactBox"))
  , m_showMetaBox(m_dialog.findChild<QCheckBox *>("showMetaBox"))
  , m_usePrereleaseBox(m_dialog.findChild<QCheckBox *>("usePrereleaseBox"))
  , m_overwritingBtn(m_dialog.findChild<QPushButton *>("overwritingBtn"))
  , m_overwrittenBtn(m_dialog.findChild<QPushButton *>("overwrittenBtn"))
  , m_overwritingArchiveBtn(m_dialog.findChild<QPushButton *>("overwritingArchiveBtn"))
  , m_overwrittenArchiveBtn(m_dialog.findChild<QPushButton *>("overwrittenArchiveBtn"))
  , m_containsBtn(m_dialog.findChild<QPushButton *>("containsBtn"))
  , m_containedBtn(m_dialog.findChild<QPushButton *>("containedBtn"))
  , m_colorSeparatorsBox(m_dialog.findChild<QCheckBox *>("colorSeparatorsBox"))
{
  // FIXME I think 'addLanguages' lives in here not in parent
  m_parent->addLanguages(m_languageBox);
  {
    QString languageCode = m_parent->language();
    int currentID        = m_languageBox->findData(languageCode);
    // I made a mess. :( Most languages are stored with only the iso country
    // code (2 characters like "de") but chinese
    // with the exact language variant (zh_TW) so I have to search for both
    // variants
    if (currentID == -1) {
      currentID = m_languageBox->findData(languageCode.mid(0, 2));
    }
    if (currentID != -1) {
      m_languageBox->setCurrentIndex(currentID);
    }
  }

  // FIXME I think addStyles lives in here not in parent
  m_parent->addStyles(m_styleBox);
  {
    int currentID = m_styleBox->findData(
        m_Settings.value("Settings/style", "").toString());
    if (currentID != -1) {
      m_styleBox->setCurrentIndex(currentID);
    }
  }
  /* verision using palette only works with fusion theme for some stupid reason...
  m_overwritingBtn->setAutoFillBackground(true);
  m_overwrittenBtn->setAutoFillBackground(true);
  m_containsBtn->setAutoFillBackground(true);
  m_containedBtn->setAutoFillBackground(true);
  m_overwritingBtn->setPalette(QPalette(m_parent->modlistOverwritingLooseColor()));
  m_overwrittenBtn->setPalette(QPalette(m_parent->modlistOverwrittenLooseColor()));
  m_containsBtn->setPalette(QPalette(m_parent->modlistContainsPluginColor()));
  m_containedBtn->setPalette(QPalette(m_parent->pluginListContainedColor()));
  QPalette palette1 = m_overwritingBtn->palette();
  QPalette palette2 = m_overwrittenBtn->palette();
  QPalette palette3 = m_containsBtn->palette();
  QPalette palette4 = m_containedBtn->palette();
  palette1.setColor(QPalette::Background, m_parent->modlistOverwritingLooseColor());
  palette2.setColor(QPalette::Background, m_parent->modlistOverwrittenLooseColor());
  palette3.setColor(QPalette::Background, m_parent->modlistContainsPluginColor());
  palette4.setColor(QPalette::Background, m_parent->pluginListContainedColor());
  m_overwritingBtn->setPalette(palette1);
  m_overwrittenBtn->setPalette(palette2);
  m_containsBtn->setPalette(palette3);
  m_containedBtn->setPalette(palette4);
  */

  //version with stylesheet
  m_dialog.setButtonColor(m_overwritingBtn, m_parent->modlistOverwritingLooseColor());
  m_dialog.setButtonColor(m_overwrittenBtn, m_parent->modlistOverwrittenLooseColor());
  m_dialog.setButtonColor(m_overwritingArchiveBtn, m_parent->modlistOverwritingArchiveColor());
  m_dialog.setButtonColor(m_overwrittenArchiveBtn, m_parent->modlistOverwrittenArchiveColor());
  m_dialog.setButtonColor(m_containsBtn, m_parent->modlistContainsPluginColor());
  m_dialog.setButtonColor(m_containedBtn, m_parent->pluginListContainedColor());

  m_dialog.setOverwritingColor(m_parent->modlistOverwritingLooseColor());
  m_dialog.setOverwrittenColor(m_parent->modlistOverwrittenLooseColor());
  m_dialog.setOverwritingArchiveColor(m_parent->modlistOverwritingArchiveColor());
  m_dialog.setOverwrittenArchiveColor(m_parent->modlistOverwrittenArchiveColor());
  m_dialog.setContainsColor(m_parent->modlistContainsPluginColor());
  m_dialog.setContainedColor(m_parent->pluginListContainedColor());

  m_compactBox->setChecked(m_parent->compactDownloads());
  m_showMetaBox->setChecked(m_parent->metaDownloads());
  m_usePrereleaseBox->setChecked(m_parent->usePrereleases());
  m_colorSeparatorsBox->setChecked(m_parent->colorSeparatorScrollbar());
}

void Settings::GeneralTab::update()
{
  QString oldLanguage = m_parent->language();
  QString newLanguage = m_languageBox->itemData(m_languageBox->currentIndex()).toString();
  if (newLanguage != oldLanguage) {
    m_Settings.setValue("Settings/language", newLanguage);
    emit m_parent->languageChanged(newLanguage);
  }

  QString oldStyle = m_Settings.value("Settings/style", "").toString();
  QString newStyle = m_styleBox->itemData(m_styleBox->currentIndex()).toString();
  if (oldStyle != newStyle) {
    m_Settings.setValue("Settings/style", newStyle);
    emit m_parent->styleChanged(newStyle);
  }

  m_Settings.setValue("Settings/overwritingLooseFilesColor", m_dialog.getOverwritingColor());
  m_Settings.setValue("Settings/overwrittenLooseFilesColor", m_dialog.getOverwrittenColor());
  m_Settings.setValue("Settings/overwritingArchiveFilesColor", m_dialog.getOverwritingArchiveColor());
  m_Settings.setValue("Settings/overwrittenArchiveFilesColor", m_dialog.getOverwrittenArchiveColor());
  m_Settings.setValue("Settings/containsPluginColor", m_dialog.getContainsColor());
  m_Settings.setValue("Settings/containedColor", m_dialog.getContainedColor());
  m_Settings.setValue("Settings/compact_downloads", m_compactBox->isChecked());
  m_Settings.setValue("Settings/meta_downloads", m_showMetaBox->isChecked());
  m_Settings.setValue("Settings/use_prereleases", m_usePrereleaseBox->isChecked());
  m_Settings.setValue("Settings/colorSeparatorScrollbars", m_colorSeparatorsBox->isChecked());
}

Settings::PathsTab::PathsTab(Settings *parent, SettingsDialog &dialog)
  : SettingsTab(parent, dialog)
  , m_baseDirEdit(m_dialog.findChild<QLineEdit *>("baseDirEdit"))
  , m_downloadDirEdit(m_dialog.findChild<QLineEdit *>("downloadDirEdit"))
  , m_modDirEdit(m_dialog.findChild<QLineEdit *>("modDirEdit"))
  , m_cacheDirEdit(m_dialog.findChild<QLineEdit *>("cacheDirEdit"))
  , m_profilesDirEdit(m_dialog.findChild<QLineEdit *>("profilesDirEdit"))
  , m_overwriteDirEdit(m_dialog.findChild<QLineEdit *>("overwriteDirEdit"))
  , m_managedGameDirEdit(m_dialog.findChild<QLineEdit *>("managedGameDirEdit"))
{
  m_baseDirEdit->setText(m_parent->getBaseDirectory());
  m_managedGameDirEdit->setText(m_parent->m_GamePlugin->gameDirectory().absoluteFilePath(m_parent->m_GamePlugin->binaryName()));
  QString basePath = parent->getBaseDirectory();
  QDir baseDir(basePath);
  for (const auto &dir : {
       std::make_pair(m_downloadDirEdit, m_parent->getDownloadDirectory(false)),
       std::make_pair(m_modDirEdit, m_parent->getModDirectory(false)),
       std::make_pair(m_cacheDirEdit, m_parent->getCacheDirectory(false)),
       std::make_pair(m_profilesDirEdit, m_parent->getProfileDirectory(false)),
       std::make_pair(m_overwriteDirEdit, m_parent->getOverwriteDirectory(false))
      }) {
    QString storePath = baseDir.relativeFilePath(dir.second);
    storePath = dir.second;
    dir.first->setText(storePath);
  }
}

void Settings::PathsTab::update()
{
  typedef std::tuple<QString, QString, std::wstring> Directory;

  QString basePath = m_parent->getBaseDirectory();

  for (const Directory &dir :{
       Directory{m_downloadDirEdit->text(), "download_directory", AppConfig::downloadPath()},
       Directory{m_cacheDirEdit->text(), "cache_directory", AppConfig::cachePath()},
       Directory{m_modDirEdit->text(), "mod_directory", AppConfig::modsPath()},
       Directory{m_overwriteDirEdit->text(), "overwrite_directory", AppConfig::overwritePath()},
       Directory{m_profilesDirEdit->text(), "profiles_directory", AppConfig::profilesPath()}
      }) {
    QString path, settingsKey;
    std::wstring defaultName;
    std::tie(path, settingsKey, defaultName) = dir;

    settingsKey = QString("Settings/%1").arg(settingsKey);

    QString realPath = path;
    realPath.replace("%BASE_DIR%", m_baseDirEdit->text());

    if (!QDir(realPath).exists()) {
      if (!QDir().mkpath(realPath)) {
        QMessageBox::warning(qApp->activeWindow(), tr("Error"),
                             tr("Failed to create \"%1\", you may not have the "
                                "necessary permission. path remains unchanged.")
                                 .arg(realPath));
      }
    }

    if (QFileInfo(realPath)
        != QFileInfo(basePath + "/" + QString::fromStdWString(defaultName))) {
      m_Settings.setValue(settingsKey, path);
    } else {
      m_Settings.remove(settingsKey);
    }
  }

  if (QFileInfo(m_baseDirEdit->text()) !=
      QFileInfo(qApp->property("dataPath").toString())) {
    m_Settings.setValue("Settings/base_directory", m_baseDirEdit->text());
  } else {
    m_Settings.remove("Settings/base_directory");
  }

  QFileInfo oldGameExe(m_parent->m_GamePlugin->gameDirectory().absoluteFilePath(m_parent->m_GamePlugin->binaryName()));
  QFileInfo newGameExe(m_managedGameDirEdit->text());
  if (oldGameExe != newGameExe) {
    m_Settings.setValue("gamePath", newGameExe.absolutePath());
  }
}

Settings::DiagnosticsTab::DiagnosticsTab(Settings *m_parent, SettingsDialog &m_dialog)
  : Settings::SettingsTab(m_parent, m_dialog)
  , m_logLevelBox(m_dialog.findChild<QComboBox *>("logLevelBox"))
  , m_dumpsTypeBox(m_dialog.findChild<QComboBox *>("dumpsTypeBox"))
  , m_dumpsMaxEdit(m_dialog.findChild<QSpinBox *>("dumpsMaxEdit"))
  , m_diagnosticsExplainedLabel(m_dialog.findChild<QLabel *>("diagnosticsExplainedLabel"))
{
  m_logLevelBox->setCurrentIndex(m_parent->logLevel());
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
  m_Settings.setValue("Settings/log_level", m_logLevelBox->currentIndex());
  m_Settings.setValue("Settings/crash_dumps_type", m_dumpsTypeBox->currentIndex());
  m_Settings.setValue("Settings/crash_dumps_max", m_dumpsMaxEdit->value());
}

Settings::NexusTab::NexusTab(Settings *parent, SettingsDialog &dialog)
  : Settings::SettingsTab(parent, dialog)
  , m_nexusConnect(dialog.findChild<QPushButton *>("nexusConnect"))
  , m_offlineBox(dialog.findChild<QCheckBox *>("offlineBox"))
  , m_proxyBox(dialog.findChild<QCheckBox *>("proxyBox"))
  , m_knownServersList(dialog.findChild<QListWidget *>("knownServersList"))
  , m_preferredServersList(
        dialog.findChild<QListWidget *>("preferredServersList"))
  , m_endorsementBox(dialog.findChild<QCheckBox *>("endorsementBox"))
  , m_updateStrategyBox(dialog.findChild<QCheckBox *>("updateStrategy"))
{
  qRegisterMetaType<Settings::NexusUpdateStrategy>("NexusUpdateStrategy");

  if (!deObfuscate("APIKEY").isEmpty()) {
    m_nexusConnect->setText("Nexus API Key Stored");
    m_nexusConnect->setDisabled(true);
  }

  m_offlineBox->setChecked(parent->offlineMode());
  m_proxyBox->setChecked(parent->useProxy());
  m_endorsementBox->setChecked(parent->endorsementIntegration());
  if (parent->nexusUpdateStrategy() == Settings::NexusUpdateStrategy::Flexible)
    m_updateStrategyBox->setChecked(true);


  // display server preferences
  m_Settings.beginGroup("Servers");
  for (const QString &key : m_Settings.childKeys()) {
    QVariantMap val = m_Settings.value(key).toMap();
    QString descriptor = key;
    if (!descriptor.compare("CDN", Qt::CaseInsensitive)) {
      descriptor += QStringLiteral(" (automatic)");
    }
    if (val.contains("downloadSpeed") && val.contains("downloadCount") && (val["downloadCount"].toInt() > 0)) {
      int bps = static_cast<int>(val["downloadSpeed"].toDouble() / val["downloadCount"].toInt());
      descriptor += QString(" (%1 kbps)").arg(bps / 1024);
    }

    QListWidgetItem *newItem = new QListWidgetItemEx<int>(descriptor, Qt::UserRole + 1);

    newItem->setData(Qt::UserRole, key);
    newItem->setData(Qt::UserRole + 1, val["preferred"].toInt());
    if (val["preferred"].toInt() > 0) {
      m_preferredServersList->addItem(newItem);
    } else {
      m_knownServersList->addItem(newItem);
    }
    m_preferredServersList->sortItems(Qt::DescendingOrder);
  }
  m_Settings.endGroup();
}

void Settings::NexusTab::update()
{
  /*
  if (m_loginCheckBox->isChecked()) {
    m_Settings.setValue("Settings/nexus_login", true);
    m_Settings.setValue("Settings/nexus_username", m_usernameEdit->text());
    m_Settings.setValue("Settings/nexus_password", obfuscate(m_passwordEdit->text()));
  } else {
    m_Settings.setValue("Settings/nexus_login", false);
    m_Settings.remove("Settings/nexus_username");
    m_Settings.remove("Settings/nexus_password");
  }
  */
  m_Settings.setValue("Settings/offline_mode", m_offlineBox->isChecked());
  m_Settings.setValue("Settings/use_proxy", m_proxyBox->isChecked());
  m_Settings.setValue("Settings/endorsement_integration", m_endorsementBox->isChecked());
  m_Settings.setValue("Settings/nexus_update_strategy", m_updateStrategyBox->isChecked()
    ? Settings::NexusUpdateStrategy::Flexible : Settings::NexusUpdateStrategy::Rigid);

  // store server preference
  m_Settings.beginGroup("Servers");
  for (int i = 0; i < m_knownServersList->count(); ++i) {
    QString key = m_knownServersList->item(i)->data(Qt::UserRole).toString();
    QVariantMap val = m_Settings.value(key).toMap();
    val["preferred"] = 0;
    m_Settings.setValue(key, val);
  }
  int count = m_preferredServersList->count();
  for (int i = 0; i < count; ++i) {
    QString key = m_preferredServersList->item(i)->data(Qt::UserRole).toString();
    QVariantMap val = m_Settings.value(key).toMap();
    val["preferred"] = count - i;
    m_Settings.setValue(key, val);
  }
  m_Settings.endGroup();
}

Settings::SteamTab::SteamTab(Settings *m_parent, SettingsDialog &m_dialog)
  : Settings::SettingsTab(m_parent, m_dialog)
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
  : Settings::SettingsTab(m_parent, m_dialog)
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
  : Settings::SettingsTab(m_parent, m_dialog)
  , m_appIDEdit(m_dialog.findChild<QLineEdit *>("appIDEdit"))
  , m_mechanismBox(m_dialog.findChild<QComboBox *>("mechanismBox"))
  , m_nmmVersionEdit(m_dialog.findChild<QLineEdit *>("nmmVersionEdit"))
  , m_hideUncheckedBox(m_dialog.findChild<QCheckBox *>("hideUncheckedBox"))
  , m_forceEnableBox(m_dialog.findChild<QCheckBox *>("forceEnableBox"))
  , m_displayForeignBox(m_dialog.findChild<QCheckBox *>("displayForeignBox"))
  , m_lockGUIBox(m_dialog.findChild<QCheckBox *>("lockGUIBox"))
  , m_enableArchiveParsingBox(m_dialog.findChild<QCheckBox *>("enableArchiveParsingBox"))
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

  m_nmmVersionEdit->setText(m_parent->getNMMVersion());
  m_hideUncheckedBox->setChecked(m_parent->hideUncheckedPlugins());
  m_forceEnableBox->setChecked(m_parent->forceEnableCoreFiles());
  m_displayForeignBox->setChecked(m_parent->displayForeign());
  m_lockGUIBox->setChecked(m_parent->lockGUI());
  m_enableArchiveParsingBox->setChecked(m_parent->archiveParsing());

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
  m_Settings.setValue("Settings/nmm_version", m_nmmVersionEdit->text());
  m_Settings.setValue("Settings/hide_unchecked_plugins", m_hideUncheckedBox->isChecked());
  m_Settings.setValue("Settings/force_enable_core_files", m_forceEnableBox->isChecked());
  m_Settings.setValue("Settings/display_foreign", m_displayForeignBox->isChecked());
  m_Settings.setValue("Settings/lock_gui", m_lockGUIBox->isChecked());
  m_Settings.setValue("Settings/archive_parsing_experimental", m_enableArchiveParsingBox->isChecked());

  m_Settings.setValue("Settings/executable_blacklist", m_dialog.getExecutableBlacklist());

  if (m_dialog.getResetGeometries()) {
    m_Settings.setValue("reset_geometry", true);
    if (QMessageBox::question(nullptr,
          tr("Restart Mod Organizer?"),
          tr("In order to reset the window geometries, MO must be restarted.\n"
             "Restart it now?"),
          QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      qApp->exit(INT_MAX);
    }
  }
}
