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

#include "ifiletree.h"
#include "imodinterface.h"
#include "versioninfo.h"

class OrganizerCore;
class PluginManager;
class QDir;
class QDateTime;

#include <QColor>
#include <QRecursiveMutex>
#include <QSharedPointer>
#include <QString>
#include <QStringList>

#include <boost/function.hpp>

#include <map>
#include <set>
#include <vector>

namespace MOBase
{
class IPluginGame;
}
namespace MOShared
{
class DirectoryEntry;
}

/**
 * @brief Represents meta information about a single mod.
 *
 * Represents meta information about a single mod. The class interface is used
 * to manage the mod collection
 *
 */
class ModInfo : public QObject, public MOBase::IModInterface
{

  Q_OBJECT

public:  // Type definitions:
  typedef QSharedPointer<ModInfo> Ptr;

  static QString s_HiddenExt;

  enum EConflictFlag
  {
    FLAG_CONFLICT_OVERWRITE,
    FLAG_CONFLICT_OVERWRITTEN,
    FLAG_CONFLICT_MIXED,
    FLAG_CONFLICT_REDUNDANT,
    FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITE,
    FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITTEN,
    FLAG_ARCHIVE_CONFLICT_OVERWRITE,
    FLAG_ARCHIVE_CONFLICT_OVERWRITTEN,
    FLAG_ARCHIVE_CONFLICT_MIXED,
    FLAG_OVERWRITE_CONFLICT,
  };

  enum EFlag
  {
    FLAG_INVALID,
    FLAG_BACKUP,
    FLAG_SEPARATOR,
    FLAG_OVERWRITE,
    FLAG_FOREIGN,
    FLAG_HIDDEN_FILES,
    FLAG_NOTENDORSED,
    FLAG_NOTES,
    FLAG_PLUGIN_SELECTED,
    FLAG_ALTERNATE_GAME,
    FLAG_TRACKED,
  };

  enum EHighlight
  {
    HIGHLIGHT_NONE      = 0,
    HIGHLIGHT_INVALID   = 1,
    HIGHLIGHT_CENTER    = 2,
    HIGHLIGHT_IMPORTANT = 4,
    HIGHLIGHT_PLUGIN    = 8
  };

  enum EModType
  {
    MOD_DEFAULT,
    MOD_DLC,
    MOD_CC
  };

public:  // Static functions:
  /**
   * @brief Read the mod directory and Mod ModInfo objects for all subdirectories.
   */
  static void updateFromDisc(const QString& modDirectory, OrganizerCore& core,
                             bool displayForeign, std::size_t refreshThreadCount);

  static void clear()
  {
    s_Collection.clear();
    s_ModsByName.clear();
    s_ModsByModID.clear();
  }

  /**
   * @brief Retrieve the number of mods.
   *
   * @return the number of mods.
   */
  static unsigned int getNumMods();

  /**
   * @brief Retrieve a ModInfo object based on its index.
   *
   * @param index The index to look up. The maximum value is getNumMods() - 1.
   *
   * @return a reference counting pointer to the mod info.
   *
   * @note since the pointer is reference counting, the pointer remains valid even if
   * the collection is refreshed in a different thread.
   */
  static ModInfo::Ptr getByIndex(unsigned int index);

  /**
   * @brief Retrieve ModInfo objects based on mod id.
   *
   * @param modID The mod id to look up.
   *
   * @return a vector of reference counting pointer to the mod info objects with the
   * given mod ID.
   */
  static std::vector<ModInfo::Ptr> getByModID(QString game, int modID);

  /**
   * @brief Retrieve a ModInfo object based on its name.
   *
   * @param name The name to look up.
   *
   * @return a reference counting pointer to the mod info.
   *
   * @note Since the pointer is reference counter, the pointer remains valid even if the
   *     collection is refreshed in a different thread.
   */
  static ModInfo::Ptr getByName(const QString& name);

