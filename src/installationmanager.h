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

#ifndef INSTALLATIONMANAGER_H
#define INSTALLATIONMANAGER_H

#include <archive/archive.h>
#include <uibase/errorcodes.h>
#include <uibase/guessedvalue.h>
#include <uibase/ifiletree.h>
#include <uibase/iinstallationmanager.h>
#include <uibase/iplugininstaller.h>

#include <QObject>
#define WIN32_LEAN_AND_MEAN
#include <QProgressDialog>
#include <Windows.h>
#include <map>
#include <set>

#include "modinfo.h"
#include "pluginmanager.h"

// contains installation result from the manager, internal class
// for MO2 that is not forwarded to plugin
class InstallationResult
{
public:
  // result status of the installation
  //
  auto result() const { return m_result; }

  // information about the installation, only valid for successful
  // installation
  //
  QString name() const { return m_name; }
  bool backupCreated() const { return m_backup; }
  bool merged() const { return m_merged; }
  bool replaced() const { return m_replaced; }
  bool hasIniTweaks() const { return m_iniTweaks; }
  bool mergedOrReplaced() const { return merged() || replaced(); }

  // check if the installation was a success
  //
  explicit operator bool() const
  {
    return result() == MOBase::IPluginInstaller::EInstallResult::RESULT_SUCCESS;
  }

private:
  friend class InstallationManager;

  // create a failed result
  InstallationResult(MOBase::IPluginInstaller::EInstallResult result =
                         MOBase::IPluginInstaller::EInstallResult::RESULT_FAILED);

  MOBase::IPluginInstaller::EInstallResult m_result;

  QString m_name;

  bool m_iniTweaks;
  bool m_backup;
  bool m_merged;
  bool m_replaced;
};

class InstallationManager : public QObject, public MOBase::IInstallationManager
{
  Q_OBJECT

public:
  /**
   * @brief constructor
   *
   * @param parent parent object.
   **/
  explicit InstallationManager();

  virtual ~InstallationManager();

  void setParentWidget(QWidget* widget);

  /**
   * @brief Notify all installer plugins that an installation is about to start.
   *
   * @param archive Path to the archive that is going to be installed.
   * @param reinstallation True if this is a reinstallation, false otherwise.
   * @param currentMod The installed mod corresponding to the archive being installed,
   * or a null if there is no such mod.
   */
  void notifyInstallationStart(QString const& archive, bool reinstallation,
                               ModInfo::Ptr currentMod);

  /**
   * @brief notify all installer plugins that an installation has ended.
   *
   * @param result The result of the installation process.
   * @param currentMod The newly install mod, if result is SUCCESS, a null pointer
   * otherwise.
   */
  void notifyInstallationEnd(const InstallationResult& result, ModInfo::Ptr newMod);

  /**
   * @brief update the directory where mods are to be installed
   * @param modsDirectory the mod directory
   * @note this is called a lot, probably redundantly
   */
  void setModsDirectory(const QString& modsDirectory)
  {
    m_ModsDirectory = modsDirectory;
  }

  /**
   *
   */
  void setPluginManager(const PluginManager* pluginManager);

  /**
   * @brief update the directory where downloads are stored
   * @param downloadDirectory the download directory
   */
  void setDownloadDirectory(const QString& downloadDirectory)
  {
    m_DownloadsDirectory = downloadDirectory;
  }

  /**
   * @brief install a mod from an archive
   *
   * @param fileName absolute file name of the archive to install
   * @param modName suggested name of the mod. If this is empty (the default), a name
   *will be guessed based on the filename. The user will always have a chance to rename
   *the mod
   * @return true if the archive was installed, false if installation failed or was
   *refused
   * @exception std::exception an exception may be thrown if the archive can't be opened
   *(maybe the format is invalid or the file is damaged)
   **/
  InstallationResult install(const QString& fileName,
                             MOBase::GuessedValue<QString>& modName, int modID = 0);

  /**
   * @return true if the installation was canceled
   **/
  bool wasCancelled() const;

  /**
   * @return true if an installation is currently in progress
   **/
  bool isRunning() const;

  /**
   * @brief retrieve a string describing the specified error code
   *
   * @param errorCode an error code as returned by the archiving function
   * @return the error string
   * @todo This function doesn't belong here, it is only public because the SelfUpdater
   *class also uses "Archive" to get to the package.txt file
   **/
  static QString getErrorString(Archive::Error errorCode);

  /**
   * @return the extensions of archives supported by this installation manager.
   */
  QStringList getSupportedExtensions() const override;

  /**
   * @brief Extract the specified file from the currently opened archive to a temporary
   * location.
   *
   * This method cannot be used to extract directory.
   *
   * @param entry Entry corresponding to the file to extract.
   * @param silent If true, the dialog showing extraction progress will not be shown.
   *
   * @return the absolute path to the temporary file.
   *
   * @note The call will fail with an exception if no archive is open (plugins deriving
   *       from IPluginInstallerSimple can rely on that, custom installers should not).
   * @note The temporary file is automatically cleaned up after the installation.
   * @note This call can be very slow if the archive is large and "solid".
   */
  QString extractFile(std::shared_ptr<const MOBase::FileTreeEntry> entry,
                      bool silent = false) override;

