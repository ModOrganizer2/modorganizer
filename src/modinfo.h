/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MODINFO_H
#define MODINFO_H

#include "nexusinterface.h"
#include <directoryentry.h>
#include <versioninfo.h>
#include <imodinterface.h>

#include <QString>
#include <QMutex>
#include <QIcon>
#include <QDir>
#include <QSharedPointer>
#include <QDateTime>
#include <map>
#include <set>
#include <vector>


/**
 * @brief Represents meta information about a single mod.
 * 
 * Represents meta information about a single mod. The class interface is used
 * to manage the mod collection
 *
 **/
class ModInfo : public QObject, public IModInterface
{

  Q_OBJECT

public:

  typedef QSharedPointer<ModInfo> Ptr;

  static QString s_HiddenExt;

  enum EFlag {
    FLAG_INVALID,
    FLAG_BACKUP,
    FLAG_OVERWRITE,
    FLAG_NOTENDORSED,
    FLAG_NOTES,
    FLAG_CONFLICT_OVERWRITE,
    FLAG_CONFLICT_OVERWRITTEN,
    FLAG_CONFLICT_MIXED,
    FLAG_CONFLICT_REDUNDANT
  };

  enum EHighlight {
    HIGHLIGHT_NONE = 0,
    HIGHLIGHT_INVALID = 1,
    HIGHLIGHT_CENTER = 2,
    HIGHLIGHT_IMPORTANT = 4
  };

  enum EEndorsedState {
    ENDORSED_FALSE,
    ENDORSED_TRUE,
    ENDORSED_UNKNOWN
  };

  struct NexusFileInfo {
    NexusFileInfo(const QString &data);
    NexusFileInfo(int id, const QString &name, const QString &url, const QString &version,
                  const QString &description, int category, int size)
      : id(id), name(name), url(url), version(version), description(description),
        category(category), size(size) {}
    int id;
    QString name;
    QString url;
    QString version;
    QString description;
    int category;
    int size;

    QString toString() const;
  };

public:

  /**
   * @brief read the mod directory and Mod ModInfo objects for all subdirectories
   **/
  static void updateFromDisc(const QString &modDirectory, DirectoryEntry **directoryStructure);

  static void clear() { s_Collection.clear(); s_ModsByName.clear(); s_ModsByModID.clear(); }

  /**
   * @brief retrieve the number of mods
   *
   * @return number of mods
   **/
  static unsigned int getNumMods();

  /**
   * @brief retrieve a ModInfo object based on its index
   *
   * @param index the index to look up. the maximum is getNumMods() - 1
   * @return a reference counting pointer to the mod info.
   * @note since the pointer is reference counting, the pointer remains valid even if the collection is refreshed in a different thread
   **/
  static ModInfo::Ptr getByIndex(unsigned int index);

  /**
   * @brief retrieve a ModInfo object based on its nexus mod id
   *
   * @param modID the nexus mod id to look up
   * @param missingAcceptable if true, this function will return a null-pointer if no mod has the specified mod id, otherwise an exception is thrown
   * @return a reference counting pointer to the mod info
   * @todo in its current form, this function is broken! There may be multiple mods with the same nexus id,
   *       this function will return only one of them
   **/
  static ModInfo::Ptr getByModID(int modID, bool missingAcceptable);

  /**
   * @brief remove a mod by index
   *
   * this physically deletes the specified mod from the disc and updates the ModInfo collection
   * but not other structures that reference mods
   * @param index index of the mod to delete
   * @return true if removal was successful, fals otherwise
   **/
  static bool removeMod(unsigned int index);

  /**
   * @brief retrieve the mod index by the mod name
   *
   * @param name name of the mod to look up
   * @return the index of the mod. If the mod doesn't exist, UINT_MAX is returned
   **/
  static unsigned int getIndex(const QString &name);

  /**
   * @brief find the first mod that fulfills the filter function (after no particular order)
   * @param filter a function to filter by. should return true for a match
   * @return index of the matching mod or UINT_MAX if there wasn't a match
   */
  static unsigned int findMod(const boost::function<bool (ModInfo::Ptr)> &filter);

  /**
   * @brief check a bunch of mods for updates
   * @param modIDs list of mods (Nexus Mod IDs) to check for updates
   * @return
   */
  static void checkChunkForUpdate(const std::vector<int> &modIDs, QObject *receiver);

