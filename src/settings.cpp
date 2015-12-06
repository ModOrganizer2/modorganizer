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

#include "settingsdialog.h"
#include "utility.h"
#include "helper.h"
#include <gameinfo.h>
#include <appconfig.h>
#include <utility.h>
#include <iplugingame.h>

#include <QCheckBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDirIterator>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegExp>

#include <memory>

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


static const unsigned char Key2[20] = { 0x99, 0xb8, 0x76, 0x42, 0x3e, 0xc1, 0x60, 0xa4, 0x5b, 0x01,
                                        0xdb, 0xf8, 0x43, 0x3a, 0xb7, 0xb6, 0x98, 0xd4, 0x7d, 0xa2 };

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
  std::wstring nxmPath = ToWString(QCoreApplication::applicationDirPath() + "/nxmhandler.exe");
  std::wstring executable = ToWString(QCoreApplication::applicationFilePath());
  std::wstring mode = force ? L"forcereg" : L"reg";
  std::wstring parameters = mode + L" " + m_GamePlugin->getGameShortName().toStdWString() + L" \"" + executable + L"\"";
  HINSTANCE res = ::ShellExecuteW(nullptr, L"open", nxmPath.c_str(), parameters.c_str(), nullptr, SW_SHOWNORMAL);
  if ((int)res <= 32) {
    QMessageBox::critical(nullptr, tr("Failed"),
                          tr("Sorry, failed to start the helper application"));
  }
}

void Settings::managedGameChanged(IPluginGame const *gamePlugin)
{
  m_GamePlugin = gamePlugin;
}

void Settings::registerPlugin(IPlugin *plugin)
{
  m_Plugins.push_back(plugin);
  m_PluginSettings.insert(plugin->name(), QMap<QString, QVariant>());
  m_PluginDescriptions.insert(plugin->name(), QMap<QString, QVariant>());
  foreach (const PluginSetting &setting, plugin->settings()) {
    QVariant temp = m_Settings.value("Plugins/" + plugin->name() + "/" + setting.key, setting.defaultValue);
    if (!temp.convert(setting.defaultValue.type())) {
      qWarning("failed to interpret \"%s\" as correct type for \"%s\" in plugin \"%s\", using default",
               qPrintable(temp.toString()), qPrintable(setting.key), qPrintable(plugin->name()));
      temp = setting.defaultValue;
    }
    m_PluginSettings[plugin->name()][setting.key] = temp;
    m_PluginDescriptions[plugin->name()][setting.key] = QString("%1 (default: %2)").arg(setting.description).arg(setting.defaultValue.toString());
  }
}

QString Settings::obfuscate(const QString &password)
{
  QByteArray temp = password.toUtf8();

  QByteArray buffer;
  for (int i = 0; i < temp.length(); ++i) {
    buffer.append(temp.at(i) ^ Key2[i % 20]);
  }
  return buffer.toBase64();
}

QString Settings::deObfuscate(const QString &password)
{
  QByteArray temp(QByteArray::fromBase64(password.toUtf8()));

  QByteArray buffer;
  for (int i = 0; i < temp.length(); ++i) {
    buffer.append(temp.at(i) ^ Key2[i % 20]);
  }
  return QString::fromUtf8(buffer.constData());
}

bool Settings::hideUncheckedPlugins() const
{
  return m_Settings.value("Settings/hide_unchecked_plugins", false).toBool();
}

bool Settings::forceEnableCoreFiles() const
{
  return m_Settings.value("Settings/force_enable_core_files", true).toBool();
}

bool Settings::automaticLoginEnabled() const
{
  return m_Settings.value("Settings/nexus_login", false).toBool();
}

QString Settings::getSteamAppID() const
{
  return m_Settings.value("Settings/app_id", m_GamePlugin->steamAPPId()).toString();
}

