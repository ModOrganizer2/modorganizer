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

#ifndef PROFILE_H
#define PROFILE_H


#include "modinfo.h"
#include <iprofile.h>
#include <delayedfilewriter.h>

#include <QString>
#include <QDir>
#include <QSettings>

#include <vector>
#include <tuple>


namespace MOBase { class IPluginGame; }

/**
 * @brief represents a profile
 **/
class Profile : public QObject, public MOBase::IProfile
{

  Q_OBJECT

public:

  typedef boost::shared_ptr<Profile> Ptr;

public:

  /**
   * @brief constructor
   *
   * This constructor is used to create a new profile so it is to be assumed a profile
   * by this name does not yet exist
   * @param name name of the new profile
   * @param filter save game filter. Defaults to &lt;no filter&gt;.
   **/
  Profile(const QString &name, MOBase::IPluginGame const *gamePlugin, bool useDefaultSettings);

  /**
   * @brief constructor
   *
   * This constructor is used to open an existing profile though it will also try to repair
   * the profile if important files are missing (including the directory itself) so technically,
   * invoking this should always produce a working profile
   * @param directory directory to read the profile from
   **/
  Profile(const QDir &directory, MOBase::IPluginGame const *gamePlugin);

  Profile(const Profile &reference);

  ~Profile();

  /**
   * @return true if this profile (still) exists on disc
   */
  bool exists() const;

  /**
   * @param name of the new profile
   * @param reference profile to copy from
   **/
  static Profile *createPtrFrom(const QString &name, const Profile &reference, MOBase::IPluginGame const *gamePlugin);

  MOBase::DelayedFileWriter &modlistWriter() { return m_ModListWriter; }

  /**
   * @brief test if this profile uses archive invalidation
   *
   * @param supported if this is not null, the parameter will be set to false if invalidation is not supported in this profile
   * @return true if archive invalidation is active
   * @note currently, invalidation is not supported if the relevant entry in the ini file does not exist
   **/
  bool invalidationActive(bool *supported) const;

  /**
   * @brief deactivate archive invalidation if it was active
   **/
  void deactivateInvalidation();

  /**
   * @brief activate archive invalidation
   *
   * @param dataDirectory data directory of the game
   * @todo passing the data directory as a parameter is useless, the function should
   *       be able to query it from GameInfo
   **/
  void activateInvalidation();

  /**
   * @return true if this profile uses local save games
   */
  bool localSavesEnabled() const;

  /**
   * @brief enables or disables the use of local save games for this profile
   * disabling this does not delete exising local saves but they will not be visible
   * in the game
   * @param enable if true, local saves are enabled, otherewise they are disabled
   */
  bool enableLocalSaves(bool enable);

  /**
   * @return name of the profile (this is identical to its directory name)
   **/
  QString name() const { return m_Directory.dirName(); }

  /**
   * @return the path of the plugins file in this profile
   * @todo is this required? can the functionality using this function be moved to the Profile-class?
   **/
  QString getPluginsFileName() const;

  /**
   * @return the path of the loadorder file in this profile
   **/
  QString getLoadOrderFileName() const;

  /**
   * @return the path of the file containing locked mod indices
   */
  QString getLockedOrderFileName() const;

  /**
   * @return the path of the modlist file in this profile
   */
  QString getModlistFileName() const;

  /**
   * @return path of the archives file in this profile
   */
  QString getArchivesFileName() const;

  /**
   * @return the path of the delete file in this profile
   * @note the deleter file lists plugins that should be hidden from the game and other tools
   **/
  QString getDeleterFileName() const;

  /**
   * @return the path of the ini file in this profile
   * @todo since the game can contain multiple ini files (i.e. skyrim.ini skyrimprefs.ini)
   *       the concept of this function is somewhat broken
   **/
  QString getIniFileName() const;

  /**
   * @return the path of the tweak ini in this profile
   */
  QString getProfileTweaks() const;

  /**
   * @return path to this profile
   **/
  QString absolutePath() const;

  void rename(const QString &newName);

  /**
   * @brief create the ini file to be used by the game
   *
   * the tweaked ini file constructed by this file is a merger
   * of the game-ini of this profile with ini tweaks applied */
  void createTweakedIniFile();

  /**
   * @brief re-read the modlist.txt and update the mod status from it
   **/
  void refreshModStatus();

  /**
   * @brief retrieve a list of mods that are enabled in this profile
   *
   * @return list of active mods sorted by priority (ascending). "first" is the mod name, "second" is its path
   **/
  std::vector<std::tuple<QString, QString, int> > getActiveMods();

  /**
   * retrieve the number of mods for which this object has status information.
   * This is usually the same as ModInfo::getNumMods() except between
   * calls to ModInfo::updateFromDisc() and the Profile::refreshModStatus()
   * 
   * @return number of mods for which the profile has status information
   **/
  unsigned int numMods() const { return m_ModStatus.size(); }

  /**
   * @return the number of mods that can be enabled and where the priority can be modified
   */
  unsigned int numRegularMods() const { return m_NumRegularMods; }

  /**
   * @brief retrieve the mod index based on the priority
   *
   * @param priority priority to look up
   * @return the index of the mod
   * @throw std::exception an exception is thrown if there is no mod with the specified priority
   **/
  unsigned int modIndexByPriority(unsigned int priority) const;

  /**
   * @brief enable or disable a mod
   *
   * @param index index of the mod to enable/disable
   * @param enabled true if the mod is to be enabled, false if it is to be disabled
   **/
  void setModEnabled(unsigned int index, bool enabled);

  /**
   * change the priority of a mod. Of course this also changes the priority of other mods.
   * The priority of the mods in the range ]old, new priority] are shifted so that no gaps
   * are possible.
   *
   * @param index index of the mod to change
   * @param newPriority the new priority value
   *
   * @todo what happens if the new priority is outside the range?
   **/
  void setModPriority(unsigned int index, int &newPriority);

  /**
   * @brief determine if a mod is enabled
   *
   * @param index index of the mod to look up
   * @return true if the mod is enabled, false otherwise
   **/
  bool modEnabled(unsigned int index) const;

  /**
   * @brief query the priority of a mod
   *
   * @param index index of the mod to look up
   * @return priority of the specified mod
   **/
  int getModPriority(unsigned int index) const;

  void dumpModStatus() const;

signals:

  /**
   * @brief emitted whenever the status (enabled/disabled) of a mod changed
   *
   * @param index index of the mod that changed
   **/
  void modStatusChanged(unsigned int index);

public slots:

  void writeModlistNow();

private:

  class ModStatus {
    friend class Profile;
  public:
    ModStatus() : m_Overwrite(false), m_Enabled(false), m_Priority(-1) {}
  private:
    bool m_Overwrite;
    bool m_Enabled;
    int m_Priority;
  };

private:
  Profile& operator=(const Profile &reference); // not implemented

  void initTimer();

  void updateIndices();

  void copyFilesTo(QString &target) const;

  std::vector<std::wstring> splitDZString(const wchar_t *buffer) const;
  void mergeTweak(const QString &tweakName, const QString &tweakedIni) const;
  void mergeTweaks(ModInfo::Ptr modInfo, const QString &tweakedIni) const;
  void touchFile(QString fileName);
  void finishChangeStatus() const;

private:

  QDir m_Directory;

  MOBase::IPluginGame const * const m_GamePlugin;

  mutable QByteArray m_LastModlistHash;
  std::vector<ModStatus> m_ModStatus;
  std::vector<unsigned int> m_ModIndexByPriority;
  unsigned int m_NumRegularMods;

  MOBase::DelayedFileWriter m_ModListWriter;

};


#endif // PROFILE_H
