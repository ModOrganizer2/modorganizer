#ifndef MODINFOREGULAR_H
#define MODINFOREGULAR_H

#include <limits>

#include "modinfowithconflictinfo.h"
#include "nexusinterface.h"

/**
 * @brief Represents meta information about a single mod.
 *
 * Represents meta information about a single mod. The class interface is used
 * to manage the mod collection
 *
 **/
class ModInfoRegular : public ModInfoWithConflictInfo
{

  Q_OBJECT

  friend class ModInfo;

public:
  ~ModInfoRegular();

  virtual bool isRegular() const override { return true; }

  virtual bool isEmpty() const override;

  bool isAlternate() { return m_IsAlternate; }
  bool isConverted() { return m_Converted; }
  bool isValidated() { return m_Validated; }

  /**
   * @brief test if there is a newer version of the mod
   *
   * test if there is a newer version of the mod. This does NOT cause
   * information to be retrieved from the nexus, it will only test version information
   *already available locally. Use checkAllForUpdate() to update this version
   *information
   *
   * @return true if there is a newer version
   **/
  bool updateAvailable() const override;

  /**
   * @return true if the current update is being ignored
   */
  virtual bool updateIgnored() const override
  {
    return m_IgnoredVersion.isValid() && m_IgnoredVersion == m_NewestVersion;
  }

  /**
   * @brief test if there is a newer version of the mod
   *
   * test if there is a newer version of the mod. This does NOT cause
   * information to be retrieved from the nexus, it will only test version information
   *already available locally. Use checkAllForUpdate() to update this version
   *information
   *
   * @return true if there is a newer version
   **/
  bool downgradeAvailable() const override;

  /**
   * @brief request an update of nexus description for this mod.
   *
   * This requests mod information from the nexus. This is an asynchronous request,
   * so there is no immediate effect of this call.
   *
   * @return returns true if information for this mod will be updated, false if there is
   *no nexus mod id to use
   **/
  bool updateNXMInfo() override;

  /**
   * @brief assign or unassign the specified category
   *
   * Every mod can have an arbitrary number of categories assigned to it
   *
   * @param categoryID id of the category to set
   * @param active determines wheter the category is assigned or unassigned
   * @note this function does not test whether categoryID actually identifies a valid
   *category
   **/
  void setCategory(int categoryID, bool active) override;

  /**
   * @brief set the name of this mod
   *
   * set the name of this mod. This will also update the name of the
   * directory that contains this mod
   *
   * @param name new name of the mod
   * @return true on success, false if the new name can't be used (i.e. because the new
   *         directory name wouldn't be valid)
   **/
  bool setName(const QString& name) override;

  /**
   * @brief changes the comments (manually set information displayed in the mod list)
   * for this mod
   * @param comments new comments
   */
  void setComments(const QString& comments) override;

  /**
   * @brief change the notes (manually set information) for this mod
   * @param notes new notes
   */
  void setNotes(const QString& notes) override;

  /**
   * @brief set/change the source game of this mod
   *
   * @param gameName the source game shortName
   */
  virtual void setGameName(const QString& gameName) override;

  /**
   * @brief set/change the nexus mod id of this mod
   *
   * @param modID the nexus mod id
   **/
  void setNexusID(int modID) override;

  /**
   * @brief set the version of this mod
   *
   * this can be used to overwrite the version of a mod without actually
   * updating the mod
   *
   * @param version the new version to use
   **/
  void setVersion(const MOBase::VersionInfo& version) override;

  /**
   * @brief set the newest version of this mod on the nexus
   *
   * this can be used to overwrite the version of a mod without actually
   * updating the mod
   *
   * @param version the new version to use
   * @todo this function should be made obsolete. All queries for mod information should
   *go through this class so no public function for this change is required
   **/
  void setNewestVersion(const MOBase::VersionInfo& version) override;

  /**
   * @brief changes/updates the nexus description text
   * @param description the current description text
   */
  virtual void setNexusDescription(const QString& description) override;

  virtual void setInstallationFile(const QString& fileName) override;

  /**
   * @brief sets the category id from a nexus category id. Conversion to MO id happens
   * internally
   * @param categoryID the nexus category id
   * @note if a mapping is not possible, the category is set to the default value
   */
  virtual void addNexusCategory(int categoryID) override;