  /**
   * @brief Remove a mod by index.
   *
   * This physically deletes the specified mod from the disc and updates the ModInfo
   * collection but not other structures that reference mods.
   *
   * @param index Index of the mod to delete.
   *
   * @return true if removal was successful, false otherwise.
   */
  static bool removeMod(unsigned int index);

  /**
   * @brief Retrieve the mod index by the mod name.
   *
   * @param name Name of the mod to look up.
   *
   * @return The index of the mod. If the mod doesn't exist, UINT_MAX is returned.
   */
  static unsigned int getIndex(const QString& name);

  /**
   * @brief Retrieve the overwrite mod.
   */
  static ModInfo::Ptr getOverwrite() { return s_Overwrite; }

  /**
   * @brief Find the first mod that fulfills the filter function (after no particular
   * order).
   *
   * @param filter A function to filter mods by. Should return true for a match.
   *
   * @return index of the matching mod or UINT_MAX if there was no match.
   */
  static unsigned int findMod(const boost::function<bool(ModInfo::Ptr)>& filter);

  /**
   * @brief Run a limited batch of mod update checks for "newest version" information.
   *
   */
  static void manualUpdateCheck(QObject* receiver, std::multimap<QString, int> IDs);

  /**
   * @brief Query nexus information for every mod and update the "newest version"
   * information.
   *
   * @return true if any mods are checked for update.
   */
  static bool checkAllForUpdate(PluginManager* pluginManager, QObject* receiver);

  /**
   *
   */
  static std::set<ModInfo::Ptr> filteredMods(QString gameName, QVariantList updateData,
                                             bool addOldMods  = false,
                                             bool markUpdated = false);

  /**
   * @brief Check wheter a name corresponds to a separator or not,
   *
   * @return whether the given name is used for separators.
   */
  static bool isSeparatorName(const QString& name);

  /**
   * @brief Check wheter a name corresponds to a backup or not,
   *
   * @return whether the given name is used for backups.
   */
  static bool isBackupName(const QString& name);

  /**
   * @brief Check wheter a name corresponds to a regular mod or not,
   *
   * @return whether the given name corresponds to a regular mod.
   */
  static bool isRegularName(const QString& name);

public:  // IModInterface implementations / Re-declaration
  // Note: This section contains default-implementation for some of the virtual methods
  // from IModInterface, but also redeclaration of all the pure-virtual methods to
  // centralize all of them in a single place.

  /**
   * @return the name of the mod.
   */
  virtual QString name() const = 0;

  /**
   * @return the absolute path to the mod to be used in file system operations.
   */
  virtual QString absolutePath() const = 0;

  /**
   * @return the comments for this mod, if any.
   */
  virtual QString comments() const = 0;

  /**
   * @return the notes for this mod, if any.
   */
  virtual QString notes() const = 0;

  /**
   * @brief Retrieve the short name of the game associated with this mod. This may
   * differ from the current game plugin (e.g. you can install a Skyrim LE game in a SSE
   *     installation).
   *
   * @return the name of the game associated with this mod.
   */
  virtual QString gameName() const = 0;

  /**
   * @return the name of the repository from which this mod was installed.
   */
  virtual QString repository() const override { return ""; }

  /**
   * @return the nexus ID of this mod on the repository.
   */
  virtual int nexusId() const = 0;

  /**
   * @return the current version of this mod.
   */
  virtual MOBase::VersionInfo version() const override { return m_Version; }

  /**
   * @return the newest version of thid mod (as known by MO2). If this matches
   * version(), then the mod is up-to-date.
   */
  virtual MOBase::VersionInfo newestVersion() const = 0;

  /**
   * @return the ignored version of this mod (for update), or an invalid version if the
   * user did not ignore version for this mod.
   */
  virtual MOBase::VersionInfo ignoredVersion() const = 0;

  /**
   * @return the absolute path to the file that was used to install this mod.
   */
  virtual QString installationFile() const = 0;