  /**
   * @brief query nexus information for every mod and update the "newest version" information
   **/
  static int checkAllForUpdate(QObject *receiver);

  /**
   * @brief create a new mod from the specified directory and add it to the collection
   * @param dir directory to create from
   * @return pointer to the info-structure of the newly created/added mod
   */
  static ModInfo::Ptr createFrom(const QDir &dir, DirectoryEntry **directoryStructure);

  virtual bool isRegular() const { return false; }

  /**
   * @brief test if there is a newer version of the mod
   *
   * test if there is a newer version of the mod. This does NOT cause
   * information to be retrieved from the nexus, it will only test version information already
   * available locally. Use checkAllForUpdate() to update this version information
   *
   * @return true if there is a newer version
   **/
  virtual bool updateAvailable() const = 0;

  /**
   * @brief request an update of nexus description for this mod.
   *
   * This requests mod information from the nexus. This is an asynchronous request,
   * so there is no immediate effect of this call.
   * Right now, Mod Organizer interprets the "newest version" and "description" from the
   * response, though the description is only stored in memory
   *
   **/
  virtual bool updateNXMInfo() = 0;

  /**
   * @brief assign or unassign the specified category
   *
   * Every mod can have an arbitrary number of categories assigned to it
   *
   * @param categoryID id of the category to set
   * @param active determines wheter the category is assigned or unassigned
   * @note this function does not test whether categoryID actually identifies a valid category
   **/
  virtual void setCategory(int categoryID, bool active) = 0;

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
  virtual bool setName(const QString &name) = 0;

  /**
   * @brief change the notes (manually set information) for this mod
   * @param notes new notes
   */
  virtual void setNotes(const QString &notes) = 0;

  /**
   * @brief set/change the nexus mod id of this mod
   *
   * @param modID the nexus mod id
   **/
  virtual void setNexusID(int modID) = 0;

  /**
   * @brief set/change the version of this mod
   * @param version new version of the mod
   */
  virtual void setVersion(const VersionInfo &version);

  /**
   * @brief set the newest version of this mod on the nexus
   *
   * this can be used to overwrite the version of a mod without actually
   * updating the mod
   *
   * @param version the new version to use
   * @todo this function should be made obsolete. All queries for mod information should go through
   *       this class so no public function for this change is required
   **/
  virtual void setNewestVersion(const VersionInfo &version) = 0;

  /**
   * @brief changes/updates the nexus description text
   * @param description the current description text
   */
  virtual void setNexusDescription(const QString &description) = 0;

  /**
   * update the endorsement state for the mod. This only changes the
   * buffered state, it does not sync with Nexus
   * @param endorsed the new endorsement state
   */
  virtual void setIsEndorsed(bool endorsed) = 0;

  /**
   * @brief delete the mod from the disc. This does not update the global ModInfo structure or indices
   * @return true if the mod was successfully removed
   **/
  virtual bool remove() = 0;

  /**
   * @brief endorse or un-endorse the mod. This will sync with nexus!
   * @param doEndorse if true, the mod is endorsed, if false, it's un-endorsed.
   * @note if doEndorse doesn't differ from the current value, nothing happens.
   */
  virtual void endorse(bool doEndorse) = 0;

  /**
   * @brief getter for the mod name
   *
   * @return the mod name
   **/
  virtual QString name() const = 0;

  /**
   * @brief getter for the mod path
   *
   * @return the (absolute) path to the mod
   **/
  virtual QString absolutePath() const = 0;

  /**
   * @brief getter for the installation file
   *
   * @return file used to install this mod from
   */
  virtual QString getInstallationFile() const = 0;

  /**
   * @return version object for machine based comparisons
   **/
  virtual VersionInfo getVersion() const { return m_Version; }

  /**
   * @brief getter for the newest version number of this mod
   *
   * @return newest version of the mod
   **/
  virtual VersionInfo getNewestVersion() const = 0;

  /**
   * @brief getter for the nexus mod id
   *
   * @return the nexus mod id. may be 0 if the mod id isn't known or doesn't exist
   **/
  virtual int getNexusID() const = 0;

  /**
   * @return the fixed priority of mods of this type or INT_MIN if the priority of mods
   *         needs to be user-modifiable. Can be < 0 to force a priority below user-modifable mods
   *         or INT_MAX to force priority above all user-modifiables
   */
  virtual int getFixedPriority() const = 0;

