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

#include <tuple>

#include "installationmanager.h"

#include "utility.h"
#include "report.h"
#include "categories.h"
#include "questionboxmemory.h"
#include "settings.h"
#include "queryoverwritedialog.h"
#include "messagedialog.h"
#include "iplugininstallersimple.h"
#include "iplugininstallercustom.h"
#include "nexusinterface.h"
#include "selectiondialog.h"
#include "modinfo.h"
#include <scopeguard.h>
#include <utility.h>
#include <scopeguard.h>

#include <QFileInfo>
#include <QLibrary>
#include <QInputDialog>
#include <QRegExp>
#include <QDir>
#include <QMessageBox>
#include <QSettings>
#include <QPushButton>
#include <QApplication>
#include <QDateTime>
#include <QDirIterator>
#include <QDebug>
#include <QTextDocument>
#include <QtConcurrent/QtConcurrentRun>

#include <Shellapi.h>

#include <boost/assign.hpp>
#include <boost/scoped_ptr.hpp>

#include "archivefiletree.h"


using namespace MOBase;
using namespace MOShared;


template <typename T>
static T resolveFunction(QLibrary &lib, const char *name)
{
  T temp = reinterpret_cast<T>(lib.resolve(name));
  if (temp == nullptr) {
    throw std::runtime_error(QObject::tr("invalid 7-zip32.dll: %1")
                                 .arg(lib.errorString())
                                 .toLatin1()
                                 .constData());
  }
  return temp;
}

InstallationManager::InstallationManager()
    : m_ParentWidget(nullptr),
      m_SupportedExtensions({"zip", "rar", "7z", "fomod", "001"}),
      m_IsRunning(false) {
  m_ArchiveHandler = CreateArchive();
  if (!m_ArchiveHandler->isValid()) {
    throw MyException(getErrorString(m_ArchiveHandler->getLastError()));
  }
  m_ArchiveHandler->setLogCallback([](auto level, auto const& message) {
    using LogLevel = Archive::LogLevel;
    switch (level) {
    case LogLevel::Debug:
      log::debug("{}", message);
      break;
    case LogLevel::Info:
      log::info("{}", message);
      break;
    case LogLevel::Warning:
      log::warn("{}", message);
      break;
    case LogLevel::Error:
      log::error("{}", message);
      break;
    }
  });

  // Connect the query password slot - This is the only way I found to be able to query user
  // from a separate thread. We use a BlockingQueuedConnection so that calling passwordRequested()
  // will block until the end of the slot.
  connect(this, &InstallationManager::passwordRequested, 
    this, &InstallationManager::queryPassword, Qt::BlockingQueuedConnection);
}

InstallationManager::~InstallationManager()
{
}

void InstallationManager::setParentWidget(QWidget *widget)
{
  m_ParentWidget = widget;
  for (IPluginInstaller *installer : m_Installers) {
    installer->setParentWidget(widget);
  }
}

void InstallationManager::setURL(QString const &url)
{
  m_URL = url;
}

void InstallationManager::queryPassword() {
  m_Password = QInputDialog::getText(m_ParentWidget, tr("Password required"),
    tr("Password"), QLineEdit::Password);
}

