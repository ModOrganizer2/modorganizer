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

#include "imodinterface.h"
#include "versioninfo.h"
//#include <directoryentry.h>

class QDateTime;
class QDir;
#include <QMutex>
#include <QSharedPointer>
#include <QString>
#include <QStringList>

#include <boost/function.hpp>

#include <map>
#include <set>
#include <vector>

namespace MOBase { class IPluginGame; }
namespace MOShared { class DirectoryEntry; }

/**
 * @brief Represents meta information about a single mod.
 * 
 * Represents meta information about a single mod. The class interface is used
 * to manage the mod collection
 *
 **/
class ModInfo : public QObject, public MOBase::IModInterface
{

  Q_OBJECT

public:

  typedef QSharedPointer<ModInfo> Ptr;

  static QString s_HiddenExt;

  enum EFlag {
    FLAG_INVALID,
    FLAG_BACKUP,
    FLAG_OVERWRITE,
    FLAG_FOREIGN,
    FLAG_NOTENDORSED,
    FLAG_NOTES,
    FLAG_CONFLICT_OVERWRITE,
    FLAG_CONFLICT_OVERWRITTEN,
    FLAG_CONFLICT_MIXED,
    FLAG_CONFLICT_REDUNDANT
  };

  enum EContent {
    CONTENT_PLUGIN,
    CONTENT_TEXTURE,
    CONTENT_MESH,
    CONTENT_BSA,
    CONTENT_INTERFACE,
    CONTENT_MUSIC,
    CONTENT_SOUND,
    CONTENT_SCRIPT,
    CONTENT_SKSE,
    CONTENT_SKYPROC,
    CONTENT_STRING
  };

  static const int NUM_CONTENT_TYPES = CONTENT_STRING + 1;

  enum EHighlight {
    HIGHLIGHT_NONE = 0,
    HIGHLIGHT_INVALID = 1,
    HIGHLIGHT_CENTER = 2,
    HIGHLIGHT_IMPORTANT = 4
  };

  enum EEndorsedState {
    ENDORSED_FALSE,
    ENDORSED_TRUE,
    ENDORSED_UNKNOWN,
    ENDORSED_NEVER
  };

public:

  /**
   * @brief read the mod directory and Mod ModInfo objects for all subdirectories
   **/
  static void updateFromDisc(const QString &modDirectory,
                             MOShared::DirectoryEntry **directoryStructure,
                             bool displayForeign,
                             MOBase::IPluginGame const *game);

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
   * @return a reference counting pointer to the mod info
   * @todo in its current form, this function is broken! There may be multiple mods with the same nexus id,
   *       this function will return only one of them
   **/
  static std::vector<ModInfo::Ptr> getByModID(int modID);

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
  static ModInfo::Ptr createFrom(const QDir &dir, MOShared::DirectoryEntry **directoryStructure);

  /**
   * @brief create a new "foreign-managed" mod from a tuple of plugin and archives
   * @param espName name of the plugin
   * @param bsaNames names of archives
   * @return a new mod
   */
  static ModInfo::Ptr createFromPlugin(const QString &espName, const QStringList &bsaNames, MOShared::DirectoryEntry **directoryStructure);

  /**
   * @brief retieve a name for one of the CONTENT_ enums
   * @param contentType the content value
   * @return a display string
   */
  static QString getContentTypeName(int contentType);

  virtual bool isRegular() const { return false; }

  virtual bool isEmpty() const { return false; }

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
   * @return true if the update currently available is ignored
   */
  virtual bool updateIgnored() const = 0;

  /**
   * @brief test if the "newest" version of the mod is older than the installed version
   *
   * test if there is a newer version of the mod. This does NOT cause
   * information to be retrieved from the nexus, it will only test version information already
   * available locally. Use checkAllForUpdate() to update this version information
   *
   * @return true if the newest version is older than the installed one
   **/
  virtual bool downgradeAvailable() const = 0;

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
  virtual void setVersion(const MOBase::VersionInfo &version);

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
  virtual void setNewestVersion(const MOBase::VersionInfo &version) = 0;

  /**
   * @brief sets the repository that was used to download the mod
   */
  virtual void setRepository(const QString &) {}

  /**
   * @brief changes/updates the nexus description text
   * @param description the current description text
   */
  virtual void setNexusDescription(const QString &description) = 0;

  /**
   * @brief sets the file this mod was installed from
   * @param fileName name of the file
   */
  virtual void setInstallationFile(const QString &fileName) = 0;

  /**
   * @brief sets the category id from a nexus category id. Conversion to MO id happens internally
   * @param categoryID the nexus category id
   * @note if a mapping is not possible, the category is set to the default value
   */
  virtual void addNexusCategory(int categoryID) = 0;

  virtual void addCategory(const QString &categoryName) override;
  virtual bool removeCategory(const QString &categoryName) override;
  virtual QStringList categories() override;