  /**
   * @return true if the mod can be updated
   */
  virtual bool canBeUpdated() const { return false; }

  /**
   * @return true if the mod can be enabled/disabled
   */
  virtual bool canBeEnabled() const { return false; }

  /**
   * @return a list of flags for this mod
   */
  virtual std::vector<EFlag> getFlags() const = 0;

  /**
   * @return an indicator if and how this mod should be highlighted by the UI
   */
  virtual int getHighlight() const { return HIGHLIGHT_NONE; }

  /**
   * @return list of names of ini tweaks
   **/
  virtual std::vector<QString> getIniTweaks() const = 0;

  /**
   * @return a description about the mod, to be displayed in the ui
   */
  virtual QString getDescription() const = 0;

  /**
   * @return manually set notes for this mod
   */
  virtual QString notes() const = 0;

  /**
   * @brief return the list of files on nexus related to this mod in the form of iterators
   * @note this is valid only after querying the files
   * @param begin iterator to the first file
   * @param end iterator to one past the last file
   */
  virtual void getNexusFiles(QList<NexusFileInfo>::const_iterator &begin, QList<NexusFileInfo>::const_iterator &end) = 0;

  /**
   * @return nexus description of the mod (html)
   */
  virtual QString getNexusDescription() const = 0;

  /**
   * @return last time nexus was queried for infos on this mod
   */
  virtual QDateTime getLastNexusQuery() const = 0;

  /**
   * @brief test if the mod belongs to the specified category
   *
   * @param categoryID the category to test for.
   * @return true if the mod belongs to the specified category
   * @note this does not verify the id actually identifies a category
   **/
  bool categorySet(int categoryID) const;

  /**
   * @brief retrive the whole list of categories this mod belongs to
   *
   * @return list of categories
   **/
  const std::set<int> &getCategories() const { return m_Categories; }

  /**
   * @return the primary category of this mod
   */
  int getPrimaryCategory() const { return m_PrimaryCategory; }

  /**
   * @brief sets the new primary category of the mod
   * @param categoryID the category to set
   */
  void setPrimaryCategory(int categoryID) { m_PrimaryCategory = categoryID; }

  /**
   * @return true if this mod is considered "valid", that is: it contains data used by the game
   **/
  bool isValid() const { return m_Valid; }

  /**
   * @return true if the file has been endorsed on nexus
   */
  virtual EEndorsedState endorsedState() const { return ENDORSED_UNKNOWN; }

  /**
   * @brief updates the valid-flag for this mod
   */
  void testValid();

  /**
   * @brief stores meta information back to disk
   */
  virtual void saveMeta() {}

signals:

  /**
   * @brief emitted whenever the information of a mod changes
   *
   * @param success true if the mod details were updated successfully, false if not
   **/
  void modDetailsUpdated(bool success);

protected:

  ModInfo();

  static void updateIndices();

private:

  static void createFromOverwrite();

protected:

  static std::vector<ModInfo::Ptr> s_Collection;
  static std::map<QString, unsigned int> s_ModsByName;

  int m_PrimaryCategory;
  std::set<int> m_Categories;

  VersionInfo m_Version;

private:

  static QMutex s_Mutex;
  static std::map<int, unsigned int> s_ModsByModID;
  static int s_NextID;

  bool m_Valid;

};



/**
 * @brief Represents meta information about a single mod.
 * 
 * Represents meta information about a single mod. The class interface is used
 * to manage the mod collection
 *
 **/
class ModInfoRegular : public ModInfo
{

  Q_OBJECT

  friend class ModInfo;

public:

  ~ModInfoRegular();

  virtual bool isRegular() const { return true; }

  /**
   * @brief test if there is a newer version of the mod
   *
   * test if there is a newer version of the mod. This does NOT cause
   * information to be retrieved from the nexus, it will only test version information already
   * available locally. Use checkAllForUpdate() to update this version information
   *
   * @return true if there is a newer version
   **/
  bool updateAvailable() const;

  /**
   * @brief request an update of nexus description for this mod.
   * 
   * This requests mod information from the nexus. This is an asynchronous request,
   * so there is no immediate effect of this call.
   *
   * @return returns true if information for this mod will be updated, false if there is no nexus mod id to use
   **/
  bool updateNXMInfo();