  virtual std::set<std::pair<int, int>> installedFiles() const = 0;

  /**
   * @return true if this mod was marked as converted by the user.
   *
   * @note When a mod is for a different game, a flag is shown to users to warn them,
   * but they can mark mods as converted to remove this flag.
   */
  virtual bool converted() const = 0;

  /**
   * @return true if th is mod was marked as containing valid game data.
   *
   * @note MO2 uses ModDataChecker to check the content of mods, but sometimes these
   * fail, in which case mods are incorrectly marked as 'not containing valid games
   * data'. Users can choose to mark these mods as valid to hide the warning / flag.
   */
  virtual bool validated() const = 0;

  /**
   * @return the color of the 'Notes' column chosen by the user.
   */
  virtual QColor color() const override { return QColor(); }

  /**
   * @return the URL of this mod, or an empty QString() if no URL is associated
   *     with this mod.
   */
  virtual QString url() const override { return ""; }

  /**
   * @return the ID of the primary category of this mod.
   */
  int primaryCategory() const override { return m_PrimaryCategory; }

  /**
   * @return the list of categories this mod belongs to.
   */
  virtual QStringList categories() const override;

  /**
   * @return the tracked state of this mod.
   */
  virtual MOBase::TrackedState trackedState() const override
  {
    return MOBase::TrackedState::TRACKED_FALSE;
  }

  /**
   * @return the endorsement state of this mod.
   */
  virtual MOBase::EndorsedState endorsedState() const override
  {
    return MOBase::EndorsedState::ENDORSED_NEVER;
  }

  /**
   * @brief Retrieve a file tree corresponding to the underlying disk content
   *     of this mod.
   *
   * The file tree should not be cached since it is already cached and updated when
   * required.
   *
   * @return a file tree representing the content of this mod.
   */
  virtual std::shared_ptr<const MOBase::IFileTree> fileTree() const = 0;

  /**
   * @return true if this object represents a regular mod.
   */
  virtual bool isRegular() const { return false; }

  /**
   * @return true if this object represents the overwrite mod.
   */
  virtual bool isOverwrite() const { return false; }

  /**
   * @return true if this object represents a backup.
   */
  virtual bool isBackup() const { return false; }

  /**
   * @return true if this object represents a separator.
   */
  virtual bool isSeparator() const { return false; }

  /**
   * @return true if this object represents a foreign mod.
   */
  virtual bool isForeign() const { return false; }

public:  // Mutable operations:
  /**
   * @brief Sets or changes the version of this mod.
   *
   * @param version New version of the mod.
   */
  virtual void setVersion(const MOBase::VersionInfo& version) override;

  /**
   * @brief Sets the installation file for this mod.
   *
   * @param fileName archive file name.
   */
  virtual void setInstallationFile(const QString& fileName) = 0;

  /**
   * @brief Sets or changes the latest known version of this mod.
   *
   * @param version Newest known version of the mod.
   */
  virtual void setNewestVersion(const MOBase::VersionInfo& version) = 0;

  /**
   * @brief Sets endorsement state of the mod.
   *
   * @param endorsed New endorsement state.
   */
  virtual void setIsEndorsed(bool endorsed) = 0;

  /**
   * @brief Sets the mod id on nexus for this mod.
   *
   * @param nexusID The new Nexus id to set.
   */
  virtual void setNexusID(int nexusID) = 0;

  /**
   * @brief Sets the category id from a nexus category id. Conversion to MO id happens
   *     internally.
   *
   * @param categoryID The nexus category id.
   *
   * @note If a mapping is not possible, the category is set to the default value.
   */
  virtual void addNexusCategory(int categoryID) = 0;

  /**
   * @brief Assigns a category to the mod. If the named category does not exist it is
   * created.
   *
   * @param categoryName Name of the new category.
   */
  virtual void addCategory(const QString& categoryName) override;