bool InstallationManager::extractFiles(QString extractPath, QString title, bool showFilenames, bool silent)
{
  TimeThis tt("InstallationManager::extractFiles");

  // Callback for errors:
  QString errorMessage;
  auto errorCallback = [&errorMessage, this](std::wstring const& message) {
    m_ArchiveHandler->cancel();
    errorMessage = QString::fromStdWString(message);
  };

  // The future that will hold the result:
  QFuture<bool> future;

  if (silent) {
    future = QtConcurrent::run([&]() -> bool {
      return m_ArchiveHandler->extract(
        extractPath.toStdWString(),
        nullptr,
        nullptr,
        errorCallback
      );
    });
    future.waitForFinished();
  }
  else {
    QProgressDialog* installationProgress = new QProgressDialog(m_ParentWidget);
    ON_BLOCK_EXIT([=]() {
      installationProgress->cancel();
      installationProgress->hide();
      installationProgress->deleteLater();
      });
    installationProgress->setWindowFlags(
      installationProgress->windowFlags() & (~Qt::WindowContextHelpButtonHint));
    if (!title.isEmpty()) {
      installationProgress->setWindowTitle(title);
    }
    installationProgress->setWindowModality(Qt::WindowModal);
    installationProgress->setFixedSize(600, 100);

    // Turn off auto-reset otherwize the progress dialog is reset before the end. This
    // is kind of annoying because updateProgress consider percentage of progression
    // through the archive (pack), while we are waiting for extracting archive entries, so
    // the percentage of in updateProgress is not really related to the percentage of files
    // extracted...
    installationProgress->setAutoReset(false);

    // Connect signals emitted by the extraction callback to the progress dialog slots:
    connect(this, &InstallationManager::progressUpdate, installationProgress, &QProgressDialog::setValue);
    connect(this, &InstallationManager::progressFileChange, installationProgress, &QProgressDialog::setLabelText);

    // Cancelling progress only cancel the extraction, we do not force exiting the event-loop:
    connect(installationProgress, &QProgressDialog::canceled, [this]() { m_ArchiveHandler->cancel(); });

    installationProgress->show();

    // The callbacks:
    auto progressCallback = [this](auto progressType, uint64_t current, uint64_t total) {
      if (progressType == Archive::ProgressType::EXTRACTION) {
        int progress = static_cast<int>(100 * current / total);
        emit progressUpdate(progress);
      }
    };
    Archive::FileChangeCallback fileChangeCallback = [this](auto changeType, std::wstring const& file) {
      if (changeType == Archive::FileChangeType::EXTRACTION_START) {
        emit progressFileChange(QString::fromStdWString(file));
      }
    };

    // Future watcher to exit the loop:
    QFutureWatcher<bool> futureWatcher;

    QEventLoop loop(this);
    connect(
      &futureWatcher, &QFutureWatcher<bool>::finished,
      &loop, &QEventLoop::quit,
      Qt::QueuedConnection);

    // unpack only the files we need for the installer
    futureWatcher.setFuture(QtConcurrent::run([&]() -> bool {
      return m_ArchiveHandler->extract(
        extractPath.toStdWString(),
        progressCallback,
        showFilenames ? fileChangeCallback : nullptr,
        errorCallback
      );
    }));

    // Wait for future to complete:
    loop.exec();
    
    future = futureWatcher.future();
  }

  // Check the result:
  if (!future.result()) {
    if (m_ArchiveHandler->getLastError() == Archive::Error::ERROR_EXTRACT_CANCELLED) {
      if (!errorMessage.isEmpty()) {
        throw MyException(tr("Extraction failed: %1").arg(errorMessage));
      }
      else {
        return false;
      }
    }
    else {
      throw MyException(tr("Extraction failed: %1").arg(static_cast<int>(m_ArchiveHandler->getLastError())));
    }
  }

  return true;
}

QString InstallationManager::extractFile(std::shared_ptr<const FileTreeEntry> entry, bool silent)
{
  QStringList result = this->extractFiles({ entry }, silent);
  return result.isEmpty() ? QString() : result[0];
}

QStringList InstallationManager::extractFiles(std::vector<std::shared_ptr<const FileTreeEntry>> const& entries, bool silent)
{
  // Remove the directory since mapToArchive would add them:
  std::vector<std::shared_ptr<const FileTreeEntry>> files;
  std::copy_if(entries.begin(), entries.end(), std::back_inserter(files),
    [](auto const& entry) { return entry->isFile(); });

  // Update the archive:
  ArchiveFileTree::mapToArchive(*m_ArchiveHandler, files);

  // Retrieve the file path:
  QStringList result;

  for (auto &entry : files) {
    auto path = entry->path();
    result.append(QDir::tempPath().append("/").append(path));
    m_TempFilesToDelete.insert(path);
  }

  if (!extractFiles(QDir::tempPath(), tr("Extracting files"), false, silent)) {
    return QStringList();
  }

  return result;
}


