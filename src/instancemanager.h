#ifndef MODORGANIZER_INSTANCEMANAGER_INCLUDED
#define MODORGANIZER_INSTANCEMANAGER_INCLUDED

#include <QString>
#include <QSettings>

namespace MOBase { class IPluginGame; }

class Settings;
class PluginContainer;


class Instance
{
public:
  enum class SetupResults
  {
    Ok,
    BadIni,
    IniMissingGame,
    PluginGone,
    GameGone,
    MissingVariant
  };

  Instance(QDir dir, bool portable, QString profileName={});

  SetupResults setup(PluginContainer& plugins);

  void setGame(const QString& name, const QString& dir);
  void setVariant(const QString& name);

  QString name() const;
  QString gameName() const;
  QString gameDirectory() const;
  QDir directory() const;
  MOBase::IPluginGame* gamePlugin() const;
  QString profileName() const;
  QString iniPath() const;
  bool isPortable() const;

private:
  QDir m_dir;
  bool m_portable;
  QString m_gameName, m_gameDir, m_gameVariant;
  MOBase::IPluginGame* m_plugin;
  QString m_profile;

  SetupResults getGamePlugin(PluginContainer& plugins);
  void getProfile(const Settings& s);
};


class InstanceManager
{
public:
  static InstanceManager& singleton();

  void overrideInstance(const QString& instanceName);
  void overrideProfile(const QString& profileName);

  const MOBase::IPluginGame* gamePluginForDirectory(
    const QDir& dir, const PluginContainer& plugins) const;

  void clearCurrentInstance();
  std::optional<Instance> currentInstance() const;
  void setCurrentInstance(const QString &name);

  bool allowedToChangeInstance() const;
  static bool isPortablePath(const QString& dataPath);
  static QString portablePath();
  bool portableInstanceExists() const;

  QString instancesPath() const;
  QStringList instanceNames() const;
  std::vector<QDir> instancePaths() const;

  QString sanitizeInstanceName(const QString &name) const;
  QString makeUniqueName(const QString& instanceName) const;
  bool instanceExists(const QString& instanceName) const;
  bool validInstanceName(const QString& instanceName) const;
  QString instancePath(const QString& instanceName) const;
  static QString iniPath(const QDir& instanceDir);

private:
  InstanceManager();
  bool portableInstallIsLocked() const;

private:
  bool m_overrideInstance{false};
  QString m_overrideInstanceName;
  bool m_overrideProfile{false};
  QString m_overrideProfileName;
};

#endif  // MODORGANIZER_INSTANCEMANAGER_INCLUDED