  /**
   * update the endorsement state for the mod. This only changes the
   * buffered state, it does not sync with Nexus
   * @param endorsed the new endorsement state
   */
  virtual void setIsEndorsed(bool endorsed) = 0;

  /**
   * set the mod to "i don't intend to endorse". The mod will not show as unendorsed but can still be endorsed
   */
  virtual void setNeverEndorse() = 0;

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
   * @brief clear all caches held for this mod
   */
  virtual void clearCaches() {}

  /**
   * @brief getter for the mod name
   *
   * @return the mod name
   **/
  virtual QString name() const = 0;

  /**
   * @brief getter for an internal name. This is usually the same as the regular name, but with special mod types it might be
   *        this is used to distinguish between mods that have the same visible name
   * @return internal mod name
   */
  virtual QString internalName() const { return name(); }

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
  virtual MOBase::VersionInfo getVersion() const { return m_Version; }

  /**
   * @brief getter for the newest version number of this mod
   *
   * @return newest version of the mod
   **/
  virtual MOBase::VersionInfo getNewestVersion() const = 0;

  /**
   * @return the repository from which the file was downloaded. Only relevant regular mods
   */
  virtual QString repository() const { return ""; }

  /**
   * @brief ignore the newest version for updates
   */
  virtual void ignoreUpdate(bool ignore) = 0;

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
   * @return true if the mod is always enabled
   */
  virtual bool alwaysEnabled() const { return false; }

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
   * @return a list of content types contained in a mod
   */
  virtual std::vector<EContent> getContents() const { return std::vector<EContent>(); }

  /**
   * @brief test if the specified flag is set for this mod
   * @param flag the flag to test
   * @return true if the flag is set, false otherwise
   */
  bool hasFlag(EFlag flag) const;

  /**
   * @brief test if the mods contains the specified content
   * @param content the content to test
   * @return true if the content is there, false otherwise
   */
  bool hasContent(ModInfo::EContent content) const;

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
   * @return notes for this mod
   */
  virtual QString notes() const = 0;

  /**
   * @return creation time of this mod
   */
  virtual QDateTime creationTime() const = 0;

  /**
   * @return nexus description of the mod (html)
   */
  virtual QString getNexusDescription() const = 0;

  /**
   * @return last time nexus was queried for infos on this mod
   */
  virtual QDateTime getLastNexusQuery() const = 0;

  /**
   * @return a list of files that, if they exist in the data directory are treated as files in THIS mod
   */
  virtual QStringList stealFiles() const { return QStringList(); }

  /**
   * @return a list of archives belonging to this mod (as absolute file paths)
   */
  virtual QStringList archives() const = 0;

  /**
   * @brief adds the information that a file has been installed into this mod
   * @param modId id of the mod installed
   * @param fileId id of the file installed
   */
  virtual void addInstalledFile(int modId, int fileId) = 0;

  /**
   * @brief test if the mod belongs to the specified category
   *
   * @param categoryID the category to test for.
   * @return true if the mod belongs to the specified category
   * @note this does not verify the id actually identifies a category
   **/
  bool categorySet(int categoryID) const;

  /**
   * @brief retrive the whole list of categories (as ids) this mod belongs to
   *
   * @return list of categories
   **/
  const std::set<int> &getCategories() const { return m_Categories; }

  /**
   * @return id of the primary category of this mod
   */
  int getPrimaryCategory() const { return m_PrimaryCategory; }

  /**
   * @brief sets the new primary category of the mod
   * @param categoryID the category to set
   */
  virtual void setPrimaryCategory(int categoryID) { m_PrimaryCategory = categoryID; }

  /**
   * @return true if this mod is considered "valid", that is: it contains data used by the game
   **/
  bool isValid() const { return m_Valid; }

  /**
   * @return true if the file has been endorsed on nexus
   */
  virtual EEndorsedState endorsedState() const { return ENDORSED_NEVER; }

  /**
   * @brief updates the valid-flag for this mod
   */
  void testValid();

  /**
   * @brief reads meta information from disk
   */
  virtual void readMeta() {}

  /**
   * @brief stores meta information back to disk
   */
  virtual void saveMeta() {}

  /**
   * @return retrieve list of mods (as mod index) that are overwritten by this one. Updates may be delayed
   */
  virtual std::set<unsigned int> getModOverwrite() { return std::set<unsigned int>(); }

  /**
   * @return list of mods (as mod index) that overwrite this one. Updates may be delayed
   */
  virtual std::set<unsigned int> getModOverwritten() { return std::set<unsigned int>(); }

  /**
   * @brief update conflict information
   */
  virtual void doConflictCheck() const {}

  /**
   * @brief set the URL for a mod
   */
  virtual void setURL(QString const &) {}

  /**
   * @returns the URL for a mod
   */
  virtual QString getURL() const { return ""; }

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

  MOBase::VersionInfo m_Version;

private:

  static QMutex s_Mutex;
  static std::map<int, std::vector<unsigned int> > s_ModsByModID;
  static int s_NextID;

  bool m_Valid;

};


#endif // MODINFO_H