QString InstallationManager::createFile(std::shared_ptr<const MOBase::FileTreeEntry> entry) 
{
  // Use QTemporaryFile to create the temporary file with the given template:
  QTemporaryFile tempFile(QDir::cleanPath(QDir::tempPath() + QDir::separator() + "mo2-install"));

  // Turn-off autoRemove otherwise the file is deleted when destructor is called:
  tempFile.setAutoRemove(false);

  // Open/Close the file so that installer can use it properly:
  if (!tempFile.open()) {
    return QString();
  }
  tempFile.close();

  // fileName() returns the full path since we provide a full path in the constructor:
  const QString absPath = tempFile.fileName();

  m_CreatedFiles[entry] = absPath;
  m_TempFilesToDelete.insert(QDir::temp().relativeFilePath(absPath));

  // Returns the path with native separators:
  return QDir::toNativeSeparators(absPath);
}

void InstallationManager::cleanCreatedFiles(std::shared_ptr<const MOBase::IFileTree> fileTree) 
{
  // We simply have to check if all the entries have fileTree as a parent:
  for (auto it = std::begin(m_CreatedFiles); it != std::end(m_CreatedFiles); ) {
    
    // Find the parent - Could this be in FileTreeEntry?
    bool found = false;
    {
      auto parent = it->first->parent();
      while (parent && !found) {
        if (parent == fileTree) {
          found = true;
        }
        else {
          parent = parent->parent();
        }
      }
    }

    // If the parent was not found, we remove the entry, otherwize we move to the next one:
    if (!found) {
      it = m_CreatedFiles.erase(it);
    }
    else {
      ++it;
    }
  }
}


IPluginInstaller::EInstallResult InstallationManager::installArchive(GuessedValue<QString> &modName, const QString &archiveName, int modId)
{
  // in earlier versions the modName was copied here and the copy passed to install. I don't know why I did this and it causes
  // a problem if this is called by the bundle installer and the bundled installer adds additional names that then end up being used,
  // because the caller will then not have the right name.
  bool iniTweaks;
  return install(archiveName, modName, iniTweaks, modId);
}


QString InstallationManager::generateBackupName(const QString &directoryName) const
{
  QString backupName = directoryName + "_backup";
  if (QDir(backupName).exists()) {
    int idx = 2;
    QString temp = backupName + QString::number(idx);
    while (QDir(temp).exists()) {
      ++idx;
      temp = backupName + QString::number(idx);
    }
    backupName = temp;
  }
  return backupName;
}


bool InstallationManager::testOverwrite(GuessedValue<QString> &modName, bool *merge) const
{
  QString targetDirectory = QDir::fromNativeSeparators(m_ModsDirectory + "\\" + modName);

  while (QDir(targetDirectory).exists()) {
    Settings &settings(Settings::instance());

    const bool backup = settings.keepBackupOnInstall();
    QueryOverwriteDialog overwriteDialog(
      m_ParentWidget,
      backup ? QueryOverwriteDialog::BACKUP_YES : QueryOverwriteDialog::BACKUP_NO);

    if (overwriteDialog.exec()) {
      settings.setKeepBackupOnInstall(overwriteDialog.backup());

      if (overwriteDialog.backup()) {
        QString backupDirectory = generateBackupName(targetDirectory);
        if (!copyDir(targetDirectory, backupDirectory, false)) {
          reportError(tr("Failed to create backup"));
          return false;
        }
      }
      if (merge != nullptr) {
        *merge = (overwriteDialog.action() == QueryOverwriteDialog::ACT_MERGE);
      }
      if (overwriteDialog.action() == QueryOverwriteDialog::ACT_RENAME) {
        bool ok = false;
        QString name = QInputDialog::getText(m_ParentWidget, tr("Mod Name"), tr("Name"),
                                             QLineEdit::Normal, modName, &ok);
        if (ok && !name.isEmpty()) {
          modName.update(name, GUESS_USER);
          if (!ensureValidModName(modName)) {
            return false;
          }
          targetDirectory = QDir::fromNativeSeparators(m_ModsDirectory) + "/" + modName;
        }
      } else if (overwriteDialog.action() == QueryOverwriteDialog::ACT_REPLACE) {
        // save original settings like categories. Because it makes sense
        QString metaFilename = targetDirectory + "/meta.ini";
        QFile settingsFile(metaFilename);
        QByteArray originalSettings;
        if (settingsFile.open(QIODevice::ReadOnly)) {
          originalSettings = settingsFile.readAll();
          settingsFile.close();
        }

        // remove the directory with all content, then recreate it empty
        shellDelete(QStringList(targetDirectory));
        if (!QDir().mkdir(targetDirectory)) {
          // windows may keep the directory around for a moment, preventing its re-creation. Not sure
          // if this still happens with shellDelete
          Sleep(100);
          QDir().mkdir(targetDirectory);
        }
        // restore the saved settings
        if (settingsFile.open(QIODevice::WriteOnly)) {
          settingsFile.write(originalSettings);
          settingsFile.close();
        } else {
          log::error("failed to restore original settings: {}", metaFilename);
        }
        return true;
      } else if (overwriteDialog.action() == QueryOverwriteDialog::ACT_MERGE) {
        return true;
      }
    } else {
      return false;
    }
  }

  QDir().mkdir(targetDirectory);

  return true;
}