  /**
   * @brief Unassigns a category from this mod.
   *
   * @param categoryName Name of the category to be removed.
   *
   * @return true if the category was removed successfully, false if no such category
   *    was assigned.
   */
  virtual bool removeCategory(const QString& categoryName) override;

  /**
   * @brief Sets or changes the source game of this mod.
   *
   * @param gameName The source game short name.
   */
  virtual void setGameName(const QString& gameName) = 0;

  /**
   * @brief Sets the name of this mod.
   *
   * This will also update the name of the directory that contains this mod.
   *
   * @param name New name of the mod.
   *
   * @return true on success, false if the new name can't be used (i.e. because the new
   *     directory name wouldn't be valid).
   */
  virtual bool setName(const QString& name) = 0;

public:  // Methods after this do not come from IModInterface:
  /**
   * @return true if this mod is empty, false otherwise.
   */
  virtual bool isEmpty() const { return false; }

  /**
   * @brief Check if there is a newer version of the mod.
   *
   * Check if there is a newer version of the mod. This does NOT cause information
   * to be retrieved from the nexus, it will only test version information already
   * available locally. Use checkAllForUpdate() to update this version information.
   *
   * @return true if there is a newer version, false otherwise.
   */
  virtual bool updateAvailable() const = 0;

  /**
   * @return true if the update currently available is ignored.
   */
  virtual bool updateIgnored() const = 0;

  /**
   * @brief Check if the "newest" version of the mod is older than the installed
   * version.
   *
   * Check if there is a newer version of the mod. This does NOT cause information to be
   * retrieved from the nexus, it will only test version information already available
   * locally. Use checkAllForUpdate() to update this version information.
   *
   * @return true if the newest version is older than the installed one.
   */
  virtual bool downgradeAvailable() const = 0;

  /**
   * @brief Request an update of nexus description for this mod.
   *
   * This requests mod information from the nexus. This is an asynchronous request,
   * so there is no immediate effect of this call. Right now, Mod Organizer interprets
   * the "newest version" and "description" from the response, though the description is
   * only stored in memory.
   *
   */
  virtual bool updateNXMInfo() = 0;

  /**
   * @brief Assigns or unassigns the specified category/
   *
   * Every mod can have an arbitrary number of categories assigned to it.
   *
   * @param categoryID ID of the category to set.
   * @param active Determines whether the category is assigned or unassigned.
   *
   * @note This function does not test whether categoryID actually identifies a valid
   * category.
   */
  virtual void setCategory(int categoryID, bool active) = 0;

  /**
   * @brief Changes the comments (manually set information displayed in the mod list)
   * for this mod.
   *
   * @param comments The new comments.
   */
  virtual void setComments(const QString& comments) = 0;

  /**
   * @brief Change the notes (manually set information) for this mod.
   *
   * @param notes The new notes.
   */
  virtual void setNotes(const QString& notes) = 0;

  /**
   * @brief Controls if mod should be highlighted based on plugin selection.
   *
   * @param isSelected Whether or not the plugin has a selected mod.
   */
  virtual void setPluginSelected(const bool& isSelected);

  /**
   * @brief Sets the repository that was used to download the mod.
   */
  virtual void setRepository(const QString&) {}

  /**
   * @brief Set the mod to "I do not intend to endorse.". The mod will not show as
   * unendorsed but can still be endorsed.
   */
  virtual void setNeverEndorse() = 0;

  /**
   * @brief Updates the tracked state for the mod. This only changes the buffered state,
   *     it does not sync with Nexus.
   *
   * @param tracked The new tracked state.
   *
   * @see track(bool)
   */
  virtual void setIsTracked(bool tracked) = 0;

  /**
   * @brief Endorses or un-endorses the mod. This will sync with nexus!
   *
   * @param doEndorse If true, the mod is endorsed, if false, it's un-endorsed.
   *
   * @note If doEndorse does not differ from the current value, nothing happens.
   */
  virtual void endorse(bool doEndorse) = 0;

