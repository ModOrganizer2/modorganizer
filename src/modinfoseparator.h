#ifndef MODINFOSEPARATOR_H
#define MODINFOSEPARATOR_H

#include "modinforegular.h"

class ModInfoSeparator:
    public ModInfoRegular
{
  friend class ModInfo;

public:

  virtual bool updateAvailable() const { return false; }
  virtual bool updateIgnored() const { return false; }
  virtual bool downgradeAvailable() const { return false; }
  virtual bool updateNXMInfo() { return false; }
  virtual bool isValid() const { return true; }
  //TODO: Fix renaming method to avoid priority reset
  virtual bool setName(const QString& name);

  virtual int getNexusID() const { return -1; }

  virtual void setGameName(QString /*gameName*/)
  {
  }

  virtual void setNexusID(int /*modID*/)
  {
  }

  virtual void endorse(bool /*doEndorse*/)
  {
  }

  virtual void ignoreUpdate(bool /*ignore*/)
  {
  }

  virtual bool canBeUpdated() const { return false; }
  virtual bool canBeEnabled() const { return false; }
  virtual std::vector<QString> getIniTweaks() const { return std::vector<QString>(); }

  virtual std::vector<EFlag> getFlags() const;
  virtual int getHighlight() const;

  virtual QString getDescription() const;
  virtual QString name() const;
  virtual QString getGameName() const { return ""; }
  virtual QString getInstallationFile() const { return ""; }
  virtual QString getURL() const { return ""; }
  virtual QString repository() const { return ""; }

  virtual QDateTime getLastNexusQuery() const { return QDateTime(); }
  virtual QDateTime creationTime() const { return QDateTime(); }

  virtual void getNexusFiles
    (
      QList<MOBase::ModRepositoryFileInfo*>::const_iterator& /*unused*/,
      QList<MOBase::ModRepositoryFileInfo*>::const_iterator& /*unused*/)
  {
  }

  virtual QString getNexusDescription() const { return QString(); }

  virtual void addInstalledFile(int /*modId*/, int /*fileId*/)
  {
  }

private:

  ModInfoSeparator
    (
      PluginContainer* pluginContainer, const MOBase::IPluginGame* game, const QDir& path,
      MOShared::DirectoryEntry** directoryStructure);
};

#endif
