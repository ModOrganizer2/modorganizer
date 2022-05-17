#ifndef MODINFOBACKUP_H
#define MODINFOBACKUP_H

#include "modinforegular.h"

class ModInfoBackup : public ModInfoRegular
{

  Q_OBJECT

  friend class ModInfo;

public:
  virtual bool updateAvailable() const override { return false; }
  virtual bool updateIgnored() const override { return false; }
  virtual bool downgradeAvailable() const override { return false; }
  virtual bool updateNXMInfo() override { return false; }
  virtual void setGameName(const QString& gameName) override {}
  virtual void setNexusID(int) override {}
  virtual void endorse(bool) override {}
  virtual void ignoreUpdate(bool) override {}
  virtual bool alwaysDisabled() const override { return true; }
  virtual bool canBeUpdated() const override { return false; }
  virtual QDateTime getExpires() const override { return QDateTime(); }
  virtual bool canBeEnabled() const override { return false; }
  virtual std::vector<QString> getIniTweaks() const override
  {
    return std::vector<QString>();
  }
  virtual std::vector<EFlag> getFlags() const override;
  virtual QString getDescription() const override;
  virtual int getNexusFileStatus() const override { return 0; }
  virtual void setNexusFileStatus(int) override {}
  virtual QDateTime getLastNexusQuery() const override { return QDateTime(); }
  virtual void setLastNexusQuery(QDateTime) override {}
  virtual QDateTime getLastNexusUpdate() const override { return QDateTime(); }
  virtual void setLastNexusUpdate(QDateTime) override {}
  virtual QDateTime getNexusLastModified() const override { return QDateTime(); }
  virtual void setNexusLastModified(QDateTime) override {}
  virtual QString getNexusDescription() const override { return QString(); }
  virtual bool isBackup() const override { return true; }

  virtual void addInstalledFile(int, int) override {}

private:
  ModInfoBackup(const QDir& path, OrganizerCore& core);
};

#endif  // MODINFOBACKUP_H
