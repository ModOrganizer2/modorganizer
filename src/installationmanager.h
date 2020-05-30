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

#include <ifiletree.h>
#include <iinstallationmanager.h>
#include <iplugininstaller.h>
#include <guessedvalue.h>

#include <QObject>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <archive.h>
#include <QProgressDialog>
#include <set>
#include <map>
#include <errorcodes.h>


/**
 * @brief manages the installation of mod archives
 * This currently supports two special kind of archives:
 * - "simple" archives: properly packaged archives without options, so they can be extracted to the (virtual) data directory directly
 * - "complex" bain archives: archives with options for the bain system.
 * All other archives are managed through the manual "InstallDialog"
 * @todo this may be a good place to support plugins
 **/
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

  void setParentWidget(QWidget *widget);

  void setURL(const QString &url);

  /**
   * @brief update the directory where mods are to be installed
   * @param modsDirectory the mod directory
   * @note this is called a lot, probably redundantly
   */
  void setModsDirectory(const QString &modsDirectory) { m_ModsDirectory = modsDirectory; }

  /**
   * @brief update the directory where downloads are stored
   * @param downloadDirectory the download directory
   */
  void setDownloadDirectory(const QString &downloadDirectory) { m_DownloadsDirectory = downloadDirectory; }

  /**
   * @brief install a mod from an archive
   *
   * @param fileName absolute file name of the archive to install
   * @param modName suggested name of the mod. If this is empty (the default), a name will be guessed based on the filename. The user will always have a chance to rename the mod
   * @return true if the archive was installed, false if installation failed or was refused
   * @exception std::exception an exception may be thrown if the archive can't be opened (maybe the format is invalid or the file is damaged)
   **/
  MOBase::IPluginInstaller::EInstallResult install(const QString &fileName, MOBase::GuessedValue<QString> &modName, bool &hasIniTweaks, int modID = 0);

  /**
   * @return true if the installation was canceled
   **/
  bool wasCancelled();

  /**
   * @return true if an installation is currently in progress
   **/
  bool isRunning();

  /**
   * @brief retrieve a string describing the specified error code
   *
   * @param errorCode an error code as returned by the archiving function
   * @return the error string
   * @todo This function doesn't belong here, it is only public because the SelfUpdater class also uses "Archive" to get to the package.txt file
   **/
  static QString getErrorString(Archive::Error errorCode);

  /**
   * @brief register an installer-plugin
   * @param the installer to register
   */
  void registerInstaller(MOBase::IPluginInstaller *installer); 
  
  /**
   * @return list of file extensions we can install
   */
  QStringList getSupportedExtensions() const;

  /**
   * @brief Extract the specified file from the currently opened archive to a temporary location.
   *
   * This method cannot be used to extract directory.
   *
   * @param entry Entry corresponding to the file to extract.
   *
   * @return the absolute path to the temporary file.
   *
   * @note The call will fail with an exception if no archive is open (plugins deriving
   *       from IPluginInstallerSimple can rely on that, custom installers should not).
   * @note The temporary file is automatically cleaned up after the installation.
   * @note This call can be very slow if the archive is large and "solid".
   */
  virtual QString extractFile(std::shared_ptr<const MOBase::FileTreeEntry> entry) override;

  /**
   * @brief Extract the specified files from the currently opened archive to a temporary location.
   *
   * This method cannot be used to extract directory.
   *
   * @param entres Entries corresponding to the files to extract.
   *
   * @return the list of absolute paths to the temporary files.
   *
   * @note The call will fail with an exception if no archive is open (plugins deriving
   *       from IPluginInstallerSimple can rely on that, custom installers should not).
   * @note The temporary file is automatically cleaned up after the installation.
   * @note This call can be very slow if the archive is large and "solid".
   *
   * The flatten argument is not present here while it is present in the deprecated QStringList
   * version for multiple reasons: 1) it was never used, 2) it is kind of fishy because there
   * is no way to know if a file is going to be overriden, 3) it is quite easy to flatten a 
   * IFileTree and thus to given a list of entries flattened (this was not possible with the
   * QStringList version since these were based on the name of the file inside the archive).
   */
  virtual QStringList extractFiles(std::vector<std::shared_ptr<const MOBase::FileTreeEntry>> const& entries) override;

  /**
   * @brief Create a new file on the disk corresponding to the given entry.
   *
   * This method can be used by installer that needs to create files that are not in the original
   * archive. At the end of the installation, if there are entries in the final tree that were used
   * to create files, the corresponding files will be moved to the mod folder.
   *
   * @param entry The entry for which a temporary file should be created.
   *
   * @return the path to the created file.
   */
  virtual QString createFile(std::shared_ptr<const MOBase::FileTreeEntry> entry) override;
  
  /**
   * @brief Installs the given archive.
   *
   * @param modName Suggested name of the mod.
   * @param archiveFile Path to the archive to install.
   * @param modId ID of the mod, if available.
   *
   * @return the installation result.
   */
  virtual MOBase::IPluginInstaller::EInstallResult installArchive(MOBase::GuessedValue<QString> &modName, const QString &archiveName, int modId = 0) override;

  /**
   * @brief test if the specified mod name is free. If not, query the user how to proceed
   * @param modName current possible names for the mod
   * @param merge if this value is not null, the value will be set to whether the use chose to merge or replace
   * @return true if we can proceed with the installation, false if the user canceled or in case of an unrecoverable error
   */
  virtual bool testOverwrite(MOBase::GuessedValue<QString> &modName, bool *merge = nullptr) const;

  QString generateBackupName(const QString &directoryName) const;