bool InstallationManager::ensureValidModName(GuessedValue<QString> &name) const
{
  while (name->isEmpty()) {
    bool ok;
    name.update(QInputDialog::getText(m_ParentWidget, tr("Invalid name"),
                                      tr("The name you entered is invalid, please enter a different one."),
                                      QLineEdit::Normal, "", &ok),
                GUESS_USER);
    if (!ok) {
      return false;
    }
  }
  return true;
}

IPluginInstaller::EInstallResult InstallationManager::doInstall(GuessedValue<QString> &modName, QString gameName, int modID,
                                    const QString &version, const QString &newestVersion,
                                    int categoryID, int fileCategoryID, const QString &repository)
{
  if (!ensureValidModName(modName)) {
    return IPluginInstaller::RESULT_FAILED;
  }

  bool merge = false;
  // determine target directory
  if (!testOverwrite(modName, &merge)) {
    return IPluginInstaller::RESULT_FAILED;
  }

  QString targetDirectory = QDir(m_ModsDirectory + "/" + modName).canonicalPath();
  QString targetDirectoryNative = QDir::toNativeSeparators(targetDirectory);

  log::debug("installing to \"{}\"", targetDirectoryNative);
  if (!extractFiles(targetDirectory, "", true, false)) {
    return IPluginInstaller::RESULT_CANCELED;
  }

  // Copy the created files:
  for (auto& p : m_CreatedFiles) {
    QString destPath = QDir::cleanPath(targetDirectory + QDir::separator() + p.first->path());
    log::debug("Moving {} to {}.", p.second, destPath);

    // We need to remove the path if it exists:
    if (QFile::exists(destPath)) {
      QFile::remove(destPath);
    }
    
    QDir dir = QFileInfo(destPath).absoluteDir();
    if (!dir.exists()) {
      dir.mkpath(".");
    }
    
    QFile::copy(p.second, destPath);
  }

  QSettings settingsFile(targetDirectory + "/meta.ini", QSettings::IniFormat);

  // overwrite settings only if they are actually are available or haven't been set before
  if ((gameName != "") || !settingsFile.contains("gameName")) {
    settingsFile.setValue("gameName", gameName);
  }
  if ((modID != 0) || !settingsFile.contains("modid")) {
    settingsFile.setValue("modid", modID);
  }
  if (!settingsFile.contains("version") ||
      (!version.isEmpty() &&
       (!merge || (VersionInfo(version) >= VersionInfo(settingsFile.value("version").toString()))))) {
    settingsFile.setValue("version", version);
  }
  if (!newestVersion.isEmpty() || !settingsFile.contains("newestVersion")) {
    settingsFile.setValue("newestVersion", newestVersion);
  }
  // issue #51 used to overwrite the manually set categories
  if (!settingsFile.contains("category")) {
    settingsFile.setValue("category", QString::number(categoryID));
  }
  settingsFile.setValue("nexusFileStatus", fileCategoryID);
  settingsFile.setValue("installationFile", m_CurrentFile);
  settingsFile.setValue("repository", repository);

  if (!m_URL.isEmpty() || !settingsFile.contains("url")) {
    settingsFile.setValue("url", m_URL);
  }

  //cleanup of m_URL or this will persist across installs.
  m_URL = "";

  if (!merge) {
    // this does not clear the list we have in memory but the mod is going to have to be re-read anyway
    // btw.: installedFiles were written with beginWriteArray but we can still clear it with beginGroup. nice
    settingsFile.beginGroup("installedFiles");
    settingsFile.remove("");
    settingsFile.endGroup();
  }

  return IPluginInstaller::RESULT_SUCCESS;
}


