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

#include "installdialog.h"

#include <iinstallationmanager.h>
#include <iplugininstaller.h>

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
  explicit InstallationManager(QWidget *parent);

  ~InstallationManager();

  /**
   * @brief install a mod from an archive
   *
   * @param fileName absolute file name of the archive to install
   * @param modName suggested name of the mod. If this is empty (the default), a name will be guessed based on the filename. The user will always have a chance to rename the mod
   * @param preferIntegrated if true, integrated installers are chosen over external installers
   * @return true if the archive was installed, false if installation failed or was refused
   * @exception std::exception an exception may be thrown if the archive can't be opened (maybe the format is invalid or the file is damaged)
   **/
  bool install(const QString &fileName, const QString &pluginsFileName, const QString &modsDirectory, bool preferIntegrated, bool enableQuickInstall, QString &modName, bool &hasIniTweaks);

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

  void installManual(MOBase::DirectoryTree::Node *baseNode, MOBase::DirectoryTree *filesTree, bool &hasIniTweaks, QString &modName, int categoryID, const QString &modsDirectory, QString newestVersion, QString version, int modID, bool success, bool manualRequest);

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
  virtual QStringList extractFiles(const QStringList &files);

  /**
   * @brief installs an archive
   * @param modName suggested name of the mod
   * @param archiveFile path to the archive to install
   * @return the installation result
   */
  virtual MOBase::IPluginInstaller::EInstallResult installArchive(const QString &modName, const QString &archiveName);

private:

  void queryPassword(LPSTR password);
  void updateProgress(float percentage);
  void updateProgressFile(LPCWSTR fileName);
  void report7ZipError(LPCWSTR errorMessage);

  void dummyProgressFile(LPCWSTR) {}

  MOBase::DirectoryTree *createFilesTree();

  // remap all files in the archive to the directory structure represented by baseNode
  // files not present in baseNode are disabled
  void mapToArchive(const MOBase::DirectoryTree::Node *baseNode);

  // recursive worker function for mapToArchive
  void mapToArchive(const MOBase::DirectoryTree::Node *node, std::wstring path, FileData * const *data);
  bool unpackPackageTXT();
  bool unpackSingleFile(const QString &fileName);


  bool isSimpleArchiveTopLayer(const MOBase::DirectoryTree::Node *node, bool bainStyle);
  MOBase::DirectoryTree::Node *getSimpleArchiveBase(MOBase::DirectoryTree *dataTree);
  bool checkBainPackage(MOBase::DirectoryTree *dataTree);
  bool checkFomodPackage(MOBase::DirectoryTree *dataTree, QString &offset, bool &xmlInstaller);
  bool checkNMMInstaller();

  void fixModName(QString &name);

  bool testOverwrite(const QString &modsDirectory, QString &modName);

  bool doInstall(const QString &modsDirectory, QString &modName,
                 int modID, const QString &version, const QString &newestVersion, int categoryID);

  bool installFomodExternal(const QString &fileName, const QString &pluginsFileName, const QString &modDirectory);
  bool installFomodInternal(MOBase::DirectoryTree *&baseNode, const QString &fomodPath, const QString &modsDirectory,
                            int modID, const QString &version, const QString &newestVersion,
                            int categoryID, QString &modName, bool nameGuessed, bool &manualRequest);
  QString generateBackupName(const QString &directoryName);

  bool ensureValidModName(QString &name);

private slots:

  void openFile(const QString &fileName);

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

  std::vector<MOBase::IPluginInstaller*> m_Installers;
  std::set<QString, CaseInsensitive> m_SupportedExtensions;

  QString m_NCCPath;

  Archive *m_CurrentArchive;
  QString m_CurrentFile;

  QProgressDialog m_InstallationProgress;

  std::set<QString> m_FilesToDelete;
  std::set<QString> m_TempFilesToDelete;

};


#endif // INSTALLATIONMANAGER_H