private:

  void queryPassword(QString *password);
  void updateProgress(float percentage);
  void updateProgressFile(const QString &fileName);
  void report7ZipError(const QString &errorMessage);

  // Recursive worker function for mapToArchive (takes raw reference for "speed").
  bool unpackSingleFile(const QString &fileName);

  MOBase::IPluginInstaller::EInstallResult doInstall(MOBase::GuessedValue<QString> &modName, QString gameName,
                 int modID, const QString &version, const QString &newestVersion, int categoryID, int fileCategoryID, const QString &repository);

  /**
   * @brief Clean the list of created files by removing all entries that are not
   *     in the given tree.
   *
   * @param tree The parent tree. Usually the tree returned by the installer.
   */
  void cleanCreatedFiles(std::shared_ptr<const MOBase::IFileTree> fileTree);

  bool ensureValidModName(MOBase::GuessedValue<QString> &name) const;

  void postInstallCleanup();

signals:

  void progressUpdate(float percentage);
  void progressUpdate(QString const fileName);

private:

  struct ByPriority {
    bool operator()(MOBase::IPluginInstaller *LHS, MOBase::IPluginInstaller *RHS) const
    {
      return LHS->priority() > RHS->priority();
    }
  };

  struct CaseInsensitive {
      bool operator() (const QString &LHS, const QString &RHS) const
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
  bool extractFiles(QDir extractPath);

private:

  bool m_IsRunning;

  QWidget *m_ParentWidget;

  QString m_ModsDirectory;
  QString m_DownloadsDirectory;

  std::vector<MOBase::IPluginInstaller*> m_Installers;
  std::set<QString, CaseInsensitive> m_SupportedExtensions;

  Archive *m_ArchiveHandler;
  QString m_CurrentFile;
  QString m_ErrorMessage;

  // List of creates files:
  std::map<std::shared_ptr<const MOBase::FileTreeEntry>, QString> m_CreatedFiles;

  QProgressDialog *m_InstallationProgress { nullptr };
  int m_Progress;
  QString m_ProgressFile;

  std::set<QString> m_TempFilesToDelete;

  QString m_URL;
};


#endif // INSTALLATIONMANAGER_H
