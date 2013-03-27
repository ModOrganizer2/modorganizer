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

#include "installationmanager.h"

#include "utility.h"
#include "installdialog.h"
#include "simpleinstalldialog.h"
#include "baincomplexinstallerdialog.h"
#include "fomodinstallerdialog.h"
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
#include <scopeguard.h>
#include <installationtester.h>
#include <gameinfo.h>
#include <utility.h>
#include <QFileInfo>
#include <QLibrary>
#include <QInputDialog>
#include <QRegExp>
#include <QDir>
#include <QMessageBox>
#include <QSettings>
#include <Shellapi.h>
#include <QPushButton>
#include <QApplication>
#include <QDirIterator>
#include <boost/assign.hpp>


using namespace MOBase;
using namespace MOShared;


typedef Archive* (*CreateArchiveType)();



template <typename T> T resolveFunction(QLibrary &lib, const char *name)
{
  T temp = reinterpret_cast<T>(lib.resolve(name));
  if (temp == NULL) {
    throw std::runtime_error(QObject::tr("invalid 7-zip32.dll: %1").arg(lib.errorString()).toLatin1().constData());
  }
  return temp;
}


InstallationManager::InstallationManager(QWidget *parent)
  : QObject(parent), m_ParentWidget(parent),
    m_NCCPath(QApplication::applicationDirPath() + "/NCC/NexusClientCLI.exe"),
    m_InstallationProgress(parent), m_SupportedExtensions(boost::assign::list_of("zip")("rar")("7z")("fomod"))
{
  QLibrary archiveLib("dlls\\archive.dll");
  if (!archiveLib.load()) {
    throw MyException(tr("archive.dll not loaded: \"%1\"").arg(archiveLib.errorString()));
  }

  CreateArchiveType CreateArchiveFunc = resolveFunction<CreateArchiveType>(archiveLib, "CreateArchive");

  m_CurrentArchive = CreateArchiveFunc();
  if (!m_CurrentArchive->isValid()) {
    throw MyException(getErrorString(m_CurrentArchive->getLastError()));
  }

}


InstallationManager::~InstallationManager()
{
  delete m_CurrentArchive;
}


void InstallationManager::queryPassword(LPSTR password)
{
  QString result = QInputDialog::getText(NULL, tr("Password required"), tr("Password"), QLineEdit::Password);
  strncpy(password, result.toLocal8Bit().constData(), MAX_PASSWORD_LENGTH);
}


void InstallationManager::mapToArchive(const DirectoryTree::Node *node, std::wstring path, FileData * const *data)
{
  if (path.length() > 0) {
    path.append(L"\\");
  }

  for (DirectoryTree::const_leaf_iterator iter = node->leafsBegin(); iter != node->leafsEnd(); ++iter) {
    data[iter->getIndex()]->setSkip(false);
    data[iter->getIndex()]->setOutputFileName(path.substr().append(ToWString(iter->getName())).c_str());
  }

  for (DirectoryTree::const_node_iterator iter = node->nodesBegin(); iter != node->nodesEnd(); ++iter) {
    if ((*iter)->getData().index != -1) {
      data[(*iter)->getData().index]->setSkip(false);
      data[(*iter)->getData().index]->setOutputFileName(path.substr().append(ToWString((*iter)->getData().name)).c_str());
    }
    mapToArchive(*iter, path.substr().append(ToWString((*iter)->getData().name)), data);
  }
}


void InstallationManager::mapToArchive(const DirectoryTree::Node *baseNode)
{
  FileData* const *data;
  size_t size;
  m_CurrentArchive->getFileList(data, size);

  // first disable all files + folders, we will re-enable those present in baseNode
  for (size_t i = 0; i < size; ++i) {
    data[i]->setSkip(true);
  }

  std::wstring currentPath;

  mapToArchive(baseNode, currentPath, data);
}


bool InstallationManager::unpackPackageTXT()
{
  return unpackSingleFile("package.txt");
}


bool InstallationManager::unpackSingleFile(const QString &fileName)
{
  FileData* const *data;
  size_t size;
  m_CurrentArchive->getFileList(data, size);

  QString baseName = QFileInfo(fileName).fileName();

  bool available = false;
  for (size_t i = 0; i < size; ++i) {
    if (_wcsicmp(data[i]->getFileName(), ToWString(fileName).c_str()) == 0) {
      available = true;
      data[i]->setSkip(false);
      data[i]->setOutputFileName(ToWString(baseName).c_str());
      m_TempFilesToDelete.insert(baseName);
    } else {
      data[i]->setSkip(true);
    }
  }

  if (available) {
    m_InstallationProgress.setWindowTitle(tr("Extracting files"));
    m_InstallationProgress.setLabelText(QString());
    m_InstallationProgress.setValue(0);
    m_InstallationProgress.setWindowModality(Qt::WindowModal);
    m_InstallationProgress.show();

    bool res = m_CurrentArchive->extract(ToWString(QDir::toNativeSeparators(QDir::tempPath())).c_str(),
                                  new MethodCallback<InstallationManager, void, float>(this, &InstallationManager::updateProgress),
                                  new MethodCallback<InstallationManager, void, LPCWSTR>(this, &InstallationManager::dummyProgressFile),
                                  new MethodCallback<InstallationManager, void, LPCWSTR>(this, &InstallationManager::report7ZipError));

    m_InstallationProgress.hide();

    return res;
  } else {
    return false;
  }
}


QString InstallationManager::extractFile(const QString &fileName)
{
  if (unpackSingleFile(fileName)) {
    QString tempFileName = QDir::tempPath().append("/").append(QFileInfo(fileName).fileName());

    m_FilesToDelete.insert(tempFileName);

    return tempFileName;
  } else {
    return QString();
  }
}


QString canonicalize(const QString &name)
{
  QString result(name);
  if ((result.startsWith('/')) ||
      (result.startsWith('\\'))) {
    result.remove(0, 1);
  }
  result.replace('/', '\\');

  return result;
}


