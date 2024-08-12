#include "extensionsettings.h"

#include "settingsutilities.h"

using namespace MOBase;

static const QString EXTENSIONS_GROUP         = "Extensions";
static const QString EXTENSIONS_ENABLED_GROUP = "ExtensionsEnabled";
static const QString PLUGINS_GROUP            = "Plugins";
static const QString PLUGINS_PERSISTENT_GROUP = "PluginPersistance";

ExtensionSettings::ExtensionSettings(QSettings& settings) : m_Settings(settings) {}

QString ExtensionSettings::path(const IExtension& extension, const Setting& setting)
{
  QString path = extension.metadata().identifier();
  if (!setting.group().isEmpty()) {
    path += "/" + setting.group();
  }
  return path + "/" + setting.name();
}

bool ExtensionSettings::isEnabled(const MOBase::IExtension& extension,
                                  bool defaultValue) const
{
  return get<bool>(m_Settings, EXTENSIONS_ENABLED_GROUP,
                   extension.metadata().identifier(), defaultValue);
}

void ExtensionSettings::setEnabled(const MOBase::IExtension& extension,
                                   bool enabled) const
{
  set(m_Settings, EXTENSIONS_ENABLED_GROUP, extension.metadata().identifier(), enabled);
}

QVariant ExtensionSettings::setting(const IExtension& extension,
                                    const Setting& setting) const
{
  return get<QVariant>(m_Settings, EXTENSIONS_GROUP, path(extension, setting),
                       setting.defaultValue());
}

void ExtensionSettings::setSetting(const IExtension& extension, const Setting& setting,
                                   const QVariant& value)
{
  set(m_Settings, EXTENSIONS_GROUP, path(extension, setting), value);
}

// commits all the settings to the ini
//
void ExtensionSettings::save()
{
  m_Settings.sync();
}

PluginSettings::PluginSettings(QSettings& settings) : m_Settings(settings) {}

QString PluginSettings::path(const QString& pluginName, const QString& key)
{
  return pluginName + "/" + key;
}

void PluginSettings::checkPluginSettings(const IPlugin* plugin) const
{
  for (const auto& setting : plugin->settings()) {
    const auto settingPath = path(plugin->name(), setting.name());

    QVariant temp = get<QVariant>(m_Settings, PLUGINS_GROUP, settingPath, QVariant());

    // No previous enabled? Skip.
    if (setting.name() == "enabled" && (!temp.isValid() || !temp.canConvert<bool>())) {
      continue;
    }

    if (!temp.isValid()) {
      temp = setting.defaultValue();
    } else if (!temp.convert(setting.defaultValue().metaType())) {
      log::warn("failed to interpret \"{}\" as correct type for \"{}\" in plugin "
                "\"{}\", using default",
                temp.toString(), setting.name(), plugin->name());

      temp = setting.defaultValue();
    }
  }
}

void PluginSettings::fixPluginEnabledSetting(const IPlugin* plugin)
{
  // handle previous "enabled" settings
  // TODO: keep this?
  const auto previousEnabledPath = plugin->name() + "/enabled";
  const QVariant previousEnabled =
      get<QVariant>(m_Settings, PLUGINS_GROUP, previousEnabledPath, QVariant());
  if (previousEnabled.isValid()) {
    setPersistent(plugin->name(), "enabled", previousEnabled.toBool(), true);

    // We need to drop it manually in Settings since it is not possible to remove
    // plugin settings:
    remove(m_Settings, PLUGINS_GROUP, previousEnabledPath);
  }
}

QVariant PluginSettings::setting(const QString& pluginName, const QString& key,
                                 const QVariant& defaultValue) const
{
  return get<QVariant>(m_Settings, PLUGINS_GROUP, path(pluginName, key), defaultValue);
}

void PluginSettings::setSetting(const QString& pluginName, const QString& key,
                                const QVariant& value)
{
  const auto settingPath = path(pluginName, key);
  const auto oldValue =
      get<QVariant>(m_Settings, PLUGINS_GROUP, settingPath, QVariant());
  set(m_Settings, PLUGINS_GROUP, settingPath, value);
  emit pluginSettingChanged(pluginName, key, oldValue, value);
}

QVariant PluginSettings::persistent(const QString& pluginName, const QString& key,
                                    const QVariant& def) const
{
  return get<QVariant>(m_Settings, "PluginPersistance", pluginName + "/" + key, def);
}

void PluginSettings::setPersistent(const QString& pluginName, const QString& key,
                                   const QVariant& value, bool sync)
{
  set(m_Settings, PLUGINS_PERSISTENT_GROUP, pluginName + "/" + key, value);

  if (sync) {
    m_Settings.sync();
  }
}

void PluginSettings::addBlacklist(const QString& fileName)
{
  m_PluginBlacklist.insert(fileName);
  writeBlacklist();
}

bool PluginSettings::blacklisted(const QString& fileName) const
{
  return m_PluginBlacklist.contains(fileName);
}

void PluginSettings::setBlacklist(const QStringList& pluginNames)
{
  m_PluginBlacklist.clear();

  for (const auto& name : pluginNames) {
    m_PluginBlacklist.insert(name);
  }
}

const QSet<QString>& PluginSettings::blacklist() const
{
  return m_PluginBlacklist;
}

void PluginSettings::save()
{
  m_Settings.sync();
  writeBlacklist();
}

void PluginSettings::writeBlacklist()
{
  const auto current = readBlacklist();

  if (current.size() > m_PluginBlacklist.size()) {
    // Qt can't remove array elements, the section must be cleared
    removeSection(m_Settings, "pluginBlacklist");
  }

  ScopedWriteArray swa(m_Settings, "pluginBlacklist", m_PluginBlacklist.size());

  for (const QString& plugin : m_PluginBlacklist) {
    swa.next();
    swa.set("name", plugin);
  }
}

QSet<QString> PluginSettings::readBlacklist() const
{
  QSet<QString> set;

  ScopedReadArray sra(m_Settings, "pluginBlacklist");
  sra.for_each([&] {
    set.insert(sra.get<QString>("name"));
  });

  return set;
}
