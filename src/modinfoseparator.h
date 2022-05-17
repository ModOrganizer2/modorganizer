#ifndef MODINFOSEPARATOR_H
#define MODINFOSEPARATOR_H

#include "modinforegular.h"

class ModInfoSeparator : public ModInfoRegular
{
  Q_OBJECT;

  friend class ModInfo;

public:
  virtual bool updateAvailable() const override { return false; }
  virtual bool updateIgnored() const override { return false; }
  virtual bool downgradeAvailable() const override { return false; }
  virtual bool updateNXMInfo() override { return false; }
  virtual bool isValid() const override { return true; }
  // TODO: Fix renaming method to avoid priority reset
  virtual bool setName(const QString& name);

  virtual int nexusId() const override { return -1; }
  virtual void setGameName(const QString& gameName) override {}
  virtual void setNexusID(int /*modID*/) override {}
  virtual void endorse(bool /*doEndorse*/) override {}
  virtual void ignoreUpdate(bool /*ignore*/) override {}
  virtual bool canBeUpdated() const override { return false; }
  virtual QDateTime getExpires() const override { return QDateTime(); }
  virtual bool canBeEnabled() const override { return false; }
  virtual std::vector<QString> getIniTweaks() const override
  {
    return std::vector<QString>();
  }
  virtual std::vector<EFlag> getFlags() const override;
  virtual int getHighlight() const override;
  virtual QString getDescription() const override;
  virtual QString name() const override;
  virtual QString gameName() const override { return ""; }
  virtual QString installationFile() const override { return ""; }
  virtual QString repository() const override { return ""; }
  virtual int getNexusFileStatus() const override { return 0; }
  virtual void setNexusFileStatus(int) override {}
  virtual QDateTime getLastNexusUpdate() const override { return QDateTime(); }
  virtual void setLastNexusUpdate(QDateTime) override {}
  virtual QDateTime getLastNexusQuery() const override { return QDateTime(); }
  virtual void setLastNexusQuery(QDateTime) override {}
  virtual QDateTime getNexusLastModified() const override { return QDateTime(); }
  virtual void setNexusLastModified(QDateTime) override {}
  virtual QDateTime creationTime() const override { return QDateTime(); }
  virtual QString getNexusDescription() const override { return QString(); }
  virtual void addInstalledFile(int /*modId*/, int /*fileId*/) override {}
  virtual bool isSeparator() const override { return true; }

protected:
  virtual bool doIsValid() const override { return true; }

private:
  ModInfoSeparator(const QDir& path, OrganizerCore& core);
};

#endif