QStringList InstallationManager::extractFiles(const QStringList &filesOrig)
{
  QStringList files;

  foreach (const QString &file, filesOrig) {
    files.append(canonicalize(file));
  }

  QStringList result;

  FileData* const *data;
  size_t size;
  m_CurrentArchive->getFileList(data, size);

  for (size_t i = 0; i < size; ++i) {
    if (files.contains(ToQString(data[i]->getFileName()), Qt::CaseInsensitive)) {
      const wchar_t *baseName = wcsrchr(data[i]->getFileName(), '\\');
      if (baseName == NULL) {
        baseName = wcsrchr(data[i]->getFileName(), '/');
      }
      if (baseName == NULL) {
        qCritical("failed to find backslash in %ls", data[i]->getFileName());
        continue;
      }
      data[i]->setOutputFileName(baseName);

      result.append(QDir::tempPath().append("/").append(ToQString(baseName)));

      data[i]->setSkip(false);
      m_TempFilesToDelete.insert(ToQString(baseName));
    } else {
      data[i]->setSkip(true);
    }
  }

  m_InstallationProgress.setWindowTitle(tr("Extracting files"));
  m_InstallationProgress.setLabelText(QString());
  m_InstallationProgress.setValue(0);
  m_InstallationProgress.setWindowModality(Qt::WindowModal);
  m_InstallationProgress.show();

  // unpack only the files we need for the installer
  if (!m_CurrentArchive->extract(ToWString(QDir::toNativeSeparators(QDir::tempPath())).c_str(),
         new MethodCallback<InstallationManager, void, float>(this, &InstallationManager::updateProgress),
         new MethodCallback<InstallationManager, void, LPCWSTR>(this, &InstallationManager::dummyProgressFile),
         new MethodCallback<InstallationManager, void, LPCWSTR>(this, &InstallationManager::report7ZipError))) {
    throw std::runtime_error("extracting failed");
  }

  m_InstallationProgress.hide();
  return result;
}

IPluginInstaller::EInstallResult InstallationManager::installArchive(const QString &modName, const QString &archiveName)
{
  QString temp = modName;
  bool iniTweaks;
  if (install(archiveName, "", "modsdir", false, true, temp, iniTweaks)) {
    return IPluginInstaller::RESULT_SUCCESS;
  } else {
    return IPluginInstaller::RESULT_FAILED;
  }
}


DirectoryTree *InstallationManager::createFilesTree()
{
  FileData* const *data;
  size_t size;
  m_CurrentArchive->getFileList(data, size);

  QScopedPointer<DirectoryTree> result(new DirectoryTree);

  for (size_t i = 0; i < size; ++i) {
    // the files are in a flat list where each file has a a full path relative to the archive root
    // to create a tree, we have to iterate over each path component of each. This could be sped up by
    // grouping the filenames first, but so far there doesn't seem to be an actual performance problem
    DirectoryTree::Node *currentNode = result.data();

    QString fileName = ToQString(data[i]->getFileName());
    QStringList components = fileName.split("\\");

    // iterate over all path-components of this filename (including the filename itself)
    for (QStringList::iterator componentIter = components.begin(); componentIter != components.end(); ++componentIter) {
      if (componentIter->size() == 0) {
        // empty string indicates fileName is actually only a directory name and we have
        // completely processed it already.
        break;
      }

      bool exists = false;
      // test if this path is already in the tree
      for (DirectoryTree::node_iterator nodeIter = currentNode->nodesBegin(); nodeIter != currentNode->nodesEnd(); ++nodeIter) {
        if ((*nodeIter)->getData().name == *componentIter) {
          currentNode = *nodeIter;
          exists = true;
          break;
        }
      }

      if (!exists) {
        if (componentIter + 1 == components.end()) {
          // last path component. directory or file?
          if (data[i]->isDirectory()) {
            // this is a bit problematic. archives will often only list directories if they are empty,
            // otherwise the dir only appears in the path of a file. In the UI however we allow the user
            // to uncheck all files in a directory while keeping the dir checked. Those directories are
            // currently not installed.
            DirectoryTree::Node *newNode = new DirectoryTree::Node;
            newNode->setData(DirectoryTreeInformation(*componentIter, i));
            currentNode->addNode(newNode, false);
            currentNode = newNode;
          } else {
            currentNode->addLeaf(FileTreeInformation(*componentIter, i));
          }
        } else {
          DirectoryTree::Node *newNode = new DirectoryTree::Node;
          newNode->setData(DirectoryTreeInformation(*componentIter, -1));
          currentNode->addNode(newNode, false);
          currentNode = newNode;
        }
      }
    }
  }

  return result.take();
}


bool InstallationManager::isSimpleArchiveTopLayer(const DirectoryTree::Node *node, bool bainStyle)
{
  // see if there is at least one directory that makes sense on the top level
  for (DirectoryTree::const_node_iterator iter = node->nodesBegin(); iter != node->nodesEnd(); ++iter) {
    if ((bainStyle && InstallationTester::isTopLevelDirectoryBain((*iter)->getData().name)) ||
        (!bainStyle && InstallationTester::isTopLevelDirectory((*iter)->getData().name))) {
      qDebug("%s on the top level", (*iter)->getData().name.toUtf8().constData());
      return true;
    }
  }

  // see if there is a file that makes sense on the top level
  for (DirectoryTree::const_leaf_iterator iter = node->leafsBegin(); iter != node->leafsEnd(); ++iter) {
    if (InstallationTester::isTopLevelSuffix(iter->getName())) {
      return true;
    }
  }
  return false;
}


DirectoryTree::Node *InstallationManager::getSimpleArchiveBase(DirectoryTree *dataTree)
{
  DirectoryTree::Node *currentNode = dataTree;

  while (true) {
    if (isSimpleArchiveTopLayer(currentNode, false)) {
      return currentNode;
    } else if ((currentNode->numLeafs() == 0) &&
               (currentNode->numNodes() == 1)) {
      currentNode = *currentNode->nodesBegin();
    } else {
      qDebug("not a simple archive");
      return NULL;
    }
  }
}