  /**
   * @brief sets the new primary category of the mod
   * @param categoryID the category to set
   */
  virtual void setPrimaryCategory(int categoryID) override
  {
    m_PrimaryCategory = categoryID;
    m_MetaInfoChanged = true;
  }

  /**
   * @brief sets the download repository
   * @param repository
   */
  virtual void setRepository(const QString& repository) override
  {
    m_Repository = repository;
  }

  /**
   * update the endorsement state for the mod. This only changes the
   * buffered state, it does not sync with Nexus
   * @param endorsed the new endorsement state
   */
  virtual void setIsEndorsed(bool endorsed) override;

  /**
   * set the mod to "i don't intend to endorse". The mod will not show as unendorsed but
   * can still be endorsed
   */
  virtual void setNeverEndorse() override;

  /**
   * update the tracked state for the mod.  This only changes the
   * buffered state.  It does not sync with Nexus
   * @param tracked the new tracked state
   */
  virtual void setIsTracked(bool tracked) override;

  /**
   * @brief endorse or un-endorse the mod
   * @param doEndorse if true, the mod is endorsed, if false, it's un-endorsed.
   * @note if doEndorse doesn't differ from the current value, nothing happens.
   */
  virtual void endorse(bool doEndorse) override;

  /**
   * @brief track or untrack the mod.  This will sync with nexus!
   * @param doTrack if true, the mod is tracked, if false, it's untracked.
   * @note if doTrack doesn't differ from the current value, nothing happens.
   */
  virtual void track(bool doTrack) override;

  /**
   * @brief updates the mod to flag it as converted in order to ignore the alternate
   * game warning
   */
  virtual void markConverted(bool converted) override;

  /**
   * @brief updates the mod to flag it as valid in order to ignore the invalid game data
   * flag
   */
  virtual void markValidated(bool validated) override;

  /**
   * @brief getter for the mod name
   *
   * @return the mod name
   **/
  QString name() const override { return m_Name; }

  /**
   * @brief getter for the mod path
   *
   * @return the (absolute) path to the mod
   **/
  QString absolutePath() const override;

  /**
   * @brief getter for the newest version number of this mod
   *
   * @return newest version of the mod
   **/
  MOBase::VersionInfo newestVersion() const override { return m_NewestVersion; }

  /**
   * @brief getter for the newest version number of this mod
   *
   * @return newest version of the mod
   **/
  MOBase::VersionInfo ignoredVersion() const override { return m_IgnoredVersion; }

  /**
   * @brief ignore the newest version for updates
   */
  void ignoreUpdate(bool ignore) override;

  /**
   * @brief getter for the nexus mod id
   *
   * @return the nexus mod id. may be 0 if the mod id isn't known or doesn't exist
   **/
  int nexusId() const override { return m_NexusID; }

  /**
   * @return true if the mod can be updated
   */
  virtual bool canBeUpdated() const override;

  /**
   * @return the update expiration date based on the last updated date from Nexus
   */
  virtual QDateTime getExpires() const override;

  /**
   * @return true if the mod can be enabled/disabled
   */
  virtual bool canBeEnabled() const override { return true; }

  /**
   * @return a list of flags for this mod
   */
  virtual std::vector<EFlag> getFlags() const override;

  /**
   * @return an indicator if and how this mod should be highlighted by the UI
   */
  virtual int getHighlight() const override;

  /**
   * @return list of names of ini tweaks
   **/
  std::vector<QString> getIniTweaks() const override;

  /**
   * @return a description about the mod, to be displayed in the ui
   */
  virtual QString getDescription() const override;

  /**
   * @return the nexus file status (aka category ID)
   */
  virtual int getNexusFileStatus() const override;

  /**
   * @brief sets the file status (category ID) from Nexus
   * @param status the status id of the installed file
   */
  virtual void setNexusFileStatus(int status) override;

  /**
   * @return comments for this mod
   */
  virtual QString comments() const override;

  /**
   * @return manually set notes for this mod
   */
  virtual QString notes() const override;

  /**
   * @return time this mod was created (file time of the directory)
   */
  virtual QDateTime creationTime() const override;

  /**
   * @return nexus description of the mod (html)
   */
  QString getNexusDescription() const override;

  /**
   * @return repository from which the file was downloaded
   */
  virtual QString repository() const override;

  /**
   * @return true if the file has been endorsed on nexus
   */
  virtual MOBase::EndorsedState endorsedState() const override;