bool InstallationManager::wasCancelled() const
{
  return m_ArchiveHandler->getLastError() == Archive::Error::ERROR_EXTRACT_CANCELLED;
}

bool InstallationManager::isRunning() const
{
  return m_IsRunning;
}


void InstallationManager::postInstallCleanup()
{
  // Clear the list of created files:
  m_CreatedFiles.clear();

  // Close the archive:
  m_ArchiveHandler->close();

  // directories we may want to remove. sorted from longest to shortest to ensure we remove subdirectories first.
  auto longestFirst = [](const QString &LHS, const QString &RHS) -> bool {
                          if (LHS.size() != RHS.size()) return LHS.size() > RHS.size();
                          else return LHS < RHS;
                        };

  std::set<QString, std::function<bool(const QString&, const QString&)>> directoriesToRemove(longestFirst);

  // clean up temp files
  // TODO: this doesn't yet remove directories. Also, the files may be left there if this point isn't reached
  for (const QString &tempFile : m_TempFilesToDelete) {
    QFileInfo fileInfo(QDir::tempPath() + "/" + tempFile);
    if (fileInfo.exists()) {
      if (!fileInfo.isReadable() || !fileInfo.isWritable()) {
        QFile::setPermissions(fileInfo.absoluteFilePath(), QFile::ReadOther | QFile::WriteOther);
      }
      if (!QFile::remove(fileInfo.absoluteFilePath())) {
        log::warn("Unable to delete {}", fileInfo.absoluteFilePath());
      }
    }
    directoriesToRemove.insert(fileInfo.absolutePath());
  }

  m_TempFilesToDelete.clear();

  // try to delete each directory we had temporary files in. the call fails for non-empty directories which is ok
  for (const QString &dir : directoriesToRemove) {
    QDir().rmdir(dir);
  }
}