  /**
   * @brief assign or unassign the specified category
   * 
   * Every mod can have an arbitrary number of categories assigned to it
   *
   * @param categoryID id of the category to set
   * @param active determines wheter the category is assigned or unassigned
   * @note this function does not test whether categoryID actually identifies a valid category
   **/
  void setCategory(int categoryID, bool active);

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
  bool setName(const QString &name);

  /**
   * @brief change the notes (manually set information) for this mod
   * @param notes new notes
   */
  void setNotes(const QString &notes);

  /**
   * @brief set/change the nexus mod id of this mod
   *
   * @param modID the nexus mod id
   **/
  void setNexusID(int modID) { m_NexusID = modID; }

  /**
   * @brief set the version of this mod
   *
   * this can be used to overwrite the version of a mod without actually
   * updating the mod
   *
   * @param version the new version to use
   **/
  void setVersion(const VersionInfo &version);

  /**
   * @brief set the newest version of this mod on the nexus
   *
   * this can be used to overwrite the version of a mod without actually
   * updating the mod
   *
   * @param version the new version to use
   * @todo this function should be made obsolete. All queries for mod information should go through
   *       this class so no public function for this change is required
   **/
  void setNewestVersion(const VersionInfo &version) { m_NewestVersion = version; }

  /**
   * @brief changes/updates the nexus description text
   * @param description the current description text
   */
  virtual void setNexusDescription(const QString &description);

  /**
   * update the endorsement state for the mod. This only changes the
   * buffered state, it does not sync with Nexus
   * @param endorsed the new endorsement state
   */
  virtual void setIsEndorsed(bool endorsed);

  /**
   * @brief delete the mod from the disc. This does not update the global ModInfo structure or indices
   * @return true if the mod was successfully removed
   **/
  bool remove();

  /**
   * @brief endorse or un-endorse the mod
   * @param doEndorse if true, the mod is endorsed, if false, it's un-endorsed.
   * @note if doEndorse doesn't differ from the current value, nothing happens.
   */
  virtual void endorse(bool doEndorse);

  /**
   * @brief getter for the mod name
   *
   * @return the mod name
   **/
  QString name() const { return m_Name; }

  /**
   * @brief getter for the mod path
   *
   * @return the (absolute) path to the mod
   **/
  QString absolutePath() const;

  /**
   * @brief getter for the newest version number of this mod
   *
   * @return newest version of the mod
   **/
  VersionInfo getNewestVersion() const { return m_NewestVersion; }

  /**
   * @brief getter for the installation file
   *
   * @return file used to install this mod from
   */
  virtual QString getInstallationFile() const { return m_InstallationFile; }
  /**
   * @brief getter for the nexus mod id
   *
   * @return the nexus mod id. may be 0 if the mod id isn't known or doesn't exist
   **/
  int getNexusID() const { return m_NexusID; }

  /**
   * @return the fixed priority of mods of this type or INT_MIN if the priority of mods
   *         needs to be user-modifiable
   */
  virtual int getFixedPriority() const { return INT_MIN; }

  /**
   * @return true if the mod can be updated
   */
  virtual bool canBeUpdated() const { return m_NexusID >= 0; }

  /**
   * @return true if the mod can be enabled/disabled
   */
  virtual bool canBeEnabled() const { return true; }

  /**
   * @return a list of flags for this mod
   */
  virtual std::vector<EFlag> getFlags() const;

  /**
   * @return an indicator if and how this mod should be highlighted by the UI
   */
  virtual int getHighlight() const;

  /**
   * @return list of names of ini tweaks
   **/
  std::vector<QString> getIniTweaks() const;

  /**
   * @return a description about the mod, to be displayed in the ui
   */
  virtual QString getDescription() const;

  /**
   * @return manually set notes for this mod
   */
  virtual QString notes() const;

  /**
   * @brief return the list of files on nexus related to this mod in the form of iterators
   * @note this is valid only after querying the files
   * @param begin iterator to the first file
   * @param end iterator to one past the last file
   */
  virtual void getNexusFiles(QList<NexusFileInfo>::const_iterator &begin, QList<NexusFileInfo>::const_iterator &end);

  /**
   * @return nexus description of the mod (html)
   */
  QString getNexusDescription() const;