  /**
   * @brief Extract the specified files from the currently opened archive to a temporary
   * location.
   *
   * This method cannot be used to extract directory.
   *
   * @param entres Entries corresponding to the files to extract.
   * @param silent If true, the dialog showing extraction progress will not be shown.
   *
   * @return the list of absolute paths to the temporary files.
   *
   * @note The call will fail with an exception if no archive is open (plugins deriving
   *       from IPluginInstallerSimple can rely on that, custom installers should not).
   * @note The temporary file is automatically cleaned up after the installation.
   * @note This call can be very slow if the archive is large and "solid".
   *
   * The flatten argument is not present here while it is present in the deprecated
   * QStringList version for multiple reasons: 1) it was never used, 2) it is kind of
   * fishy because there is no way to know if a file is going to be overriden, 3) it is
   * quite easy to flatten a IFileTree and thus to given a list of entries flattened
   * (this was not possible with the QStringList version since these were based on the
   * name of the file inside the archive).
   */
  QStringList
  extractFiles(std::vector<std::shared_ptr<const MOBase::FileTreeEntry>> const& entries,
               bool silent = false) override;

  /**
   * @brief Create a new file on the disk corresponding to the given entry.
   *
   * This method can be used by installer that needs to create files that are not in the
   * original archive. At the end of the installation, if there are entries in the final
   * tree that were used to create files, the corresponding files will be moved to the
   * mod folder.
   *
   * @param entry The entry for which a temporary file should be created.
   *
   * @return the path to the created file.
   */
  QString createFile(std::shared_ptr<const MOBase::FileTreeEntry> entry) override;

  /**
   * @brief Installs the given archive.
   *
   * @param modName Suggested name of the mod.
   * @param archiveFile Path to the archive to install.
   * @param modId ID of the mod, if available.
   *
   * @return the installation result.
   */
  MOBase::IPluginInstaller::EInstallResult
  installArchive(MOBase::GuessedValue<QString>& modName, const QString& archiveName,
                 int modId = 0) override;

  /**
   * @param modName current possible names for the mod
   *
   * @return an installation result containing information from the user.
   */
  InstallationResult testOverwrite(MOBase::GuessedValue<QString>& modName);

  QString generateBackupName(const QString& directoryName) const;

private:
  // actually perform the installation (write files to the disk, etc.), returns the
  // installation result
  //
  InstallationResult doInstall(MOBase::GuessedValue<QString>& modName, QString gameName,
                               int modID, const QString& version,
                               const QString& newestVersion, int categoryID,
                               int fileCategoryID, const QString& repository);

  /**
   * @brief Clean the list of created files by removing all entries that are not
   *     in the given tree.
   *
   * @param tree The parent tree. Usually the tree returned by the installer.
   */
  void cleanCreatedFiles(std::shared_ptr<const MOBase::IFileTree> fileTree);

  bool ensureValidModName(MOBase::GuessedValue<QString>& name) const;

  void postInstallCleanup();

private slots:

  /**
   * @brief Query user for password and update the m_Password field.
   */
  void queryPassword();

signals:

  /**
   * @brief Emitted when a password is requested from the archive wrapper.
   */
  void passwordRequested();

  /**
   * @brief Progress update from the extraction.
   */
  void progressUpdate();

  /**
   * @brief An existing mod has been replaced with a newly installed one.
   */
  void modReplaced(const QString fileName);

private:
  struct CaseInsensitive
  {
    bool operator()(const QString& LHS, const QString& RHS) const
    {
      return QString::compare(LHS, RHS, Qt::CaseInsensitive) < 0;
    }
  };

  /**
   * @brief Extract the files from the archived that are not disabled (that have
   *     output filenames associated with them) to the given path.
   *
   * @param extractPath Path (on the disk) were the extracted files should be put.
   *
   * This method is mainly a convenience method for the extractFiles() methods.
   *
   * @return true if the extraction was successful, false if the extraciton was
   *     cancelled. If an error occured, an exception is thrown.
   */
  bool extractFiles(QString extractPath, QString title, bool showFilenames,
                    bool silent);

private:
  // The plugin container, mostly to check if installer are enabled or not.
  const PluginManager* m_PluginManager;

  bool m_IsRunning;

  QWidget* m_ParentWidget;

  QString m_ModsDirectory;
  QString m_DownloadsDirectory;

  // Archive management.
  std::unique_ptr<Archive> m_ArchiveHandler;
  QString m_CurrentFile;
  QString m_Password;

  // Map from entries in the tree that is used by the installer and absolute
  // paths to temporary files.
  std::map<std::shared_ptr<const MOBase::FileTreeEntry>, QString> m_CreatedFiles;
  std::set<QString> m_TempFilesToDelete;
};

#endif  // INSTALLATIONMANAGER_H
