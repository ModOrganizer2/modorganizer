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
  virtual void setComments(const QString&) {}
  virtual void setNotes(const QString&) {}
  virtual void setGameName(const QString&) {}
  virtual void setNexusID(int) {}
  virtual void setNewestVersion(const MOBase::VersionInfo&) {}
  virtual void ignoreUpdate(bool) {}
  virtual void setNexusDescription(const QString&) {}
  virtual void setInstallationFile(const QString&) {}
  virtual void addNexusCategory(int) {}
  virtual void setIsEndorsed(bool) {}
  virtual void setNeverEndorse() {}
  virtual void setIsTracked(bool) {}
  virtual bool remove() { return false; }
  virtual void endorse(bool) {}
  virtual void track(bool) {}
  virtual void parseNexusInfo() {}
  virtual bool isEmpty() const { return false; }
  virtual QString name() const;
  virtual QString internalName() const { return name(); }
  virtual QString comments() const { return ""; }
  virtual QString notes() const { return ""; }
  virtual QDateTime creationTime() const;
  virtual QString absolutePath() const;
  virtual MOBase::VersionInfo getNewestVersion() const { return QString(); }
  virtual QString getInstallationFile() const { return ""; }
  virtual QString getGameName() const { return ""; }
  virtual int getNexusID() const { return -1; }
  virtual QDateTime getExpires() const { return QDateTime(); }
  virtual std::vector<QString> getIniTweaks() const { return std::vector<QString>(); }
  virtual std::vector<ModInfo::EFlag> getFlags() const;
  virtual int getHighlight() const;
  virtual QString getDescription() const;
  virtual int getNexusFileStatus() const { return 0; }
  virtual void setNexusFileStatus(int) {}
  virtual QDateTime getLastNexusUpdate() const { return QDateTime(); }
  virtual void setLastNexusUpdate(QDateTime) {}
  virtual QDateTime getLastNexusQuery() const { return QDateTime(); }
  virtual void setLastNexusQuery(QDateTime) {}
  virtual QDateTime getNexusLastModified() const { return QDateTime(); }
  virtual void setNexusLastModified(QDateTime) {}
  virtual QString getNexusDescription() const { return QString(); }
  virtual int getFixedPriority() const { return INT_MIN; }
  virtual QStringList archives(bool checkOnDisk = false)  { return m_Archives; }
  virtual QStringList stealFiles() const { return m_Archives + QStringList(m_ReferenceFile); }
  virtual bool alwaysEnabled() const { return true; }
  virtual void addInstalledFile(int, int) {}

protected:
  ModInfoForeign(const QString &modName, const QString &referenceFile,
                 const QStringList &archives, ModInfo::EModType modType,
                 MOShared::DirectoryEntry **directoryStructure, PluginContainer *pluginContainer);
private:

  QString m_Name;
  QString m_ReferenceFile;
  QStringList m_Archives;
  QDateTime m_CreationTime;
  int m_Priority;

};

#endif // MODINFOFOREIGN_H
