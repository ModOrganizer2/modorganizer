#ifndef MODINFOOVERWRITE_H
#define MODINFOOVERWRITE_H

#include <limits>

#include "modinfowithconflictinfo.h"

#include <QDateTime>

class ModInfoOverwrite : public ModInfoWithConflictInfo
{

  Q_OBJECT

  friend class ModInfo;

public:
  virtual bool updateAvailable() const override { return false; }
  virtual bool updateIgnored() const override { return false; }
  virtual bool downgradeAvailable() const override { return false; }
  virtual bool updateNXMInfo() override { return false; }
  virtual void setCategory(int, bool) override {}
  virtual bool setName(const QString&) override { return false; }
  virtual void setComments(const QString&) override {}
  virtual void setNotes(const QString&) override {}
  virtual void setGameName(const QString& gameName) override {}
  virtual void setNexusID(int) override {}
  virtual void setNewestVersion(const MOBase::VersionInfo&) override {}
  virtual void ignoreUpdate(bool) override {}
  virtual void setNexusDescription(const QString&) override {}
  virtual void setInstallationFile(const QString&) override {}
  virtual void addNexusCategory(int) override {}
  virtual void setIsEndorsed(bool) override {}
  virtual void setNeverEndorse() override {}
  virtual void setIsTracked(bool) override {}
  virtual void endorse(bool) override {}
  virtual void track(bool) override {}
  virtual bool alwaysEnabled() const override { return true; }
  virtual bool isEmpty() const override;
  virtual QString name() const override { return "Overwrite"; }
  virtual QString comments() const override { return ""; }
  virtual QString notes() const override { return ""; }
  virtual QDateTime creationTime() const override { return QDateTime(); }
  virtual QString absolutePath() const override;
  virtual MOBase::VersionInfo newestVersion() const override { return QString(); }
  virtual MOBase::VersionInfo ignoredVersion() const override { return QString(); }
  virtual QString installationFile() const override { return ""; }
  virtual bool converted() const override { return false; }
  virtual bool validated() const override { return false; }
  virtual QString gameName() const override { return ""; }
  virtual int nexusId() const override { return -1; }
  virtual bool isOverwrite() const override { return true; }
  virtual QDateTime getExpires() const override { return QDateTime(); }
  virtual std::vector<QString> getIniTweaks() const override
  {
    return std::vector<QString>();
  }
  virtual std::vector<ModInfo::EFlag> getFlags() const override;
  virtual std::vector<ModInfo::EConflictFlag> getConflictFlags() const override;
  virtual int getHighlight() const override;
  virtual QString getDescription() const override;
  virtual int getNexusFileStatus() const override { return 0; }
  virtual void setNexusFileStatus(int) override {}
  virtual QDateTime getLastNexusUpdate() const override { return QDateTime(); }
  virtual void setLastNexusUpdate(QDateTime) override {}
  virtual QDateTime getLastNexusQuery() const override { return QDateTime(); }
  virtual void setLastNexusQuery(QDateTime) override {}
  virtual QDateTime getNexusLastModified() const override { return QDateTime(); }
  virtual void setNexusLastModified(QDateTime) override {}
  virtual QString getNexusDescription() const override { return QString(); }
  virtual QStringList archives(bool checkOnDisk = false) override;
  virtual void addInstalledFile(int, int) override {}
  virtual std::set<std::pair<int, int>> installedFiles() const override { return {}; }

  virtual QVariant pluginSetting(const QString& pluginName, const QString& key,
                                 const QVariant& defaultValue) const override
  {
    return defaultValue;
  }
  virtual std::map<QString, QVariant>
  pluginSettings(const QString& pluginName) const override
  {
    return {};
  }
  virtual bool setPluginSetting(const QString& pluginName, const QString& key,
                                const QVariant& value) override
  {
    return false;
  }
  virtual std::map<QString, QVariant>
  clearPluginSettings(const QString& pluginName) override
  {
    return {};
  }

private:
  ModInfoOverwrite(OrganizerCore& core);
};

#endif  // MODINFOOVERWRITE_H