IPluginInstaller::EInstallResult InstallationManager::install(const QString &fileName,
                                  GuessedValue<QString> &modName,
                                  bool &hasIniTweaks,
                                  int modID)
{
  m_IsRunning = true;
  ON_BLOCK_EXIT([this]() { m_IsRunning = false; });

  QFileInfo fileInfo(fileName);
  if (m_SupportedExtensions.find(fileInfo.suffix()) == m_SupportedExtensions.end()) {
    reportError(tr("File format \"%1\" not supported").arg(fileInfo.suffix()));
    return IPluginInstaller::RESULT_FAILED;
  }

  modName.setFilter(&fixDirectoryName);

  modName.update(QFileInfo(fileName).completeBaseName(), GUESS_FALLBACK);

  // read out meta information from the download if available
  QString gameName = "";
  QString version = "";
  QString newestVersion = "";
  int categoryID = 0;
  int fileCategoryID = 1;
  QString repository = "Nexus";

  QString metaName = fileName + ".meta";
  if (QFile(metaName).exists()) {
    QSettings metaFile(metaName, QSettings::IniFormat);
    gameName = metaFile.value("gameName", "").toString();
    modID = metaFile.value("modID", 0).toInt();
    QTextDocument doc;
    doc.setHtml(metaFile.value("name", "").toString());
    modName.update(doc.toPlainText(), GUESS_FALLBACK);
    modName.update(metaFile.value("modName", "").toString(), GUESS_META);

    version = metaFile.value("version", "").toString();
    newestVersion = metaFile.value("newestVersion", "").toString();
    unsigned int categoryIndex = CategoryFactory::instance().resolveNexusID(
        metaFile.value("category", 0).toInt());
    categoryID = CategoryFactory::instance().getCategoryID(categoryIndex);
    repository = metaFile.value("repository", "").toString();
    fileCategoryID = metaFile.value("fileCategory", 1).toInt();
  }

  if (version.isEmpty()) {
    QDateTime lastMod = fileInfo.lastModified();
    version = "d" + lastMod.toString("yyyy.M.d");
  }

  { // guess the mod name and mod if from the file name if there was no meta information
    QString guessedModName;
    int guessedModID = modID;
    NexusInterface::interpretNexusFileName(QFileInfo(fileName).fileName(), guessedModName, guessedModID, false);
    if ((modID == 0) && (guessedModID != -1)) {
      modID = guessedModID;
    } else if (modID != guessedModID) {
      log::debug("passed mod id: {}, guessed id: {}", modID, guessedModID);
    }

    modName.update(guessedModName, GUESS_GOOD);
  }

  m_CurrentFile = fileInfo.absoluteFilePath();
  if (fileInfo.dir() == QDir(m_DownloadsDirectory)) {
    m_CurrentFile = fileInfo.fileName();
  }
  log::debug("using mod name \"{}\" (id {}) -> {}", QString(modName), modID, m_CurrentFile);

  //If there's an archive already open, close it. This happens with the bundle
  //installer when it uncompresses a split archive, then finds it has a real archive
  //to deal with.
  m_ArchiveHandler->close();

  // open the archive and construct the directory tree the installers work on

  bool archiveOpen = m_ArchiveHandler->open(
    fileName.toStdWString(), [this]() -> std::wstring {
      m_Password = QString();

      // Note: If we are not in the Qt event thread, we cannot use queryPassword() directly,
      // so we emit passwordRequested() that is connected to queryPassword(). The connection is
      // made using Qt::BlockingQueuedConnection, so the emit "call" is actually blocking. We
      // cannot use emit if we are in the even thread, otherwize we have a deadlock.
      if (QThread::currentThread() != QApplication::instance()->thread()) {
        emit passwordRequested();
      }
      else {
        queryPassword();
      }
      return m_Password.toStdWString();
    });
  if (!archiveOpen) {
    log::debug("integrated archiver can't open {}: {} ({})",
           fileName,
           getErrorString(m_ArchiveHandler->getLastError()),
           m_ArchiveHandler->getLastError());
  }
  ON_BLOCK_EXIT(std::bind(&InstallationManager::postInstallCleanup, this));

  std::shared_ptr<IFileTree> filesTree = 
    archiveOpen ? ArchiveFileTree::makeTree(*m_ArchiveHandler) : nullptr;
  IPluginInstaller::EInstallResult installResult = IPluginInstaller::RESULT_NOTATTEMPTED;

  std::sort(m_Installers.begin(), m_Installers.end(), [] (IPluginInstaller *LHS, IPluginInstaller *RHS) {
            return LHS->priority() > RHS->priority();
      });

  for (IPluginInstaller *installer : m_Installers) {
    // don't use inactive installers (installer can't be null here but vc static code analysis thinks it could)
    if ((installer == nullptr) || !installer->isActive()) {
      continue;
    }

    // try only manual installers if that was requested
    if (installResult == IPluginInstaller::RESULT_MANUALREQUESTED) {
      if (!installer->isManualInstaller()) {
        continue;
      }
    } else if (installResult != IPluginInstaller::RESULT_NOTATTEMPTED) {
      break;
    }

    try {
      { // simple case
        IPluginInstallerSimple *installerSimple
            = dynamic_cast<IPluginInstallerSimple *>(installer);
        if ((installerSimple != nullptr) && (filesTree != nullptr)
            && (installer->isArchiveSupported(filesTree))) {
          installResult
              = installerSimple->install(modName, filesTree, version, modID);
          if (installResult == IPluginInstaller::RESULT_SUCCESS) {

            // Downcast to an actual ArchiveFileTree and map to the archive. Test if
            // the tree is still an ArchiveFileTree, otherwize it means the installer
            // did some bad stuff.
            ArchiveFileTree* p = dynamic_cast<ArchiveFileTree*>(filesTree.get());
            if (p == nullptr) {
              throw IncompatibilityException(tr("Invalid file tree returned by plugin."));
            }
            
            // Detach the file tree (this ensure the parent is null and call to path()
            // stops at this root):
            p->detach();

            p->mapToArchive(*m_ArchiveHandler);

            // Clean the created files:
            cleanCreatedFiles(filesTree);

            // the simple installer only prepares the installation, the rest
            // works the same for all installers
            installResult = doInstall(modName, gameName, modID, version, newestVersion, categoryID, fileCategoryID, repository);
          }
        }
      }

      if (installResult != IPluginInstaller::RESULT_CANCELED) { // custom case
        IPluginInstallerCustom *installerCustom
            = dynamic_cast<IPluginInstallerCustom *>(installer);
        if ((installerCustom != nullptr)
            && (((filesTree != nullptr)
                 && installer->isArchiveSupported(filesTree))
                || ((filesTree == nullptr)
                    && installerCustom->isArchiveSupported(fileName)))) {
          std::set<QString> installerExt
              = installerCustom->supportedExtensions();
          if (installerExt.find(fileInfo.suffix()) != installerExt.end()) {
            installResult
                = installerCustom->install(modName, gameName, fileName, version, modID);
            unsigned int idx = ModInfo::getIndex(modName);
            if (idx != UINT_MAX) {
              ModInfo::Ptr info = ModInfo::getByIndex(idx);
              info->setRepository(repository);
            }
          }
        }
      }
    } catch (const IncompatibilityException &e) {
      log::error("plugin \"{}\" incompatible: {}", installer->name(), e.what());
    }

    // act upon the installation result. at this point the files have already been
    // extracted to the correct location
    switch (installResult) {
      case IPluginInstaller::RESULT_FAILED: {
        QMessageBox::information(qApp->activeWindow(), tr("Installation failed"),
          tr("Something went wrong while installing this mod."),
          QMessageBox::Ok);
        return installResult;
      } break;
      case IPluginInstaller::RESULT_SUCCESS:
      case IPluginInstaller::RESULT_SUCCESSCANCEL: {
        if (filesTree != nullptr) {
          auto iniTweakEntry = filesTree->find("INI Tweaks", FileTreeEntry::DIRECTORY);
          hasIniTweaks = iniTweakEntry != nullptr 
            && !iniTweakEntry->astree()->empty();
        }
        return IPluginInstaller::RESULT_SUCCESS;
      } break;
      case IPluginInstaller::RESULT_NOTATTEMPTED:
      case IPluginInstaller::RESULT_MANUALREQUESTED: {
        continue;
      }
      default:
        return installResult;
    }
  }
  if (installResult == IPluginInstaller::RESULT_NOTATTEMPTED) {
    reportError(tr("None of the available installer plugins were able to handle that archive.\n"
      "This is likely due to a corrupted or incompatible download or unrecognized archive format."));
  }

  return installResult;
}

