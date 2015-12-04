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


#include <iinstallationmanager.h>
#include <iplugininstaller.h>
#include <guessedvalue.h>

#include <QObject>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <archive.h>
#include <QProgressDialog>
#include <set>
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
  bool install(const QString &fileName, MOBase::GuessedValue<QString> &modName, bool &hasIniTweaks);

  /**
   * @return true if the installation was canceled
   **/
  bool wasCancelled();

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
   * @brief extract the specified file from the currently open archive to a temporary location
   * @param (relative) name of the file within the archive
   * @return the absolute name of the temporary file
   * @note the call will fail with an exception if no archive is open (plugins deriving
   *       from IPluginInstallerSimple can rely on that, custom installers shouldn't)
   * @note the temporary file is automatically cleaned up after the installation
   * @note This call can be very slow if the archive is large and "solid"
   */
  virtual QString extractFile(const QString &fileName);

  /**
   * @brief extract the specified files from the currently open archive to a temporary location
   * @param (relative) names of files within the archive
   * @return the absolute names of the temporary files
   * @note the call will fail with an exception if no archive is open (plugins deriving
   *       from IPluginInstallerSimple can rely on that, custom installers shouldn't)
   * @note the temporary file is automatically cleaned up after the installation
   * @note This call can be very slow if the archive is large and "solid"
   */
  virtual QStringList extractFiles(const QStringList &files, bool flatten);

  /**
   * @brief installs an archive
   * @param modName suggested name of the mod
   * @param archiveFile path to the archive to install
   * @return the installation result
   */
  virtual MOBase::IPluginInstaller::EInstallResult installArchive(MOBase::GuessedValue<QString> &modName, const QString &archiveName);

  /**
   * @brief test if the specified mod name is free. If not, query the user how to proceed
   * @param modName current possible names for the mod
   * @param merge if this value is not null, the value will be set to whether the use chose to merge or replace
   * @return true if we can proceed with the installation, false if the user canceled or in case of an unrecoverable error
   */
  virtual bool testOverwrite(MOBase::GuessedValue<QString> &modName, bool *merge = nullptr) const;

private:

  void queryPassword(QString *password);
  void updateProgress(float percentage);
  void updateProgressFile(const QString &fileName);
  void report7ZipError(const QString &errorMessage);

  MOBase::DirectoryTree *createFilesTree();

  // remap all files in the archive to the directory structure represented by baseNode
  // files not present in baseNode are disabled
  void mapToArchive(const MOBase::DirectoryTree::Node *baseNode);

  // recursive worker function for mapToArchive
  void mapToArchive(const MOBase::DirectoryTree::Node *node, QString path, FileData * const *data);
  bool unpackSingleFile(const QString &fileName);


  bool isSimpleArchiveTopLayer(const MOBase::DirectoryTree::Node *node, bool bainStyle);
  MOBase::DirectoryTree::Node *getSimpleArchiveBase(MOBase::DirectoryTree *dataTree);

  bool doInstall(MOBase::GuessedValue<QString> &modName,
                 int modID, const QString &version, const QString &newestVersion, int categoryID, const QString &repository);

  QString generateBackupName(const QString &directoryName) const;

  bool ensureValidModName(MOBase::GuessedValue<QString> &name) const;

  void postInstallCleanup();

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

private:

  QWidget *m_ParentWidget;

  QString m_ModsDirectory;
  QString m_DownloadsDirectory;

  std::vector<MOBase::IPluginInstaller*> m_Installers;
  std::set<QString, CaseInsensitive> m_SupportedExtensions;

  Archive *m_ArchiveHandler;
  QString m_CurrentFile;

  QProgressDialog *m_InstallationProgress { nullptr };

  std::set<QString> m_TempFilesToDelete;

  QString m_URL;
};


#endif // INSTALLATIONMANAGER_H