bool InstallationManager::checkBainPackage(DirectoryTree *dataTree)
{
  int numDirs = dataTree->numNodes();
  // each directory would have to serve as a top-level directory
  for (DirectoryTree::const_node_iterator iter = dataTree->nodesBegin();
       iter != dataTree->nodesEnd(); ++iter) {
    const QString &dirName = (*iter)->getData().name;
    if ((dirName.compare("fomod", Qt::CaseInsensitive) == 0) ||
        (dirName.startsWith("--"))) {
      --numDirs;
      continue;
    }
    if (!isSimpleArchiveTopLayer(*iter, true)) {
      qDebug("%s is not a top layer directory", (*iter)->getData().name.toUtf8().constData());
      return false;
    }
  }

  if (numDirs < 2) {
    // a complex bain package contains at least 2 directories to choose from
    qDebug("only %d dirs", numDirs);
    return false;
  }

  return true;
}


bool InstallationManager::checkNMMInstaller()
{
  return QFile::exists(m_NCCPath);
}


bool InstallationManager::checkFomodPackage(DirectoryTree *dataTree, QString &offset, bool &xmlInstaller)
{
  bool scriptInstaller = false;
  xmlInstaller = false;
  for (DirectoryTree::const_node_iterator iter = dataTree->nodesBegin();
       iter != dataTree->nodesEnd(); ++iter) {
    const QString &dirName = (*iter)->getData().name;
    if (dirName.compare("fomod", Qt::CaseInsensitive) == 0) {
      for (DirectoryTree::const_leaf_iterator fileIter = (*iter)->leafsBegin();
           fileIter != (*iter)->leafsEnd(); ++fileIter) {
        if (fileIter->getName().compare("ModuleConfig.xml", Qt::CaseInsensitive) == 0) {
          xmlInstaller = true;
        } else if (fileIter->getName().compare("script.cs", Qt::CaseInsensitive) == 0) {
          scriptInstaller = true;
        }
      }
      break;
    }
  }
  if (!xmlInstaller && !scriptInstaller && (dataTree->numNodes() == 1) && (dataTree->numLeafs() == 0)) {
    DirectoryTree::Node *node = *dataTree->nodesBegin();
    offset = node->getData().name;
    return checkFomodPackage(node, offset, xmlInstaller);
  }

  return (xmlInstaller || scriptInstaller);
}


void InstallationManager::updateProgress(float percentage)
{
  m_InstallationProgress.setValue(static_cast<int>(percentage * 100.0));
  if (m_InstallationProgress.wasCanceled()) {
    m_CurrentArchive->cancel();
    m_InstallationProgress.reset();
  }
}


void InstallationManager::updateProgressFile(LPCWSTR fileName)
{
  m_InstallationProgress.setLabelText(QString::fromUtf16(fileName));
}


void InstallationManager::report7ZipError(LPCWSTR errorMessage)
{
  reportError(QString::fromUtf16(errorMessage));
}


QString InstallationManager::generateBackupName(const QString &directoryName)
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