  /**
   * @brief Tracks or untracks the mod.  This will sync with nexus!
   *
   * @param doTrack If true, the mod is tracked, if false, it's untracked.
   *
   * @note If doTrack does not differ from the current value, nothing happens.
   */
  virtual void track(bool doTrack) = 0;

  /**
   * @brief Clear all caches held for this mod.
   */
  virtual void clearCaches() {}

  /**
   * @brief Retrieve the internal name of the mod. This is usually the same as the
   * regular name, but with special mod types it might be used to distinguish between
   * mods that have the same visible name.
   *
   * @return the internal mod name.
   */
  virtual QString internalName() const { return name(); }

  /**
   * @brief Ignores the newest version for updates.
   */
  virtual void ignoreUpdate(bool ignore) = 0;

  // check if the priority of this mod is not user-modifiable (i.e.
  // computed by MO2 automatically)
  //
  bool hasAutomaticPriority() const { return isBackup() || isOverwrite(); }

  // check if this mod should always be enabled or disabled
  //
  virtual bool alwaysEnabled() const { return false; }
  virtual bool alwaysDisabled() const { return false; }

  /**
   * @return true if the mod can be updated.
   */
  virtual bool canBeUpdated() const { return false; }

  /**
   * @return the mod update check expiration date.
   */
  virtual QDateTime getExpires() const = 0;

  /**
   * @return true if the mod can be enabled/disabled.
   */
  virtual bool canBeEnabled() const { return false; }

  /**
   * @return a list of flags for this mod.
   */
  virtual std::vector<EFlag> getFlags() const = 0;

  /**
   * @return a list of conflict flags for this mod.
   */
  virtual std::vector<EConflictFlag> getConflictFlags() const = 0;

  /**
   * @return a list of content types contained in a mod.
   *
   * @note The IDs of the content are game-dependent. See the ModDataContent game
   * feature for more details on this.
   */
  virtual const std::set<int>& getContents() const = 0;

  /**
   * @brief Check if the specified flag is set for this mod.
   *
   * @param flag The flag to test.
   *
   * @return true if the flag is set, false otherwise.
   */
  bool hasFlag(EFlag flag) const;

  /**
   * @brief Check if any of the provided flags are set for this mod.
   *
   * @param flags The flags to test.
   *
   * @return true if any of the flags are set, false otherwise.
   */
  bool hasAnyOfTheseFlags(std::vector<ModInfo::EFlag> flags) const;

  /**
   * @brief Check if this mod contains the specified content.
   *
   * @param content ID of the content to test.
   *
   * @return true if the content is there, false otherwise.
   */
  virtual bool hasContent(int content) const = 0;

  /**
   * @return an indicator if and how this mod should be highlighted in the UI.
   */
  virtual int getHighlight() const { return HIGHLIGHT_NONE; }

  /**
   * @return the list of INI tweaks in this mod.
   */
  virtual std::vector<QString> getIniTweaks() const = 0;

  /**
   * @return the description of the mod, to display in the UI.
   */
  virtual QString getDescription() const = 0;

  /**
   * @return the creation time of this mod.
   */
  virtual QDateTime creationTime() const = 0;

  /**
   * @return the list of files that, if they exist in the data directory are treated as
   * files in THIS mod.
   */
  virtual QStringList stealFiles() const { return QStringList(); }

  /**
   * @return the list of archives belonging to this mod (as absolute file paths).
   */
  virtual QStringList archives(bool checkOnDisk = false) = 0;

  /**
   * @brief Set the color of this mod for display.
   *
   * @param color New color of this mod.
   *
   * @note Currently, this changes the color of the cell under the "Notes" column.
   */
  virtual void setColor(QColor color) {}

  /**
   * @brief Adds the information that a file has been installed into this mod.
   *
   * @param modId ID of the mod installed.
   * @param fileId ID of the file installed.
   */
  virtual void addInstalledFile(int modId, int fileId) = 0;

