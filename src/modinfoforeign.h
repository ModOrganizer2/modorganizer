#ifndef MODINFOFOREIGN_H
#define MODINFOFOREIGN_H

#include <limits>

#include "modinfowithconflictinfo.h"

class ModInfoForeign: public ModInfoWithConflictInfo
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
  virtual bool remove() override { return false; }
  virtual void endorse(bool) override {}
  virtual void track(bool) override {}
  virtual bool isEmpty() const override { return false; }
  virtual QString name() const override { return m_Name; }
  virtual QString internalName() const override { return m_InternalName; }
  virtual QString comments() const override { return ""; }
  virtual QString notes() const override { return ""; }
  virtual QDateTime creationTime() const override;
  virtual QString absolutePath() const override;
  virtual MOBase::VersionInfo getNewestVersion() const override { return QString(); }
  virtual QString getInstallationFile() const override { return ""; }
  virtual QString getGameName() const override { return ""; }
  virtual int getNexusID() const override { return -1; }
  virtual QDateTime getExpires() const override { return QDateTime(); }
  virtual std::vector<QString> getIniTweaks() const override { return std::vector<QString>(); }
  virtual std::vector<ModInfo::EFlag> getFlags() const override;
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
  virtual int getFixedPriority() const override { return std::numeric_limits<int>::min(); }
  virtual QStringList archives(bool = false) override { return m_Archives; }
  virtual QStringList stealFiles() const override { return m_Archives + QStringList(m_ReferenceFile); }
  virtual bool alwaysEnabled() const override { return true; }
  virtual void addInstalledFile(int, int) override {}

  ModInfo::EModType modType() const { return m_ModType; }

protected:
  ModInfoForeign(const QString &modName, const QString &referenceFile,
                 const QStringList &archives, ModInfo::EModType modType,
                 MOShared::DirectoryEntry **directoryStructure, PluginContainer *pluginContainer);  
  
private:

  QString m_Name;
  QString m_InternalName;
  QString m_ReferenceFile;
  QStringList m_Archives;
  QDateTime m_CreationTime;
  int m_Priority;
  ModInfo::EModType m_ModType;
};

#endif // MODINFOFOREIGN_H