bool InstallationManager::testOverwrite(const QString &modsDirectory, QString &modName)
{
  QString targetDirectory = QDir::fromNativeSeparators(modsDirectory.mid(0).append("\\").append(modName));

  while (QDir(targetDirectory).exists()) {
    QueryOverwriteDialog overwriteDialog(m_ParentWidget);
    if (overwriteDialog.exec()) {
      if (overwriteDialog.backup()) {
        QString backupDirectory = generateBackupName(targetDirectory);
        if (!copyDir(targetDirectory, backupDirectory, false)) {
          reportError(tr("failed to create backup"));
          return false;
        }
      }
      if (overwriteDialog.action() == QueryOverwriteDialog::ACT_RENAME) {
        bool ok = false;
        QString name = QInputDialog::getText(m_ParentWidget, tr("Mod Name"), tr("Name"),
                                             QLineEdit::Normal, modName, &ok);
        if (ok && !name.isEmpty()) {
          modName = name;
          if (!ensureValidModName(modName)) {
            return false;
          }
          targetDirectory = QDir::fromNativeSeparators(modsDirectory.mid(0).append("\\").append(modName));
        }
      } else if (overwriteDialog.action() == QueryOverwriteDialog::ACT_REPLACE) {
        // save original settings like categories. Because it makes sense
        QString metaFilename = targetDirectory.mid(0).append("/meta.ini");
        QFile settingsFile(metaFilename);
        QByteArray originalSettings;
        if (settingsFile.open(QIODevice::ReadOnly)) {
          originalSettings = settingsFile.readAll();
          settingsFile.close();
        }

        // remove the directory with all content, then recreate it empty
        shellDelete(QStringList(targetDirectory), NULL);
        if (!QDir().mkdir(targetDirectory)) {
          // windows may keep the directory around for a moment, preventing its re-creation
          Sleep(100);
          QDir().mkdir(targetDirectory);
        }
        // restore the saved settings
        if (settingsFile.open(QIODevice::WriteOnly)) {
          settingsFile.write(originalSettings);
          settingsFile.close();
        } else {
          qCritical("failed to restore original settings: %s", metaFilename.toUtf8().constData());
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


void InstallationManager::fixModName(QString &name)
{
//  name = name.remove("^[ ]*").trimmed();
  name = name.simplified();
  while (name.endsWith('.')) name.chop(1);

  name.replace(QRegExp("[<>:\"/\\|?*]"), "");

  static QString invalidNames[] = { "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
                                    "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" };
  for (int i = 0; i < sizeof(invalidNames) / sizeof(QString); ++i) {
    if (name == invalidNames[i]) {
      name = "";
      break;
    }
  }
}


bool InstallationManager::ensureValidModName(QString &name)
{
  fixModName(name);

  while (name.isEmpty()) {
    bool ok;
    name = QInputDialog::getText(m_ParentWidget, tr("Invalid name"),
                                         tr("The name you entered is invalid, please enter a different one."),
                                         QLineEdit::Normal, "", &ok);
    if (!ok) {
      return false;
    }
    fixModName(name);
  }
  return true;
}


bool InstallationManager::doInstall(const QString &modsDirectory, QString &modName, int modID,
                                    const QString &version, const QString &newestVersion, int categoryID)
{
  if (!ensureValidModName(modName)) {
    return false;
  }

  // determine target directory
  if (!testOverwrite(modsDirectory, modName)) {
    return false;
  }

  QString targetDirectory = QDir::fromNativeSeparators(modsDirectory.mid(0).append("\\").append(modName));

  qDebug("installing to \"%s\"", targetDirectory.toUtf8().constData());

  m_InstallationProgress.setWindowTitle(tr("Extracting files"));
  m_InstallationProgress.setLabelText(QString());
  m_InstallationProgress.setValue(0);
  m_InstallationProgress.setWindowModality(Qt::WindowModal);
  m_InstallationProgress.show();

  if (!m_CurrentArchive->extract(ToWString(QDir::toNativeSeparators(targetDirectory)).c_str(),
         new MethodCallback<InstallationManager, void, float>(this, &InstallationManager::updateProgress),
         new MethodCallback<InstallationManager, void, LPCWSTR>(this, &InstallationManager::updateProgressFile),
         new MethodCallback<InstallationManager, void, LPCWSTR>(this, &InstallationManager::report7ZipError))) {
    if (m_CurrentArchive->getLastError() == Archive::ERROR_EXTRACT_CANCELLED) {
      return false;
    } else {
      throw std::runtime_error("extracting failed");
    }
  }

  m_InstallationProgress.hide();

  QSettings settingsFile(targetDirectory.mid(0).append("/meta.ini"), QSettings::IniFormat);

  // overwrite settings only if they are actually are available or haven't been set before
  if ((modID != 0) || !settingsFile.contains("modid")) {
    settingsFile.setValue("modid", modID);
  }
  if (!settingsFile.contains("version") ||
      (!version.isEmpty() &&
       (VersionInfo(version) >= VersionInfo(settingsFile.value("version").toString())))) {
    settingsFile.setValue("version", version);
  }
  if (!newestVersion.isEmpty() || !settingsFile.contains("newestVersion")) {
    settingsFile.setValue("newestVersion", newestVersion);
  }
  // issue #51 used to overwrite the manually set categories
  if (!settingsFile.contains("category")) {
    settingsFile.setValue("category", QString::number(categoryID));
  }
  settingsFile.setValue("installationFile", m_CurrentFile);

  return true;
}


void InstallationManager::openFile(const QString &fileName)
{
  unpackSingleFile(fileName);

  QString tempFileName = QDir::tempPath().append("/").append(QFileInfo(fileName).fileName());

  SHELLEXECUTEINFOW execInfo;
  memset(&execInfo, 0, sizeof(SHELLEXECUTEINFOW));
  execInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
  execInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
  execInfo.lpVerb = L"open";
  std::wstring fileNameW = ToWString(tempFileName);
  execInfo.lpFile = fileNameW.c_str();
  execInfo.nShow = SW_SHOWNORMAL;
  if (!::ShellExecuteExW(&execInfo)) {
    qCritical("failed to spawn %s: %d", tempFileName.toUtf8().constData(), ::GetLastError());
  }

  m_FilesToDelete.insert(tempFileName);
}


// copy and pasted from mo_dll
bool EndsWith(LPCWSTR string, LPCWSTR subString)
{
  size_t slen = wcslen(string);
  size_t len = wcslen(subString);
  if (slen < len) {
    return false;
  }

  for (size_t i = 0; i < len; ++i) {
    if (towlower(string[slen - len + i]) != towlower(subString[i])) {
      return false;
    }
  }
  return true;
}


bool InstallationManager::installFomodInternal(DirectoryTree *&baseNode, const QString &fomodPath, const QString &modsDirectory,
                                               int modID, const QString &version, const QString &newestVersion, int categoryID,
                                               QString &modName, bool nameGuessed, bool &manualRequest)
{
  qDebug("treating as fomod archive");

  FileData* const *data;
  size_t size;
  m_CurrentArchive->getFileList(data, size);
  wchar_t *installerFiles[] = { L"fomod\\info.xml", L"fomod\\ModuleConfig.xml",
                                L"fomod\\script.cs", L"fomod\\screenshot.png", NULL };
  for (size_t i = 0; i < size; ++i) {
    data[i]->setSkip(true);
    if (data[i]->getFileName() == NULL) {
      qCritical("invalid archive file name");
    }
    for (int fileIdx = 0; installerFiles[fileIdx] != NULL; ++fileIdx) {
      if (EndsWith(data[i]->getFileName(), installerFiles[fileIdx])) {
        wchar_t *baseName = wcsrchr(installerFiles[fileIdx], '\\');
        if (baseName != NULL) {
          data[i]->setSkip(false);
          data[i]->setOutputFileName(baseName);
          m_TempFilesToDelete.insert(ToQString(baseName));
        } else {
          qCritical("failed to find backslash in %ls", installerFiles[fileIdx]);
        }
        break;
      }
    }
    if (EndsWith(data[i]->getFileName(), L".png") ||
        EndsWith(data[i]->getFileName(), L".jpg") ||
        EndsWith(data[i]->getFileName(), L".gif") ||
        EndsWith(data[i]->getFileName(), L".bmp")) {
      const wchar_t *baseName = wcsrchr(data[i]->getFileName(), '\\');
      if (baseName == NULL) {
        baseName = data[i]->getFileName();
      } else {
        ++baseName;
      }
      data[i]->setSkip(false);
      data[i]->setOutputFileName(baseName);
      m_TempFilesToDelete.insert(ToQString(baseName));
    }
  }

  m_InstallationProgress.setWindowTitle(tr("Preparing installer"));
  m_InstallationProgress.setLabelText(QString());
  m_InstallationProgress.setValue(0);
  m_InstallationProgress.setWindowModality(Qt::WindowModal);
  m_InstallationProgress.show();

  // unpack only the files we need for the installer
  if (!m_CurrentArchive->extract(ToWString(QDir::toNativeSeparators(QDir::tempPath())).c_str(),
         new MethodCallback<InstallationManager, void, float>(this, &InstallationManager::updateProgress),
         new MethodCallback<InstallationManager, void, LPCWSTR>(this, &InstallationManager::dummyProgressFile),
         new MethodCallback<InstallationManager, void, LPCWSTR>(this, &InstallationManager::report7ZipError))) {
    throw std::runtime_error("extracting failed");
  }

  m_InstallationProgress.hide();

  bool success = false;
  try {
    FomodInstallerDialog dialog(modName, nameGuessed, fomodPath);

    FileData* const *data;
    size_t size;
    m_CurrentArchive->getFileList(data, size);

    // the installer will want to unpack screenshots...
    for (size_t i = 0; i < size; ++i) {
      data[i]->setSkip(true);
    }

    dialog.initData();
    if (dialog.exec() == QDialog::Accepted) {
      modName = dialog.getName();
      baseNode = dialog.updateTree(baseNode);
      mapToArchive(baseNode);

      if (doInstall(modsDirectory, modName, modID, version, newestVersion, categoryID)) {
        success = true;
      }
    } else {
      if (dialog.manualRequested()) {
        manualRequest = true;
        modName = dialog.getName();
      }
    }
  } catch (const std::exception &e) {
    reportError(tr("Installation as fomod failed: %1").arg(e.what()));
    manualRequest = true;
  }
  return success;
}


static BOOL CALLBACK BringToFront(HWND hwnd, LPARAM lParam)
{
  DWORD procid;

  GetWindowThreadProcessId(hwnd, &procid);

  if (procid == static_cast<DWORD>(lParam)) {
    ::SetForegroundWindow(hwnd);
    ::SetLastError(NOERROR);
    return FALSE;
  } else {
    return TRUE;
  }
}


bool InstallationManager::installFomodExternal(const QString &fileName, const QString &pluginsFileName, const QString &modDirectory)
{
  wchar_t binary[MAX_PATH];
  wchar_t parameters[1024]; // maximum: 2xMAX_PATH + approx 20 characters
  wchar_t currentDirectory[MAX_PATH];

  _snwprintf(binary, MAX_PATH, L"%ls", ToWString(QDir::toNativeSeparators(m_NCCPath)).c_str());
  _snwprintf(parameters, 1024, L"-g %ls -p \"%ls\" -i \"%ls\" \"%ls\"",
             GameInfo::instance().getGameShortName().c_str(),
             ToWString(QDir::toNativeSeparators(pluginsFileName)).c_str(),
             ToWString(QDir::toNativeSeparators(fileName)).c_str(),
             ToWString(QDir::toNativeSeparators(modDirectory)).c_str());
  _snwprintf(currentDirectory, MAX_PATH, L"%ls", ToWString(QFileInfo(m_NCCPath).absolutePath()).c_str());

  GameInfo &gameInfo = GameInfo::instance();

  QStringList copiedFiles;
  QStringList patterns;
  patterns.append(QDir::fromNativeSeparators(ToQString(gameInfo.getBinaryName())));
  patterns.append("*se_loader.exe");
  QDirIterator iter(QDir::fromNativeSeparators(ToQString(gameInfo.getGameDirectory())), patterns);
  QDir modDir(modDirectory);
  while (iter.hasNext()) {
    iter.next();
    QString destination = modDir.absoluteFilePath(iter.fileInfo().fileName());
    if (QFile::copy(iter.fileInfo().absoluteFilePath(), destination)) {
      copiedFiles.append(destination);
    }
  }
  ON_BLOCK_EXIT([&copiedFiles] {
    foreach (const QString &fileName, copiedFiles) {
      if (!QFile::remove(fileName)) qCritical("failed to remove %s", qPrintable(fileName));
    }
  });

  SHELLEXECUTEINFOW execInfo = {0};
  execInfo.cbSize = sizeof(SHELLEXECUTEINFOW);
  execInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
  execInfo.hwnd = NULL;
  execInfo.lpVerb = L"open";
  execInfo.lpFile = binary;
  execInfo.lpParameters = parameters;
  execInfo.lpDirectory = currentDirectory;
  execInfo.nShow = SW_SHOW;

  if (!::ShellExecuteExW(&execInfo)) {
    reportError(tr("failed to start %1").arg(m_NCCPath));
    return false;
  }

  QProgressDialog busyDialog(tr("Preparing external installer, this can take a few minutes.\nNote: This installer will not be aware of other installed mods (including skse)!"), tr("Force Close"), 0, 0, m_ParentWidget);
  busyDialog.setWindowModality(Qt::WindowModal);
  bool confirmCancel = false;
  busyDialog.show();
  bool finished = false;
  DWORD procid = ::GetProcessId(execInfo.hProcess);
  bool inFront = false;
  while (true) {
    QCoreApplication::processEvents();
    if (!inFront) {
      if (!::EnumWindows(BringToFront, procid) && (::GetLastError() == NOERROR)) {
        inFront = true;
      }
    }
    DWORD res = ::WaitForSingleObject(execInfo.hProcess, 100);
    if (res == WAIT_OBJECT_0) {
      finished = true;
      break;
    } else if ((busyDialog.wasCanceled()) || (res != WAIT_TIMEOUT)) {
      if (!confirmCancel) {
        confirmCancel = true;
        busyDialog.hide();
        busyDialog.reset();
        busyDialog.show();
        busyDialog.setCancelButtonText(tr("Confirm"));
      } else {
        break;
      }
    }
  }

  if (!finished) {
    ::TerminateProcess(execInfo.hProcess, 1);
    return false;
  }

  DWORD exitCode = 128;
  ::GetExitCodeProcess(execInfo.hProcess, &exitCode);

  ::CloseHandle(execInfo.hProcess);

  if ((exitCode == 0) || (exitCode == 10)) { // 0 = success, 10 = incomplete installation
    bool errorOccured = false;
    { // move all installed files from the data directory one directory up
      QDir targetDir(modDirectory);

      QDirIterator dirIter(targetDir.absoluteFilePath("Data"), QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
      bool hasFiles = false;

      while (dirIter.hasNext()) {
        dirIter.next();
        QFileInfo fileInfo = dirIter.fileInfo();
        QString newName = targetDir.absoluteFilePath(fileInfo.fileName());
        if (fileInfo.isFile() && QFile::exists(newName)) {
          if (!QFile::remove(newName)) {
            qCritical("failed to overwrite %s", qPrintable(newName));
            errorOccured = true;
          }
        } // if it's a directory and the target exists that isn't really a problem
        // TODO: use shellRename?
        if (!QFile::rename(fileInfo.absoluteFilePath(), newName)) {
          // moving doesn't work when merging
          if (!copyDir(fileInfo.absoluteFilePath(), newName, true)) {
            qCritical("failed to move %s to %s", qPrintable(fileInfo.absoluteFilePath()), qPrintable(newName));
            errorOccured = true;
          }
        }
        hasFiles = true;
      }
      // recognition of canceled installation in the external installer is broken so we assume the installation was
      // canceled if no files were installed
      if (!hasFiles) {
        exitCode = 11;
      }
    }

    QString dataDir = modDirectory.mid(0).append("/Data");
    if (!shellDelete(QStringList(dataDir), NULL)) {
      qCritical("failed to remove data directory from %s", dataDir.toUtf8().constData());
      errorOccured = true;
    }
    if (errorOccured) {
      reportError(tr("Finalization of the installation failed. The mod may or may not work correctly. See mo_interface.log for details"));
    }
  } else if (exitCode != 11) { // 11 = manually canceled
    reportError(tr("installation failed (errorcode %1)").arg(exitCode));
  }

  if ((exitCode == 0) || (exitCode == 10)) {
    return true;
  } else {
    // after cancelation or error the installer may leave the empty mod directory
    if (!shellDelete(QStringList(modDirectory), NULL)) {
      qCritical ("failed to remove empty mod directory %s", modDirectory.toUtf8().constData());
    }
    return false;
  }
}


bool InstallationManager::wasCancelled()
{
  return m_CurrentArchive->getLastError() == Archive::ERROR_EXTRACT_CANCELLED;
}


bool InstallationManager::install(const QString &fileName, const QString &pluginsFileName, const QString &modsDirectory,
                                  bool preferIntegrated, bool enableQuickInstall, QString &modName, bool &hasIniTweaks)
{
  QFileInfo fileInfo(fileName);
  bool success = false;
  if (m_SupportedExtensions.find(fileInfo.suffix()) == m_SupportedExtensions.end()) {
    reportError(tr("File format \"%1\" not supported").arg(fileInfo.completeSuffix()));
    return false;
  }

  // read out meta information from the download if available
  int modID = 0;
  QString version = "";
  QString newestVersion = "";
  int categoryID = 0;
  bool nameGuessed = false;

  QString metaName = fileName.mid(0).append(".meta");
  if (QFile(metaName).exists()) {
    QSettings metaFile(metaName, QSettings::IniFormat);
    modID = metaFile.value("modID", 0).toInt();
    if (modName.isEmpty()) {
      modName = metaFile.value("modName", "").toString();
      // it is possible we have a file-name but not the correct mod name. in this case,
      // the stored mod name may be "\0"
      if (modName.isEmpty() || (modName.length() < 2)) {
        modName = metaFile.value("name", "").toString();
      }
    }
    version = metaFile.value("version", "").toString();
    newestVersion = metaFile.value("newestVersion", "").toString();
    unsigned int categoryIndex = CategoryFactory::instance().resolveNexusID(metaFile.value("category", 0).toInt());
    categoryID = CategoryFactory::instance().getCategoryID(categoryIndex);
  }

  { // guess the mod name and mod if from the file name if there was no meta information
    QString guessedModName;
    int guessedModID = modID;
    NexusInterface::interpretNexusFileName(QFileInfo(fileName).baseName(), guessedModName, guessedModID, false);
    if ((modID == 0) && (guessedModID != -1)) {
      modID = guessedModID;
    } else if (modID != guessedModID) {
      qDebug("passed mod id: %d, guessed id: %d", modID, guessedModID);
    }

    if (modName.isEmpty()) {
      modName = guessedModName;
      nameGuessed = true;
    }
  }
  fixModName(modName);

  qDebug("using mod name \"%s\" (id %d)", modName.toUtf8().constData(), modID);
  m_CurrentFile = fileInfo.fileName();

  // open the archive and construct the directory tree the installers work on
  bool archiveOpen = m_CurrentArchive->open(ToWString(QDir::toNativeSeparators(fileName)).c_str(),
                                            new MethodCallback<InstallationManager, void, LPSTR>(this, &InstallationManager::queryPassword));

  DirectoryTree *filesTree = archiveOpen ? createFilesTree() : NULL;

/*  IPluginInstaller::EInstallResult installResult = IPluginInstaller::RESULT_NOTATTEMPTED;

  std::sort(m_Installers.begin(), m_Installers.end(), [] (IPluginInstaller *LHS, IPluginInstaller *RHS) {
            return LHS->priority() > RHS->priority();
      });

  foreach (IPluginInstaller *installer, m_Installers) {
    // don't use inactive installers
    if (!installer->isActive()) {
      continue;
    }

    // try only manual installers if that was requested
    if ((installResult == IPluginInstaller::RESULT_MANUALREQUESTED) && !installer->isManualInstaller()) {
      continue;
    }

    try {
      { // simple case
        IPluginInstallerSimple *installerSimple = dynamic_cast<IPluginInstallerSimple*>(installer);

        if ((installerSimple != NULL) &&
            (filesTree != NULL) && (installer->isArchiveSupported(*filesTree))) {
          installResult = installerSimple->install(modName, *filesTree);
          if (installResult == IPluginInstaller::RESULT_SUCCESS) {
            mapToArchive(filesTree);
            if (!doInstall(modsDirectory, modName, modID, version, newestVersion, categoryID)) {
              installResult = IPluginInstaller::RESULT_FAILED;
            }
          }
        }
      }

      { // custom case
        IPluginInstallerCustom *installerCustom = dynamic_cast<IPluginInstallerCustom*>(installer);
        if ((installerCustom != NULL) &&
            (((filesTree != NULL) && installer->isArchiveSupported(*filesTree)) ||
             ((filesTree == NULL) && installerCustom->isArchiveSupported(fileName)))) {
          std::set<QString> installerExtensions = installerCustom->supportedExtensions();
          if (installerExtensions.find(fileInfo.suffix()) != installerExtensions.end()) {
            installResult = installerCustom->install(modName, fileName);
          }
        }
      }
    } catch (const IncompatibilityException &e) {
      qCritical("plugin \"%s\" incompatible: %s",
                qPrintable(installer->name()), e.what());
    }

    // act upon the installation result. at this point the files have already been
    // extracted to the correct location
    switch (installResult) {
      case IPluginInstaller::RESULT_CANCELED:
      case IPluginInstaller::RESULT_FAILED: {
        return false;
      } break;
      case IPluginInstaller::RESULT_SUCCESS: {
        DirectoryTree::node_iterator iniTweakNode = filesTree->nodeFind(DirectoryTreeInformation("INI Tweaks"));
        hasIniTweaks = (iniTweakNode != filesTree->nodesEnd()) &&
                       ((*iniTweakNode)->numLeafs() != 0);
        return true;
      } break;
    }
  }

  reportError(tr("None of the available installer plugins were able to handle that archive"));
  return false;*/



  hasIniTweaks = false;


  DirectoryTree::Node *baseNode = NULL;
  bool manualRequest = false;

  if (!archiveOpen) {
    reportError(tr("Failed to open \"%1\": %2").arg(QDir::toNativeSeparators(fileName)).arg(getErrorString(m_CurrentArchive->getLastError())));
    return false;
  }

  // bundled fomod?
  if ((baseNode == NULL) && !manualRequest) {
    QStringList bundledFomods;
    for (DirectoryTree::const_leaf_iterator fileIter = filesTree->leafsBegin(); fileIter != filesTree->leafsEnd(); ++fileIter) {
      if (fileIter->getName().endsWith(".fomod", Qt::CaseInsensitive)) {
        bundledFomods.append(fileIter->getName());
      }
    }
    QString bundledFomodInst;
    if (bundledFomods.count() > 1) {
      SelectionDialog selection(tr("This seems like a bundle of fomods, which one do you want to install?"), m_ParentWidget);
      foreach (const QString &fomod, bundledFomods) {
        selection.addChoice(fomod, fomod, QVariant());
      }
      if (selection.exec() == QDialog::Accepted) {
        bundledFomodInst = selection.getChoiceString();
      } else {
        return false;
      }
    } else if (bundledFomods.count() == 1) {
      bundledFomodInst = bundledFomods.at(0);
      qDebug("archive contains fomod: %s", qPrintable(bundledFomodInst));
    }
    if (!bundledFomodInst.isEmpty()) {
      unpackSingleFile(bundledFomodInst);
      m_CurrentArchive->close();
      return install(QDir::tempPath().append("/").append(bundledFomodInst), pluginsFileName, modsDirectory, preferIntegrated,
                     enableQuickInstall, modName, hasIniTweaks);
    }
  }

  // fomod installer?
  if ((baseNode == NULL) && !manualRequest) {
    QString fomodPath;
    bool xmlInstaller = false;
    if (checkFomodPackage(filesTree, fomodPath, xmlInstaller)) {
      baseNode = filesTree;
      bool nmmInstaller = checkNMMInstaller();

      if (xmlInstaller || nmmInstaller) {
        if (!xmlInstaller || (nmmInstaller && !preferIntegrated)) {
          if (!ensureValidModName(modName) ||
              !testOverwrite(modsDirectory, modName)) {
            return false;
          }

          QString targetDirectory = QDir::fromNativeSeparators(modsDirectory.mid(0).append("\\").append(modName));

          if (installFomodExternal(fileName, pluginsFileName, targetDirectory)) {
            QSettings settingsFile(targetDirectory.mid(0).append("/meta.ini"), QSettings::IniFormat);

            // overwrite settings only if they are actually are available or haven't been set before
            if ((modID != 0) || !settingsFile.contains("modid")) {
              settingsFile.setValue("modid", modID);
            }
            if (!settingsFile.contains("version") ||
                (!version.isEmpty() &&
                 (VersionInfo(version) >= VersionInfo(settingsFile.value("version").toString())))) {
              settingsFile.setValue("version", version);
            }
            if (!newestVersion.isEmpty() || !settingsFile.contains("newestVersion")) {
              settingsFile.setValue("newestVersion", newestVersion);
            }
            if (!settingsFile.contains("category")) {
              settingsFile.setValue("category", QString::number(categoryID));
            }
            settingsFile.setValue("installationFile", m_CurrentFile);

            success = true;
          }
        } else {
          if (installFomodInternal(baseNode, fomodPath, modsDirectory,
                                   modID, version, newestVersion, categoryID,
                                   modName, nameGuessed, manualRequest)) {
            success = true;
          }
        }
        if (success) {
          DirectoryTree::node_iterator iniTweakNode = baseNode->nodeFind(DirectoryTreeInformation("INI Tweaks"));
          hasIniTweaks = (iniTweakNode != baseNode->nodesEnd()) &&
                         ((*iniTweakNode)->numLeafs() != 0);
        }
        if (baseNode != filesTree) {
          delete baseNode;
        }
      } else {
        if (QuestionBoxMemory::query(m_ParentWidget, Settings::instance().directInterface(), "missingNCC",
                        tr("Installer missing"),
                        tr("This package contains a scripted installer. To use this installer "
                           "you need the optional \"NCC\"-package and the .net runtime. "
                           "Do you want to continue, treating this as a manual installer?"),
                        QDialogButtonBox::Yes | QDialogButtonBox::Cancel) == QMessageBox::Yes) {
          manualRequest = true;
        } else {
          MessageDialog::showMessage(tr("Please install NCC"), m_ParentWidget);
        }
      }
    }
  }

  // simple installer?
  if ((baseNode == NULL) && enableQuickInstall && !manualRequest) {
    baseNode = getSimpleArchiveBase(filesTree);
    if (baseNode != NULL) {
      qDebug("treating as simple archive (%d)", baseNode->numLeafs());
      SimpleInstallDialog dialog(modName, m_ParentWidget);
      if (dialog.exec() == QDialog::Accepted) {
        mapToArchive(baseNode);
        modName = dialog.getName();
        if (doInstall(modsDirectory, modName, modID, version, newestVersion, categoryID)) {
          success = true;

          DirectoryTree::node_iterator iniTweakNode = baseNode->nodeFind(DirectoryTreeInformation("INI Tweaks"));
          hasIniTweaks = (iniTweakNode != baseNode->nodesEnd()) &&
                         ((*iniTweakNode)->numLeafs() != 0);
        }
      } else {
        if (dialog.manualRequested()) {
          manualRequest = true;
          modName = dialog.getName();
        }
      }
    }
  }

  // bain complex package?
  if ((baseNode == NULL) && !manualRequest) {
    if (checkBainPackage(filesTree)) {
      bool hasPackageTXT = unpackPackageTXT();

      baseNode = filesTree;
      qDebug("treating as complex archive (%d)", filesTree->numNodes());
      BainComplexInstallerDialog dialog(filesTree, modName, hasPackageTXT, m_ParentWidget);
      if (dialog.exec() == QDialog::Accepted) {
        modName = dialog.getName();
        // create a new tree with the selected directories mapped to the
        // base directory. This is destructive on the original tree
        baseNode = dialog.updateTree(baseNode);
        mapToArchive(baseNode);

        if (doInstall(modsDirectory, modName, modID, version, newestVersion, categoryID)) {
          success = true;

          DirectoryTree::node_iterator iniTweakNode = baseNode->nodeFind(DirectoryTreeInformation("INI Tweaks"));
          hasIniTweaks = (iniTweakNode != baseNode->nodesEnd()) &&
                         ((*iniTweakNode)->numLeafs() != 0);
        }
        delete baseNode;
      } else {
        if (dialog.manualRequested()) {
          manualRequest = true;
          modName = dialog.getName();
        }
      }
      QFile::remove(QDir::tempPath().append("/package.txt"));
    }
  }

  // final option: manual installer
  if ((baseNode == NULL) || manualRequest) {
    qDebug("offering installation dialog");
    InstallDialog dialog(filesTree, modName, m_ParentWidget);
    connect(&dialog, SIGNAL(openFile(QString)), this, SLOT(openFile(QString)));
    if (dialog.exec() == QDialog::Accepted) {
      modName = dialog.getModName();
      baseNode = dialog.getDataTree();
      mapToArchive(baseNode);
      if (doInstall(modsDirectory, modName, modID, version, newestVersion, categoryID)) {
        success = true;

        DirectoryTree::node_iterator iniTweakNode = baseNode->nodeFind(DirectoryTreeInformation("INI Tweaks"));
        hasIniTweaks = (iniTweakNode != baseNode->nodesEnd()) &&
                       ((*iniTweakNode)->numLeafs() != 0);
      }
      delete baseNode; // baseNode is a new tree, independent of filesTree
    }
  }

  delete filesTree;
  m_CurrentArchive->close();

  for (std::set<QString>::iterator iter = m_FilesToDelete.begin();
       iter != m_FilesToDelete.end(); ++iter) {
    QFile(*iter).remove();
  }
  m_FilesToDelete.clear();

  for (std::set<QString>::iterator iter = m_TempFilesToDelete.begin();
       iter != m_TempFilesToDelete.end(); ++iter) {
    QFile(QDir::tempPath().append("/").append(*iter)).remove();
  }

  m_TempFilesToDelete.clear();

  return success;
}



QString InstallationManager::getErrorString(Archive::Error errorCode)
{
  switch (errorCode) {
    case Archive::ERROR_NONE: {
      return tr("no error");
    } break;
    case Archive::ERROR_LIBRARY_NOT_FOUND: {
      return tr("7z.dll not found");
    } break;
    case Archive::ERROR_LIBRARY_INVALID: {
      return tr("7z.dll isn't valid");
    } break;
    case Archive::ERROR_ARCHIVE_NOT_FOUND: {
      return tr("archive not found");
    } break;
    case Archive::ERROR_FAILED_TO_OPEN_ARCHIVE: {
      return tr("failed to open archive");
    } break;
    case Archive::ERROR_INVALID_ARCHIVE_FORMAT: {
      return tr("unsupported archive type");
    } break;
    case Archive::ERROR_LIBRARY_ERROR: {
      return tr("internal library error");
    } break;
    case Archive::ERROR_ARCHIVE_INVALID: {
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
  if (installerCustom != NULL) {
    std::set<QString> extensions = installerCustom->supportedExtensions();
    m_SupportedExtensions.insert(extensions.begin(), extensions.end());
  }
}

QStringList InstallationManager::getSupportedExtensions() const
{
  QStringList result;
  foreach (const QString &extension, m_SupportedExtensions) {
    result.append(extension);
  }
  return result;
}
