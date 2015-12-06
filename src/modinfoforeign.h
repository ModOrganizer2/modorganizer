#ifndef MODINFOFOREIGN_H
#define MODINFOFOREIGN_H

#include "modinfowithconflictinfo.h"

class ModInfoForeign : public ModInfoWithConflictInfo
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
  virtual bool isEmpty() const { return false; }
  virtual QString name() const;
  virtual QString internalName() const { return name(); }
  virtual QString notes() const { return ""; }
  virtual QDateTime creationTime() const;
  virtual QString absolutePath() const;
  virtual MOBase::VersionInfo getNewestVersion() const { return QString(); }
  virtual QString getInstallationFile() const { return ""; }
  virtual int getNexusID() const { return -1; }
  virtual std::vector<QString> getIniTweaks() const { return std::vector<QString>(); }
  virtual std::vector<ModInfo::EFlag> getFlags() const;
  virtual int getHighlight() const;
  virtual QString getDescription() const;
  virtual QDateTime getLastNexusQuery() const { return QDateTime(); }
  virtual QString getNexusDescription() const { return QString(); }
  virtual int getFixedPriority() const { return INT_MIN; }
  virtual QStringList archives() const { return m_Archives; }
  virtual QStringList stealFiles() const { return m_Archives + QStringList(m_ReferenceFile); }
  virtual bool alwaysEnabled() const { return true; }
  virtual void addInstalledFile(int, int) {}

protected:

  ModInfoForeign(const QString &referenceFile, const QStringList &archives, MOShared::DirectoryEntry **directoryStructure);

private:

  QString m_Name;
  QString m_ReferenceFile;
  QStringList m_Archives;
  QDateTime m_CreationTime;
  int m_Priority;

};

#endif // MODINFOFOREIGN_H