  /**
   * @return true if the file has been endorsed on nexus
   */
  virtual EEndorsedState endorsedState() const;

  /**
   * @return last time nexus was queried for infos on this mod
   */
  QDateTime getLastNexusQuery() const;

  /**
   * @brief stores meta information back to disk
   */
  virtual void saveMeta();

private:

  enum EConflictType {
    CONFLICT_NONE,
    CONFLICT_OVERWRITE,
    CONFLICT_OVERWRITTEN,
    CONFLICT_MIXED,
    CONFLICT_REDUNDANT
  };

private slots:

  void nxmDescriptionAvailable(int modID, QVariant userData, QVariant resultData);
  void nxmFilesAvailable(int, QVariant, QVariant resultData);
  void nxmEndorsementToggled(int, QVariant userData, QVariant resultData);
  void nxmRequestFailed(int modID, QVariant userData, const QString &errorMessage);

private:

  /**
   * @return true if there is a conflict for files in this mod
   */
  EConflictType isConflicted() const;

  /**
   * @return true if this mod is completely replaced by others
   */
  bool isRedundant() const;

protected:

  ModInfoRegular(const QDir &path, DirectoryEntry **directoryStructure);

private:

  QString m_Name;
  QString m_Path;
  QString m_InstallationFile;
  QString m_Notes;
  QString m_NexusDescription;
  QList<NexusFileInfo> m_NexusFileInfos;

  QDateTime m_LastNexusQuery;

  int m_NexusID;

  bool m_MetaInfoChanged;
  VersionInfo m_NewestVersion;

  EEndorsedState m_EndorsedState;

  NexusBridge m_NexusBridge;

  DirectoryEntry **m_DirectoryStructure;

};


class ModInfoBackup : public ModInfoRegular
{

  friend class ModInfo;

public:

  virtual bool updateAvailable() const { return false; }
  virtual bool updateNXMInfo() { return false; }
  virtual void setNexusID(int) {}
  virtual void endorse(bool) {}
  virtual int getFixedPriority() const { return -1; }
  virtual bool canBeUpdated() const { return false; }
  virtual bool canBeEnabled() const { return false; }
  virtual std::vector<QString> getIniTweaks() const { return std::vector<QString>(); }
  virtual std::vector<EFlag> getFlags() const;
  virtual QString getDescription() const;
  virtual QDateTime getLastNexusQuery() const { return QDateTime(); }
  virtual void getNexusFiles(QList<NexusFileInfo>::const_iterator&, QList<NexusFileInfo>::const_iterator&) {}
  virtual QString getNexusDescription() const { return QString(); }

private:

  ModInfoBackup(const QDir &path, DirectoryEntry **directoryStructure);

};


class ModInfoOverwrite : public ModInfo
{

  Q_OBJECT

  friend class ModInfo;

public:

  virtual bool updateAvailable() const { return false; }
  virtual bool updateNXMInfo() { return false; }
  virtual void setCategory(int, bool) {}
  virtual bool setName(const QString&) { return false; }
  virtual void setNotes(const QString&) {}
  virtual void setNexusID(int) {}
  virtual void setNewestVersion(const VersionInfo&) {}
  virtual void setNexusDescription(const QString&) {}
  virtual void setIsEndorsed(bool) {}
  virtual bool remove() { return false; }
  virtual void endorse(bool) {}
  virtual QString name() const { return tr("Overwrite"); }
  virtual QString notes() const { return ""; }
  virtual QString absolutePath() const;
  virtual VersionInfo getNewestVersion() const { return ""; }
  virtual QString getInstallationFile() const { return ""; }
  virtual int getFixedPriority() const { return INT_MAX; }
  virtual int getNexusID() const { return -1; }
  virtual std::vector<QString> getIniTweaks() const { return std::vector<QString>(); }
  virtual std::vector<ModInfo::EFlag> getFlags() const;
  virtual int getHighlight() const;
  virtual QString getDescription() const;
  virtual QDateTime getLastNexusQuery() const { return QDateTime(); }
  virtual void getNexusFiles(QList<NexusFileInfo>::const_iterator&, QList<NexusFileInfo>::const_iterator&) {}
  virtual QString getNexusDescription() const { return QString(); }

private:

  ModInfoOverwrite();

};

#endif // MODINFO_H
