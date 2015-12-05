#ifndef MODINFOOVERWRITE_H
#define MODINFOOVERWRITE_H

#include "modinfo.h"

#include <QDateTime>

class ModInfoOverwrite : public ModInfo
{

  Q_OBJECT

  friend class ModInfo;

public:

  virtual bool updateAvailable() const { return false; }
  virtual bool updateIgnored() const { return false; }
  virtual bool downgradeAvailable() const { return false; }
  virtual bool updateNXMInfo() { return false; }
  virtual void setCategory(int, bool) {}
  virtual bool setName(const QString&) { return false; }
  virtual void setNotes(const QString&) {}
  virtual void setNexusID(int) {}
  virtual void setNewestVersion(const MOBase::VersionInfo&) {}
  virtual void ignoreUpdate(bool) {}
  virtual void setNexusDescription(const QString&) {}
  virtual void setInstallationFile(const QString&) {}
  virtual void addNexusCategory(int) {}
  virtual void setIsEndorsed(bool) {}
  virtual void setNeverEndorse() {}
  virtual bool remove() { return false; }
  virtual void endorse(bool) {}
  virtual bool alwaysEnabled() const { return true; }
  virtual bool isEmpty() const;
  virtual QString name() const { return "Overwrite"; }
  virtual QString notes() const { return ""; }
  virtual QDateTime creationTime() const { return QDateTime(); }
  virtual QString absolutePath() const;
  virtual MOBase::VersionInfo getNewestVersion() const { return QString(); }
  virtual QString getInstallationFile() const { return ""; }
  virtual int getFixedPriority() const { return INT_MAX; }
  virtual int getNexusID() const { return -1; }
  virtual std::vector<QString> getIniTweaks() const { return std::vector<QString>(); }
  virtual std::vector<ModInfo::EFlag> getFlags() const;
  virtual int getHighlight() const;
  virtual QString getDescription() const;
  virtual QDateTime getLastNexusQuery() const { return QDateTime(); }
  virtual QString getNexusDescription() const { return QString(); }
  virtual QStringList archives() const;
  virtual void addInstalledFile(int, int) {}

private:

  ModInfoOverwrite();

};

#endif // MODINFOOVERWRITE_H