QString InstallationManager::getErrorString(Archive::Error errorCode)
{
  switch (errorCode) {
    case Archive::Error::ERROR_NONE: {
      return tr("no error");
    } break;
    case Archive::Error::ERROR_LIBRARY_NOT_FOUND: {
      return tr("7z.dll not found");
    } break;
    case Archive::Error::ERROR_LIBRARY_INVALID: {
      return tr("7z.dll isn't valid");
    } break;
    case Archive::Error::ERROR_ARCHIVE_NOT_FOUND: {
      return tr("archive not found");
    } break;
    case Archive::Error::ERROR_FAILED_TO_OPEN_ARCHIVE: {
      return tr("failed to open archive");
    } break;
    case Archive::Error::ERROR_INVALID_ARCHIVE_FORMAT: {
      return tr("unsupported archive type");
    } break;
    case Archive::Error::ERROR_LIBRARY_ERROR: {
      return tr("internal library error");
    } break;
    case Archive::Error::ERROR_ARCHIVE_INVALID: {
      return tr("archive invalid");
    } break;
    default: {
      // this probably means the archiver.dll is newer than this
      return tr("unknown archive error");
    } break;
  }
}

void InstallationManager::registerInstaller(IPluginInstaller *installer)
{
  m_Installers.push_back(installer);
  installer->setInstallationManager(this);
  IPluginInstallerCustom *installerCustom = dynamic_cast<IPluginInstallerCustom*>(installer);
  if (installerCustom != nullptr) {
    std::set<QString> extensions = installerCustom->supportedExtensions();
    m_SupportedExtensions.insert(extensions.begin(), extensions.end());
  }
}

QStringList InstallationManager::getSupportedExtensions() const
{
  return QStringList(std::begin(m_SupportedExtensions), std::end(m_SupportedExtensions));
}