  /**
   * @brief Check if the mod belongs to the specified category.
   *
   * @param categoryID ID of the category to test for.
   *
   * @return true if the mod belongs to the specified category.
   *
   * @note This does not verify the id actually identifies a category.
   */
  bool categorySet(int categoryID) const;

  /**
   * @brief Retrieves the whole list of categories (as ids) this mod belongs to.
   *
   * @return the IDs of categories this mod belongs to.
   */
  const std::set<int>& getCategories() const { return m_Categories; }

  /**
   * @brief Sets the new primary category of the mod.
   *
   * @param categoryID ID of the primary category to set.
   */
  virtual void setPrimaryCategory(int categoryID) { m_PrimaryCategory = categoryID; }

  /**
   * @return true if this mod is considered "valid", that is it contains data used by
   * the game.
   */
  virtual bool isValid() const = 0;

  /**
   * @brief Updates the mod to flag it as converted in order to ignore the alternate
   * game warning.
   */
  virtual void markConverted(bool) {}

  /**
   * @brief Updates the mod to flag it as valid in order to ignore the invalid game data
   *     flag.
   */
  virtual void markValidated(bool) {}

  /**
   * @brief Reads meta information from disk.
   */
  virtual void readMeta() {}

  /**
   * @brief Stores meta information back to disk.
   */
  virtual void saveMeta() {}

  /**
   * @brief Sets whether this mod uses a custom url.
   */
  virtual void setHasCustomURL(bool) {}

  /**
   * @brief Check whether this mod uses a custom url.
   *
   * @return true if this mod has a custom URL, false otherwise.
   */
  virtual bool hasCustomURL() const { return false; }

  /**
   * @brief Sets the custom url.
   */
  virtual void setCustomURL(QString const&) {}

  /**
   * @brief Sets the URL for this mod.
   *
   * In practice, this is a shortcut for setHasCustomURL followed by
   * setCustomURL.
   *
   * @param url The new URL.
   */
  void setUrl(QString const& url) override
  {
    setHasCustomURL(true);
    setCustomURL(url);
  }

  /**
   * If hasCustomURL() is true and getCustomURL() is not empty, tries to parse
   * the url using QUrl::fromUserInput() and returns it. Otherwise, returns an
   * empty QUrl.
   */
  QUrl parseCustomURL() const;

public:  // Nexus stuff
  /**
   * @brief Changes the nexus description text.
   *
   * @param description The current description text.
   */
  virtual void setNexusDescription(const QString& description) = 0;

  /**
   * @return the nexus file status (aka category ID).
   */
  virtual int getNexusFileStatus() const = 0;

  /**
   * @brief Sets the file status (category ID) from Nexus.
   *
   * @param status The status id of the installed file.
   */
  virtual void setNexusFileStatus(int status) = 0;

  /**
   * @return the nexus description of the mod (html).
   */
  virtual QString getNexusDescription() const = 0;

  /**
   * @brief Get the last time nexus was checked for file updates on this mod.
   */
  virtual QDateTime getLastNexusUpdate() const = 0;

  /**
   * @brief Sets the last time nexus was checked for file updates on this mod.
   */
  virtual void setLastNexusUpdate(QDateTime time) = 0;

  /**
   * @return the last time nexus was queried for infos on this mod.
   */
  virtual QDateTime getLastNexusQuery() const = 0;

  /**
   * @brief Sets the last time nexus was queried for info on this mod.
   */
  virtual void setLastNexusQuery(QDateTime time) = 0;

  /**
   * @return the last time the mod was updated on Nexus.
   */
  virtual QDateTime getNexusLastModified() const = 0;

  /**
   * @brief Set the last time the mod was updated on Nexus.
   */
  virtual void setNexusLastModified(QDateTime time) = 0;

  /**
   * @return the assigned nexus category ID
   */
  virtual int getNexusCategory() const = 0;

