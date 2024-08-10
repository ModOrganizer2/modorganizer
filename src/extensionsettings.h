#ifndef EXTENSIONSETTINGS_H
#define EXTENSIONSETTINGS_H

#include <QObject>
#include <QSettings>

#include <uibase/iplugin.h>

// settings about plugins
//
class PluginSettings : public QObject
{
  Q_OBJECT

public:
  PluginSettings(QSettings& settings);

  // fix enabled settings from previous MO2 installation
  //
  void fixPluginEnabledSetting(const MOBase::IPlugin* plugin);

  // check that the settings stored for the given plugin are of the appropriate type,
  // warning user if not
  //
  void checkPluginSettings(const MOBase::IPlugin* plugin) const;

  // returns the plugin setting for the given key
  //
  QVariant setting(const QString& pluginName, const QString& key,
                   const QVariant& defaultValue = {}) const;

  // sets the plugin setting for the given key
  //
  void setSetting(const QString& pluginName, const QString& key, const QVariant& value);

  // get/set persistent settings
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

  // emitted when a plugin setting changes
  //
  void pluginSettingChanged(QString const& pluginName, const QString& key,
                            const QVariant& oldValue, const QVariant& newValue);

private:
  QSettings& m_Settings;
  QSet<QString> m_PluginBlacklist;

  // retrieve the path to the given setting
  //
  static QString path(const QString& pluginName, const QString& key);

  // commits the blacklist to the ini
  //
  void writeBlacklist();

  // reads the blacklist from the ini
  //
  QSet<QString> readBlacklist() const;
};

#endif