void Settings::setDownloadSpeed(const QString &serverName, int bytesPerSecond)
{
  m_Settings.beginGroup("Servers");

  foreach (const QString &serverKey, m_Settings.childKeys()) {
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

  foreach (const QString &serverKey, m_Settings.childKeys()) {
    QVariantMap data = m_Settings.value(serverKey).toMap();
    int preference = data["preferred"].toInt();
    if (preference > 0) {
      result[serverKey] = preference;
    }
  }
  m_Settings.endGroup();

  return result;
}

QString Settings::getConfigurablePath(const QString &key, const QString &def) const
{
  return QDir::fromNativeSeparators(m_Settings.value(QString("settings/") + key, qApp->property("dataPath").toString() + "/" + def).toString());
}

QString Settings::getDownloadDirectory() const
{
  return getConfigurablePath("download_directory", ToQString(AppConfig::downloadPath()));
}

QString Settings::getCacheDirectory() const
{
  return getConfigurablePath("cache_directory", ToQString(AppConfig::cachePath()));
}

QString Settings::getModDirectory() const
{
  return getConfigurablePath("mod_directory", ToQString(AppConfig::modsPath()));
}

QString Settings::getNMMVersion() const
{
  static const QString MIN_NMM_VERSION = "0.52.3";
  QString result = m_Settings.value("Settings/nmm_version", MIN_NMM_VERSION).toString();
  if (VersionInfo(result) < VersionInfo(MIN_NMM_VERSION)) {
    result = MIN_NMM_VERSION;
  }
  return result;
}

bool Settings::getNexusLogin(QString &username, QString &password) const
{
  if (m_Settings.value("Settings/nexus_login", false).toBool()) {
    username = m_Settings.value("Settings/nexus_username", "").toString();
    password = deObfuscate(m_Settings.value("Settings/nexus_password", "").toString());
    return true;
  } else {
    return false;
  }
}

bool Settings::getSteamLogin(QString &username, QString &password) const
{
  if (m_Settings.contains("Settings/steam_username")
      && m_Settings.contains("Settings/steam_password")) {
    username = m_Settings.value("Settings/steam_username").toString();
    password = deObfuscate(m_Settings.value("Settings/steam_password").toString());
    return true;
  } else {
    return false;
  }
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
  return m_Settings.value("Settings/log_level", 0).toInt();
}


void Settings::setNexusLogin(QString username, QString password)
{
  m_Settings.setValue("Settings/nexus_login", true);
  m_Settings.setValue("Settings/nexus_username", username);
  m_Settings.setValue("Settings/nexus_password", obfuscate(password));
}

void Settings::setSteamLogin(QString username, QString password)
{
  if (username == "") {
    m_Settings.remove("Settings/steam_username");
    password = "";
  } else {
    m_Settings.setValue("Settings/steam_username", username);
  }
  if (password == "") {
    m_Settings.remove("Settings/steam_password");
  } else {
    m_Settings.setValue("Settings/steam_password", obfuscate(password));
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

  foreach (const ServerInfo &server, servers) {
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
  foreach (const QString &key, m_Settings.childKeys()) {
    QVariantMap val = m_Settings.value(key).toMap();
    QDate lastSeen = val["lastSeen"].toDate();
    if (lastSeen.daysTo(now) > 30) {
      qDebug("removing server %s since it hasn't been available for downloads in over a month", qPrintable(key));
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
  m_Settings.beginWriteArray("pluginBlacklist");
  int idx = 0;
  foreach (const QString &plugin, m_PluginBlacklist) {
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
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
  styleBox->addItem("Fusion", "Fusion");
#else
  styleBox->addItem("Plastique", "Plastique");
  styleBox->addItem("Cleanlooks", "Cleanlooks");
#endif

  QDirIterator langIter(QCoreApplication::applicationDirPath() + "/" + ToQString(AppConfig::stylesheetsPath()), QStringList("*.qss"), QDir::Files);
  while (langIter.hasNext()) {
    langIter.next();
    QString style = langIter.fileName();
    styleBox->addItem(style, style);
  }
}

void Settings::resetDialogs()
{
  m_Settings.beginGroup("DialogChoices");
  QStringList keys = m_Settings.childKeys();
  foreach (QString key, keys) {
    m_Settings.remove(key);
  }

  m_Settings.endGroup();
}


void Settings::query(QWidget *parent)
{
  SettingsDialog dialog(parent);

  connect(&dialog, SIGNAL(resetDialogs()), this, SLOT(resetDialogs()));

  std::vector<std::unique_ptr<SettingsTab>> tabs;

  tabs.push_back(std::unique_ptr<SettingsTab>(new GeneralTab(this, dialog)));
  tabs.push_back(std::unique_ptr<SettingsTab>(new NexusTab(this, dialog)));
  tabs.push_back(std::unique_ptr<SettingsTab>(new SteamTab(this, dialog)));
  tabs.push_back(std::unique_ptr<SettingsTab>(new PluginsTab(this, dialog)));
  tabs.push_back(std::unique_ptr<SettingsTab>(new WorkaroundsTab(this, dialog)));

  if (dialog.exec() == QDialog::Accepted) {
    // transfer modified settings to configuration file
    for (std::unique_ptr<SettingsTab> const &tab: tabs) {
      tab->update();
    }
  }
}

Settings::SettingsTab::SettingsTab(Settings *m_parent, SettingsDialog &m_dialog) :
  m_parent(m_parent),
  m_Settings(m_parent->m_Settings),
  m_dialog(m_dialog)
{}

Settings::SettingsTab::~SettingsTab()
{}

Settings::GeneralTab::GeneralTab(Settings *m_parent, SettingsDialog &m_dialog) :
  Settings::SettingsTab(m_parent, m_dialog),
  m_languageBox(m_dialog.findChild<QComboBox*>("languageBox")),
  m_styleBox(m_dialog.findChild<QComboBox*>("styleBox")),
  m_logLevelBox(m_dialog.findChild<QComboBox*>("logLevelBox")),
  m_downloadDirEdit(m_dialog.findChild<QLineEdit*>("downloadDirEdit")),
  m_modDirEdit(m_dialog.findChild<QLineEdit*>("modDirEdit")),
  m_cacheDirEdit(m_dialog.findChild<QLineEdit*>("cacheDirEdit")),
  m_compactBox(m_dialog.findChild<QCheckBox*>("compactBox")),
  m_showMetaBox(m_dialog.findChild<QCheckBox*>("showMetaBox"))
  {
    //FIXME I think 'addLanguages' lives in here not in parent
    m_parent->addLanguages(m_languageBox);
    {
      QString languageCode = m_parent->language();
      int currentID = m_languageBox->findData(languageCode);
      // I made a mess. :( Most languages are stored with only the iso country code (2 characters like "de") but chinese
      // with the exact language variant (zh_TW) so I have to search for both variants
      if (currentID == -1) {
        currentID = m_languageBox->findData(languageCode.mid(0, 2));
      }
      if (currentID != -1) {
        m_languageBox->setCurrentIndex(currentID);
      }
    }

    //FIXME I think addStyles lives in here not in parent
    m_parent->addStyles(m_styleBox);
    {
      int currentID = m_styleBox->findData(m_Settings.value("Settings/style", "").toString());
      if (currentID != -1) {
        m_styleBox->setCurrentIndex(currentID);
      }
    }

    m_logLevelBox->setCurrentIndex(m_parent->logLevel());
    m_downloadDirEdit->setText(m_parent->getDownloadDirectory());
    m_modDirEdit->setText(m_parent->getModDirectory());
    m_cacheDirEdit->setText(m_parent->getCacheDirectory());
    m_compactBox->setChecked(m_parent->compactDownloads());
    m_showMetaBox->setChecked(m_parent->metaDownloads());
}

void Settings::GeneralTab::update()
{
  QString oldLanguage = m_Settings.value("Settings/language", "en_US").toString();
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

  m_Settings.setValue("Settings/log_level", m_logLevelBox->currentIndex());

  { // advanced settings
    if ((QDir::fromNativeSeparators(m_modDirEdit->text()) != QDir::fromNativeSeparators(m_parent->getModDirectory())) &&
        (QMessageBox::question(nullptr, tr("Confirm"), tr("Changing the mod directory affects all your profiles! "
                                                       "Mods not present (or named differently) in the new location will be disabled in all profiles. "
                                                       "There is no way to undo this unless you backed up your profiles manually. Proceed?"),
                               QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)) {
      m_modDirEdit->setText(m_parent->getModDirectory());
    }

    if (!QDir(m_downloadDirEdit->text()).exists()) {
      QDir().mkpath(m_downloadDirEdit->text());
    }
    if (QFileInfo(m_downloadDirEdit->text()) !=
        QFileInfo(qApp->property("dataPath").toString() + "/" + QString::fromStdWString(AppConfig::downloadPath()))) {
      m_Settings.setValue("Settings/download_directory", QDir::toNativeSeparators(m_downloadDirEdit->text()));
    } else {
      m_Settings.remove("Settings/download_directory");
    }

    if (!QDir(m_modDirEdit->text()).exists()) {
      QDir().mkpath(m_modDirEdit->text());
    }
    if (QFileInfo(m_modDirEdit->text()) !=
        QFileInfo(qApp->property("dataPath").toString() + "/" + QString::fromStdWString(AppConfig::modsPath()))) {
      m_Settings.setValue("Settings/mod_directory", QDir::toNativeSeparators(m_modDirEdit->text()));
    } else {
      m_Settings.remove("Settings/mod_directory");
    }

    if (!QDir(m_cacheDirEdit->text()).exists()) {
      QDir().mkpath(m_cacheDirEdit->text());
    }
    if (QFileInfo(m_cacheDirEdit->text()) !=
        QFileInfo(qApp->property("dataPath").toString() + "/" + QString::fromStdWString(AppConfig::cachePath()))) {
      m_Settings.setValue("Settings/cache_directory", QDir::toNativeSeparators(m_cacheDirEdit->text()));
    } else {
      m_Settings.remove("Settings/cache_directory");
    }
  }

  m_Settings.setValue("Settings/compact_downloads", m_compactBox->isChecked());
  m_Settings.setValue("Settings/meta_downloads", m_showMetaBox->isChecked());
}

Settings::NexusTab::NexusTab(Settings *m_parent, SettingsDialog &m_dialog) :
  Settings::SettingsTab(m_parent, m_dialog),
  m_loginCheckBox(m_dialog.findChild<QCheckBox*>("loginCheckBox")),
  m_usernameEdit(m_dialog.findChild<QLineEdit*>("usernameEdit")),
  m_passwordEdit(m_dialog.findChild<QLineEdit*>("passwordEdit")),
  m_offlineBox(m_dialog.findChild<QCheckBox*>("offlineBox")),
  m_proxyBox(m_dialog.findChild<QCheckBox*>("proxyBox")),
  m_knownServersList(m_dialog.findChild<QListWidget*>("knownServersList")),
  m_preferredServersList(m_dialog.findChild<QListWidget*>("preferredServersList"))
{
  if (m_parent->automaticLoginEnabled()) {
    m_loginCheckBox->setChecked(true);
    m_usernameEdit->setText(m_Settings.value("Settings/nexus_username", "").toString());
    m_passwordEdit->setText(deObfuscate(m_Settings.value("Settings/nexus_password", "").toString()));
  }

  m_offlineBox->setChecked(m_parent->offlineMode());
  m_proxyBox->setChecked(m_parent->useProxy());

  // display server preferences
  m_Settings.beginGroup("Servers");
  for (const QString &key : m_Settings.childKeys()) {
    QVariantMap val = m_Settings.value(key).toMap();
    QString type = val["premium"].toBool() ? "(premium)" : "(free)";

    QString descriptor = key + " " + type;
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
  if (m_loginCheckBox->isChecked()) {
    m_Settings.setValue("Settings/nexus_login", true);
    m_Settings.setValue("Settings/nexus_username", m_usernameEdit->text());
    m_Settings.setValue("Settings/nexus_password", obfuscate(m_passwordEdit->text()));
  } else {
    m_Settings.setValue("Settings/nexus_login", false);
    m_Settings.remove("Settings/nexus_username");
    m_Settings.remove("Settings/nexus_password");
  }
  m_Settings.setValue("Settings/offline_mode", m_offlineBox->isChecked());
  m_Settings.setValue("Settings/use_proxy", m_proxyBox->isChecked());

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


Settings::SteamTab::SteamTab(Settings *m_parent, SettingsDialog &m_dialog) :
  Settings::SettingsTab(m_parent, m_dialog),
  m_steamUserEdit(m_dialog.findChild<QLineEdit*>("steamUserEdit")),
  m_steamPassEdit(m_dialog.findChild<QLineEdit*>("steamPassEdit"))
{
  if (m_Settings.contains("Settings/steam_username")) {
    m_steamUserEdit->setText(m_Settings.value("Settings/steam_username", "").toString());
    if (m_Settings.contains("Settings/steam_password")) {
      m_steamPassEdit->setText(deObfuscate(m_Settings.value("Settings/steam_password", "").toString()));
    }
  }
}

void Settings::SteamTab::update()
{
  //FIXME this should be inlined here?
  m_parent->setSteamLogin(m_steamUserEdit->text(), m_steamPassEdit->text());
}

Settings::PluginsTab::PluginsTab(Settings *m_parent, SettingsDialog &m_dialog) :
  Settings::SettingsTab(m_parent, m_dialog),
  m_pluginsList(m_dialog.findChild<QListWidget*>("pluginsList")),
  m_pluginBlacklistList(m_dialog.findChild<QListWidget*>("pluginBlacklist"))
{
  // display plugin settings
  for (IPlugin *plugin : m_parent->m_Plugins) {
    QListWidgetItem *listItem = new QListWidgetItem(plugin->name(), m_pluginsList);
    listItem->setData(Qt::UserRole, QVariant::fromValue((void*)plugin));
    listItem->setData(Qt::UserRole + 1, m_parent->m_PluginSettings[plugin->name()]);
    listItem->setData(Qt::UserRole + 2, m_parent->m_PluginDescriptions[plugin->name()]);
    m_pluginsList->addItem(listItem);
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
  foreach (QListWidgetItem *item, m_pluginBlacklistList->findItems("*", Qt::MatchWildcard)) {
    m_parent->m_PluginBlacklist.insert(item->text());
  }
  m_parent->writePluginBlacklist();
}

Settings::WorkaroundsTab::WorkaroundsTab(Settings *m_parent, SettingsDialog &m_dialog) :
  Settings::SettingsTab(m_parent, m_dialog),
  m_appIDEdit(m_dialog.findChild<QLineEdit*>("appIDEdit")),
  m_mechanismBox(m_dialog.findChild<QComboBox*>("mechanismBox")),
  m_nmmVersionEdit(m_dialog.findChild<QLineEdit*>("nmmVersionEdit")),
  m_hideUncheckedBox(m_dialog.findChild<QCheckBox*>("hideUncheckedBox")),
  m_forceEnableBox(m_dialog.findChild<QCheckBox*>("forceEnableBox")),
  m_displayForeignBox(m_dialog.findChild<QCheckBox*>("displayForeignBox"))
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
}