  /**
   * @brief Assigns the given Nexus category ID
   */
  virtual void setNexusCategory(int category) = 0;

public:  // Conflicts
  // retrieve the list of mods (as mod index) that are overwritten by this one.
  // Updates may be delayed.
  //
  virtual const std::set<unsigned int>& getModOverwrite() const { return s_EmptySet; }

  // retrieve the list of mods (as mod index) that overwrite this one.
  // Updates may be delayed.
  //
  virtual const std::set<unsigned int>& getModOverwritten() const { return s_EmptySet; }

  // retrieve the list of mods (as mod index) with archives that are overwritten by
  // this one. Updates may be delayed
  //
  virtual const std::set<unsigned int>& getModArchiveOverwrite() const
  {
    return s_EmptySet;
  }

  // retrieve the list of mods (as mod index) with archives that overwrite this one.
  // Updates may be delayed.
  //
  virtual const std::set<unsigned int>& getModArchiveOverwritten() const
  {
    return s_EmptySet;
  }

  // retrieve the list of mods (as mod index) with archives that are overwritten by
  // loose files of this mod. Updates may be delayed.
  //
  virtual const std::set<unsigned int>& getModArchiveLooseOverwrite() const
  {
    return s_EmptySet;
  }

  // retrieve the list of mods (as mod index) with loose files that overwrite this one's
  // archive files. Updates may be delayed.
  //
  virtual const std::set<unsigned int>& getModArchiveLooseOverwritten() const
  {
    return s_EmptySet;
  }

public slots:

  /**
   * @brief Notify this mod that the content of the disk may have changed.
   */
  virtual void diskContentModified() = 0;

signals:

  /**
   * @brief Emitted whenever the information of a mod changes.
   *
   * @param success true if the mod details were updated successfully, false if not.
   */
  void modDetailsUpdated(bool success);

protected:
  /**
   *
   */
  ModInfo(OrganizerCore& core);

  /**
   * @brief Prefetch content for this mod.
   *
   * This method can be used to prefetch content from the mod, e.g., for isValid()
   * or getContents(). This method will only be called when first creating the mod
   * using multiple threads for all the mods.
   */
  virtual void prefetch() = 0;
  static bool ByName(const ModInfo::Ptr& LHS, const ModInfo::Ptr& RHS);

protected:
  // the mod list
  OrganizerCore& m_Core;

  // the index of the mod in s_Collection, only valid after updateIndices()
  int m_Index;

  int m_PrimaryCategory;
  std::set<int> m_Categories;
  MOBase::VersionInfo m_Version;
  bool m_PluginSelected = false;

  // empty set that can be returned in overwrite functions by
  // default
  static const std::set<unsigned int> s_EmptySet;

protected:
  friend class OrganizerCore;

  /**
   * @brief Create a new mod from the specified directory and add it to the collection.
   *
   * @param dir Directory to create from.
   *
   * @return pointer to the info-structure of the newly created/added mod.
   */
  static ModInfo::Ptr createFrom(const QDir& dir, OrganizerCore& core);

  /**
   * @brief Create a new "foreign-managed" mod from a tuple of plugin and archives.
   *
   * @param espName Name of the plugin.
   * @param bsaNames Names of archives.
   *
   * @return a new mod.
   */
  static ModInfo::Ptr createFromPlugin(const QString& modName, const QString& espName,
                                       const QStringList& bsaNames,
                                       ModInfo::EModType modType, OrganizerCore& core);

  static ModInfo::Ptr createFromOverwrite(OrganizerCore& core);

  // update the m_Index attribute of all mods and the various mapping
  //
  static void updateIndices();

protected:
  static QRecursiveMutex s_Mutex;
  static std::vector<ModInfo::Ptr> s_Collection;
  static ModInfo::Ptr s_Overwrite;
  static std::map<QString, unsigned int, MOBase::FileNameComparator> s_ModsByName;
  static std::map<std::pair<QString, int>, std::vector<unsigned int>> s_ModsByModID;
  static int s_NextID;
};

#endif  // MODINFO_H