  /**
   * @return true if the file is being tracked on nexus
   */
  virtual MOBase::TrackedState trackedState() const override;

  /**
   * @brief get the last time nexus was checked for file updates on this mod
   */
  virtual QDateTime getLastNexusUpdate() const override;

  /**
   * @brief set the last time nexus was checked for file updates on this mod
   */
  virtual void setLastNexusUpdate(QDateTime time) override;

  /**
   * @return last time nexus was queried for infos on this mod
   */
  virtual QDateTime getLastNexusQuery() const override;

  /**
   * @brief set the last time nexus was queried for info on this mod
   */
  virtual void setLastNexusQuery(QDateTime time) override;

  /**
   * @return last time the mod was updated on Nexus
   */
  virtual QDateTime getNexusLastModified() const override;

  /**
   * @brief set the last time the mod was updated on Nexus
   */
  virtual void setNexusLastModified(QDateTime time) override;

  virtual QStringList archives(bool checkOnDisk = false) override;

  virtual void setColor(QColor color) override;

  virtual QColor color() const override;

  virtual void addInstalledFile(int modId, int fileId) override;

  /**
   * @brief stores meta information back to disk
   */
  virtual void saveMeta() override;

  void readMeta() override;

  virtual void setHasCustomURL(bool b) override;
  virtual bool hasCustomURL() const override;
  virtual void setCustomURL(QString const&) override;
  virtual QString url() const override;

  virtual QString gameName() const override { return m_GameName; }
  virtual QString installationFile() const override { return m_InstallationFile; }
  virtual bool converted() const override { return m_Converted; }
  virtual bool validated() const override { return m_Validated; }
  virtual std::set<std::pair<int, int>> installedFiles() const override
  {
    return m_InstalledFileIDs;
  }

public:  // Plugin operations:
  virtual QVariant pluginSetting(const QString& pluginName, const QString& key,
                                 const QVariant& defaultValue) const override;
  virtual std::map<QString, QVariant>
  pluginSettings(const QString& pluginName) const override;
  virtual bool setPluginSetting(const QString& pluginName, const QString& key,
                                const QVariant& value) override;
  virtual std::map<QString, QVariant>
  clearPluginSettings(const QString& pluginName) override;

private:
  void setEndorsedState(MOBase::EndorsedState endorsedState);
  void setTrackedState(MOBase::TrackedState trackedState);

private slots:

  void nxmDescriptionAvailable(QString, int modID, QVariant userData,
                               QVariant resultData);
  void nxmEndorsementToggled(QString, int, QVariant userData, QVariant resultData);
  void nxmTrackingToggled(QString, int, QVariant userData, bool tracked);
  void nxmRequestFailed(QString, int modID, int fileID, QVariant userData,
                        int errorCode, const QString& errorMessage);

protected:
  virtual std::set<int> doGetContents() const override;

  ModInfoRegular(const QDir& path, OrganizerCore& core);

private:
  QString m_Name;
  QString m_Path;
  QString m_InstallationFile;
  QString m_Comments;
  QString m_Notes;
  QString m_NexusDescription;
  QString m_Repository;
  QString m_CustomURL;
  bool m_HasCustomURL;

  // Game name for the mod, can be different from the actual game running in MO2
  // e.g., for Skyrim / Skyrim SE.
  QString m_GameName;

  mutable QStringList m_Archives;

  QDateTime m_CreationTime;
  QDateTime m_LastNexusQuery;
  QDateTime m_LastNexusUpdate;
  QDateTime m_NexusLastModified;

  QColor m_Color;

  int m_NexusID;
  std::set<std::pair<int, int>> m_InstalledFileIDs;

  // List of plugin settings:
  std::map<QString, std::map<QString, QVariant>> m_PluginSettings;

  bool m_MetaInfoChanged;
  bool m_IsAlternate;
  bool m_Converted;
  bool m_Validated;
  int m_NexusFileStatus;
  MOBase::VersionInfo m_NewestVersion;
  MOBase::VersionInfo m_IgnoredVersion;

  MOBase::EndorsedState m_EndorsedState;
  MOBase::TrackedState m_TrackedState;

  NexusBridge m_NexusBridge;

  bool needsDescriptionUpdate() const;
};

#endif  // MODINFOREGULAR_H
