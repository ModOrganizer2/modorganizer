#ifndef MODINFOBACKUP_H
#define MODINFOBACKUP_H

#include "modinforegular.h"

class ModInfoBackup : public ModInfoRegular
{

  Q_OBJECT

  friend class ModInfo;

public:

  virtual bool updateAvailable() const { return false; }
  virtual bool updateIgnored() const { return false; }
  virtual bool downgradeAvailable() const { return false; }
  virtual bool updateNXMInfo() { return false; }
  virtual void setGameName(const QString&) {}
  virtual void setNexusID(int) {}
  virtual void endorse(bool) {}
  virtual void parseNexusInfo() {}
  virtual int getFixedPriority() const { return -1; }
  virtual void ignoreUpdate(bool) {}
  virtual bool canBeUpdated() const { return false; }
  virtual QDateTime getExpires() const { return QDateTime(); }
  virtual bool canBeEnabled() const { return false; }
  virtual std::vector<QString> getIniTweaks() const { return std::vector<QString>(); }
  virtual std::vector<EFlag> getFlags() const;
  virtual QString getDescription() const;
  virtual int getNexusFileStatus() const { return 0; }
  virtual void setNexusFileStatus(int) {}
  virtual QDateTime getLastNexusQuery() const { return QDateTime(); }
  virtual void setLastNexusQuery(QDateTime) {}
  virtual QDateTime getLastNexusUpdate() const { return QDateTime(); }
  virtual void setLastNexusUpdate(QDateTime) {}
  virtual QDateTime getNexusLastModified() const { return QDateTime(); }
  virtual void setNexusLastModified(QDateTime) {}
  virtual void getNexusFiles(QList<MOBase::ModRepositoryFileInfo*>::const_iterator&,
                             QList<MOBase::ModRepositoryFileInfo*>::const_iterator&) {}
  virtual QString getNexusDescription() const { return QString(); }

  virtual void addInstalledFile(int, int) {}

private:

  ModInfoBackup(PluginContainer *pluginContainer, const MOBase::IPluginGame *game, const QDir &path, MOShared::DirectoryEntry **directoryStructure);

};


#endif // MODINFOBACKUP_H
