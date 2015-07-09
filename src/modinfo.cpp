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

#include "modinfo.h"

#include "utility.h"
#include "installationtester.h"
#include "categories.h"
#include "report.h"
#include "modinfodialog.h"
#include "overwriteinfodialog.h"
#include "json.h"
#include "messagedialog.h"
#include "filenamestring.h"

#include <gameinfo.h>
#include <iplugingame.h>
#include <versioninfo.h>
#include <appconfig.h>
#include <scriptextender.h>

#include <QApplication>

#include <QDirIterator>
#include <QMutexLocker>
#include <QSettings>
#include <sstream>


using namespace MOBase;
using namespace MOShared;


std::vector<ModInfo::Ptr> ModInfo::s_Collection;
std::map<QString, unsigned int> ModInfo::s_ModsByName;
std::map<int, std::vector<unsigned int> > ModInfo::s_ModsByModID;
int ModInfo::s_NextID;
QMutex ModInfo::s_Mutex(QMutex::Recursive);

QString ModInfo::s_HiddenExt(".mohidden");


static bool ByName(const ModInfo::Ptr &LHS, const ModInfo::Ptr &RHS)
{
  return QString::compare(LHS->name(), RHS->name(), Qt::CaseInsensitive) < 0;
}


ModInfo::Ptr ModInfo::createFrom(const QDir &dir, DirectoryEntry **directoryStructure)
{
  QMutexLocker locker(&s_Mutex);
//  int id = s_NextID++;
  static QRegExp backupExp(".*backup[0-9]*");
  ModInfo::Ptr result;
  if (backupExp.exactMatch(dir.dirName())) {
    result = ModInfo::Ptr(new ModInfoBackup(dir, directoryStructure));
  } else {
    result = ModInfo::Ptr(new ModInfoRegular(dir, directoryStructure));
  }
  s_Collection.push_back(result);
  return result;
}

ModInfo::Ptr ModInfo::createFromPlugin(const QString &espName, const QStringList &bsaNames
                                       , DirectoryEntry ** directoryStructure)
{
  QMutexLocker locker(&s_Mutex);
  ModInfo::Ptr result = ModInfo::Ptr(new ModInfoForeign(espName, bsaNames, directoryStructure));
  s_Collection.push_back(result);
  return result;
}

QString ModInfo::getContentTypeName(int contentType)
{
  switch (contentType) {
    case CONTENT_PLUGIN:    return tr("Plugins");
    case CONTENT_TEXTURE:   return tr("Textures");
    case CONTENT_MESH:      return tr("Meshes");
    case CONTENT_BSA:       return tr("BSA");
    case CONTENT_INTERFACE: return tr("UI Changes");
    case CONTENT_MUSIC:     return tr("Music");
    case CONTENT_SOUND:     return tr("Sound Effects");
    case CONTENT_SCRIPT:    return tr("Scripts");
    case CONTENT_SKSE:      return tr("SKSE Plugins");
    case CONTENT_SKYPROC:   return tr("SkyProc Tools");
    case CONTENT_STRING:    return tr("Strings");
    default: throw MyException(tr("invalid content type %1").arg(contentType));
  }
}

void ModInfo::createFromOverwrite()
{
  QMutexLocker locker(&s_Mutex);

  s_Collection.push_back(ModInfo::Ptr(new ModInfoOverwrite));
}

unsigned int ModInfo::getNumMods()
{
  QMutexLocker locker(&s_Mutex);
  return s_Collection.size();
}


ModInfo::Ptr ModInfo::getByIndex(unsigned int index)
{
  QMutexLocker locker(&s_Mutex);

  if (index >= s_Collection.size()) {
    throw MyException(tr("invalid index %1").arg(index));
  }
  return s_Collection[index];
}


std::vector<ModInfo::Ptr> ModInfo::getByModID(int modID)
{
  QMutexLocker locker(&s_Mutex);

  auto iter = s_ModsByModID.find(modID);
  if (iter == s_ModsByModID.end()) {
    return std::vector<ModInfo::Ptr>();
  }

  std::vector<ModInfo::Ptr> result;
  for (auto idxIter = iter->second.begin(); idxIter != iter->second.end(); ++idxIter) {
    result.push_back(getByIndex(*idxIter));
  }

  return result;
}


bool ModInfo::removeMod(unsigned int index)
{
  QMutexLocker locker(&s_Mutex);

  if (index >= s_Collection.size()) {
    throw MyException(tr("invalid index %1").arg(index));
  }
  // update the indices first
  ModInfo::Ptr modInfo = s_Collection[index];
  s_ModsByName.erase(s_ModsByName.find(modInfo->name()));

  auto iter = s_ModsByModID.find(modInfo->getNexusID());
  if (iter != s_ModsByModID.end()) {
    std::vector<unsigned int> indices = iter->second;
    indices.erase(std::remove(indices.begin(), indices.end(), index), indices.end());
    s_ModsByModID[modInfo->getNexusID()] = indices;
  }

  // physically remove the mod directory
  //TODO the return value is ignored because the indices were already removed here, so stopping
  // would cause data inconsistencies. Instead we go through with the removal but the mod will show up
  // again if the user refreshes
  modInfo->remove();

  // finally, remove the mod from the collection
  s_Collection.erase(s_Collection.begin() + index);

  // and update the indices
  updateIndices();
  return true;
}


unsigned int ModInfo::getIndex(const QString &name)
{
  QMutexLocker locker(&s_Mutex);

  std::map<QString, unsigned int>::iterator iter = s_ModsByName.find(name);
  if (iter == s_ModsByName.end()) {
    return UINT_MAX;
  }

  return iter->second;
}

unsigned int ModInfo::findMod(const boost::function<bool (ModInfo::Ptr)> &filter)
{
  for (unsigned int i = 0U; i < s_Collection.size(); ++i) {
    if (filter(s_Collection[i])) {
      return i;
    }
  }
  return UINT_MAX;
}


void ModInfo::updateFromDisc(const QString &modDirectory, DirectoryEntry **directoryStructure, bool displayForeign)
{
  QMutexLocker lock(&s_Mutex);
  s_Collection.clear();
  s_NextID = 0;

  { // list all directories in the mod directory and make a mod out of each
    QDir mods(QDir::fromNativeSeparators(modDirectory));
    mods.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    QDirIterator modIter(mods);
    while (modIter.hasNext()) {
      createFrom(QDir(modIter.next()), directoryStructure);
    }
  }

  { // list plugins in the data directory and make a foreign-managed mod out of each
    std::vector<std::wstring> dlcPlugins = GameInfo::instance().getDLCPlugins();
    QDir dataDir(QDir::fromNativeSeparators(ToQString(GameInfo::instance().getGameDirectory())) + "/data");
    foreach (const QFileInfo &file, dataDir.entryInfoList(QStringList() << "*.esp" << "*.esm")) {
      if ((file.baseName() != "Update") // hide update
          && (file.baseName() != ToQString(GameInfo::instance().getGameName())) // hide the game esp
          && (displayForeign // show non-dlc bundles only if the user wants them
              || std::find(dlcPlugins.begin(), dlcPlugins.end(), ToWString(file.fileName())) != dlcPlugins.end())) {
        QStringList archives;
        foreach (const QString archiveName, dataDir.entryList(QStringList() << file.baseName() + "*.bsa")) {
          archives.append(dataDir.absoluteFilePath(archiveName));
        }

        createFromPlugin(file.fileName(), archives, directoryStructure);
      }
    }
  }

  createFromOverwrite();

  std::sort(s_Collection.begin(), s_Collection.end(), ByName);

  updateIndices();
}


void ModInfo::updateIndices()
{
  s_ModsByName.clear();
  s_ModsByModID.clear();

  for (unsigned int i = 0; i < s_Collection.size(); ++i) {
    QString modName = s_Collection[i]->internalName();
    int modID = s_Collection[i]->getNexusID();
    s_ModsByName[modName] = i;
    s_ModsByModID[modID].push_back(i);
  }
}


ModInfo::ModInfo()
  : m_Valid(false), m_PrimaryCategory(-1)
{
}


void ModInfo::checkChunkForUpdate(const std::vector<int> &modIDs, QObject *receiver)
{
  if (modIDs.size() != 0) {
    NexusInterface::instance()->requestUpdates(modIDs, receiver, QVariant(), QString());
  }
}


int ModInfo::checkAllForUpdate(QObject *receiver)
{
  // technically this should be 255 but those requests can take nexus fairly long, produce
  // large output and may have been the cause of issue #1166
  static const int chunkSize = 64;

  int result = 0;
  std::vector<int> modIDs;

  modIDs.push_back(GameInfo::instance().getNexusModID());

  for (const ModInfo::Ptr &mod : s_Collection) {
    if (mod->canBeUpdated()) {
      modIDs.push_back(mod->getNexusID());
      if (modIDs.size() >= chunkSize) {
        checkChunkForUpdate(modIDs, receiver);
        modIDs.clear();
      }
    }
  }

  checkChunkForUpdate(modIDs, receiver);

  return result;
}

void ModInfo::setVersion(const VersionInfo &version)
{
  m_Version = version;
}

void ModInfo::addCategory(const QString &categoryName)
{
  int id = CategoryFactory::instance().getCategoryID(categoryName);
  if (id == -1) {
    id = CategoryFactory::instance().addCategory(categoryName, std::vector<int>(), 0);
  }
  setCategory(id, true);
}

bool ModInfo::removeCategory(const QString &categoryName)
{
  int id = CategoryFactory::instance().getCategoryID(categoryName);
  if (id == -1) {
    return false;
  }
  if (!categorySet(id)) {
    return false;
  }
  setCategory(id, false);
  return true;
}

QStringList ModInfo::categories()
{
  QStringList result;

  CategoryFactory &catFac = CategoryFactory::instance();
  for (int id : m_Categories) {
    result.append(catFac.getCategoryName(catFac.getCategoryIndex(id)));
  }

  return result;
}

bool ModInfo::hasFlag(ModInfo::EFlag flag) const
{
  std::vector<EFlag> flags = getFlags();
  return std::find(flags.begin(), flags.end(), flag) != flags.end();
}

bool ModInfo::hasContent(ModInfo::EContent content) const
{
  std::vector<EContent> contents = getContents();
  return std::find(contents.begin(), contents.end(), content) != contents.end();
}

bool ModInfo::categorySet(int categoryID) const
{
  for (std::set<int>::const_iterator iter = m_Categories.begin(); iter != m_Categories.end(); ++iter) {
    if ((*iter == categoryID) ||
        (CategoryFactory::instance().isDecendantOf(*iter, categoryID))) {
      return true;
    }
  }

  return false;
}

void ModInfo::testValid()
{
  m_Valid = false;
  QDirIterator dirIter(absolutePath());
  while (dirIter.hasNext()) {
    dirIter.next();
    if (dirIter.fileInfo().isDir()) {
      if (InstallationTester::isTopLevelDirectory(dirIter.fileName())) {
        m_Valid = true;
        break;
      }
    } else {
      if (InstallationTester::isTopLevelSuffix(dirIter.fileName())) {
        m_Valid = true;
        break;
      }
    }
  }

  // NOTE: in Qt 4.7 it seems that QDirIterator leaves a file handle open if it is not iterated to the
  // end
  while (dirIter.hasNext()) {
    dirIter.next();
  }
}


ModInfoWithConflictInfo::ModInfoWithConflictInfo(DirectoryEntry **directoryStructure)
  : m_DirectoryStructure(directoryStructure) {}

void ModInfoWithConflictInfo::clearCaches()
{
  m_LastConflictCheck = QTime();
}

std::vector<ModInfo::EFlag> ModInfoWithConflictInfo::getFlags() const
{
  std::vector<ModInfo::EFlag> result;
  switch (isConflicted()) {
    case CONFLICT_MIXED: {
      result.push_back(ModInfo::FLAG_CONFLICT_MIXED);
    } break;
    case CONFLICT_OVERWRITE: {
      result.push_back(ModInfo::FLAG_CONFLICT_OVERWRITE);
    } break;
    case CONFLICT_OVERWRITTEN: {
      result.push_back(ModInfo::FLAG_CONFLICT_OVERWRITTEN);
    } break;
    case CONFLICT_REDUNDANT: {
      result.push_back(ModInfo::FLAG_CONFLICT_REDUNDANT);
    } break;
    default: { /* NOP */ }
  }
  return result;
}


void ModInfoWithConflictInfo::doConflictCheck() const
{
  m_OverwriteList.clear();
  m_OverwrittenList.clear();

  bool providesAnything = false;

  int dataID = 0;
  if ((*m_DirectoryStructure)->originExists(L"data")) {
    dataID = (*m_DirectoryStructure)->getOriginByName(L"data").getID();
  }

  std::wstring name = ToWString(this->name());

  m_CurrentConflictState = CONFLICT_NONE;

  if ((*m_DirectoryStructure)->originExists(name)) {
    FilesOrigin &origin = (*m_DirectoryStructure)->getOriginByName(name);
    std::vector<FileEntry::Ptr> files = origin.getFiles();
    // for all files in this origin
    for (FileEntry::Ptr file : files) {
      const std::vector<int> &alternatives = file->getAlternatives();
      if ((alternatives.size() == 0) || (alternatives[0] == dataID)) {
        // no alternatives -> no conflict
        providesAnything = true;
      } else {
        if (file->getOrigin() != origin.getID()) {
          FilesOrigin &altOrigin = (*m_DirectoryStructure)->getOriginByID(file->getOrigin());
          unsigned int altIndex = ModInfo::getIndex(ToQString(altOrigin.getName()));
          m_OverwrittenList.insert(altIndex);
        } else {
          providesAnything = true;
        }

        // for all non-providing alternative origins
        for (int altId : alternatives) {
          if ((altId != dataID) && (altId != origin.getID())) {
            FilesOrigin &altOrigin = (*m_DirectoryStructure)->getOriginByID(altId);
            unsigned int altIndex = ModInfo::getIndex(ToQString(altOrigin.getName()));
            if (origin.getPriority() > altOrigin.getPriority()) {
              m_OverwriteList.insert(altIndex);
            } else {
              m_OverwrittenList.insert(altIndex);
            }
          }
        }
      }
    }
    m_LastConflictCheck = QTime::currentTime();

    if (files.size() != 0) {
      if (!providesAnything)
        m_CurrentConflictState = CONFLICT_REDUNDANT;
      else if (!m_OverwriteList.empty() && !m_OverwrittenList.empty())
        m_CurrentConflictState = CONFLICT_MIXED;
      else if (!m_OverwriteList.empty())
        m_CurrentConflictState = CONFLICT_OVERWRITE;
      else if (!m_OverwrittenList.empty())
        m_CurrentConflictState = CONFLICT_OVERWRITTEN;
    }
  }
}

ModInfoWithConflictInfo::EConflictType ModInfoWithConflictInfo::isConflicted() const
{
  // this is costy so cache the result
  QTime now = QTime::currentTime();
  if (m_LastConflictCheck.isNull() || (m_LastConflictCheck.secsTo(now) > 10)) {
    doConflictCheck();
  }

  return m_CurrentConflictState;
}


bool ModInfoWithConflictInfo::isRedundant() const
{
  std::wstring name = ToWString(this->name());
  if ((*m_DirectoryStructure)->originExists(name)) {
    FilesOrigin &origin = (*m_DirectoryStructure)->getOriginByName(name);
    std::vector<FileEntry::Ptr> files = origin.getFiles();
    bool ignore = false;
    for (auto iter = files.begin(); iter != files.end(); ++iter) {
      if ((*iter)->getOrigin(ignore) == origin.getID()) {
        return false;
      }
    }
    return true;
  } else {
    return false;
  }
}



ModInfoRegular::ModInfoRegular(const QDir &path, DirectoryEntry **directoryStructure)
  : ModInfoWithConflictInfo(directoryStructure)
  , m_Name(path.dirName())
  , m_Path(path.absolutePath())
  , m_Repository()
  , m_MetaInfoChanged(false)
  , m_EndorsedState(ENDORSED_UNKNOWN)
  , m_NexusBridge()
{
  testValid();
  m_CreationTime = QFileInfo(path.absolutePath()).created();
  // read out the meta-file for information
  readMeta();

  connect(&m_NexusBridge, SIGNAL(descriptionAvailable(int,QVariant,QVariant))
          , this, SLOT(nxmDescriptionAvailable(int,QVariant,QVariant)));
  connect(&m_NexusBridge, SIGNAL(endorsementToggled(int,QVariant,QVariant))
          , this, SLOT(nxmEndorsementToggled(int,QVariant,QVariant)));
  connect(&m_NexusBridge, SIGNAL(requestFailed(int,int,QVariant,QString))
          , this, SLOT(nxmRequestFailed(int,int,QVariant,QString)));
}


ModInfoRegular::~ModInfoRegular()
{
  try {
    saveMeta();
  } catch (const std::exception &e) {
    qCritical("failed to save meta information for \"%s\": %s",
              m_Name.toUtf8().constData(), e.what());
  }
}

bool ModInfoRegular::isEmpty() const
{
  QDirIterator iter(m_Path, QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs);
  if (!iter.hasNext()) return true;
  iter.next();
  if ((iter.fileName() == "meta.ini") && !iter.hasNext()) return true;
  return false;
}


void ModInfoRegular::readMeta()
{
  QSettings metaFile(m_Path + "/meta.ini", QSettings::IniFormat);
  m_Notes            = metaFile.value("notes", "").toString();
  m_NexusID          = metaFile.value("modid", -1).toInt();
  m_Version.parse(metaFile.value("version", "").toString());
  m_NewestVersion    = metaFile.value("newestVersion", "").toString();
  m_IgnoredVersion   = metaFile.value("ignoredVersion", "").toString();
  m_InstallationFile = metaFile.value("installationFile", "").toString();
  m_NexusDescription = metaFile.value("nexusDescription", "").toString();
  m_Repository = metaFile.value("repository", "Nexus").toString();
  m_LastNexusQuery = QDateTime::fromString(metaFile.value("lastNexusQuery", "").toString(), Qt::ISODate);
  if (metaFile.contains("endorsed")) {
    if (metaFile.value("endorsed").canConvert<int>()) {
      switch (metaFile.value("endorsed").toInt()) {
        case ENDORSED_FALSE: m_EndorsedState = ENDORSED_FALSE; break;
        case ENDORSED_TRUE:  m_EndorsedState = ENDORSED_TRUE;  break;
        case ENDORSED_NEVER: m_EndorsedState = ENDORSED_NEVER; break;
        default: m_EndorsedState = ENDORSED_UNKNOWN; break;
      }
    } else {
      m_EndorsedState = metaFile.value("endorsed", false).toBool() ? ENDORSED_TRUE : ENDORSED_FALSE;
    }
  }

  QString categoriesString = metaFile.value("category", "").toString();

  QStringList categories = categoriesString.split(',', QString::SkipEmptyParts);
  for (QStringList::iterator iter = categories.begin(); iter != categories.end(); ++iter) {
    bool ok = false;
    int categoryID = iter->toInt(&ok);
    if (categoryID < 0) {
      // ignore invalid id
      continue;
    }
    if (ok && (categoryID != 0) && (CategoryFactory::instance().categoryExists(categoryID))) {
      m_Categories.insert(categoryID);
      if (iter == categories.begin()) {
        m_PrimaryCategory = categoryID;
      }
    }
  }

  int numFiles = metaFile.beginReadArray("installedFiles");
  for (int i = 0; i < numFiles; ++i) {
    metaFile.setArrayIndex(i);
    m_InstalledFileIDs.insert(std::make_pair(metaFile.value("modid").toInt(), metaFile.value("fileid").toInt()));
  }
  metaFile.endArray();

  m_MetaInfoChanged = false;
}

void ModInfoRegular::saveMeta()
{
  // only write meta data if the mod directory exists
  if (m_MetaInfoChanged && QFile::exists(absolutePath())) {
    QSettings metaFile(absolutePath().append("/meta.ini"), QSettings::IniFormat);
    if (metaFile.status() == QSettings::NoError) {
      std::set<int> temp = m_Categories;
      temp.erase(m_PrimaryCategory);
      metaFile.setValue("category", QString("%1").arg(m_PrimaryCategory) + "," + SetJoin(temp, ","));
      metaFile.setValue("newestVersion", m_NewestVersion.canonicalString());
      metaFile.setValue("ignoredVersion", m_IgnoredVersion.canonicalString());
      metaFile.setValue("version", m_Version.canonicalString());
      metaFile.setValue("installationFile", m_InstallationFile);
      metaFile.setValue("repository", m_Repository);
      metaFile.setValue("modid", m_NexusID);
      metaFile.setValue("notes", m_Notes);
      metaFile.setValue("nexusDescription", m_NexusDescription);
      metaFile.setValue("lastNexusQuery", m_LastNexusQuery.toString(Qt::ISODate));
      if (m_EndorsedState != ENDORSED_UNKNOWN) {
        metaFile.setValue("endorsed", m_EndorsedState);
      }

      metaFile.beginWriteArray("installedFiles");
      int idx = 0;
      for (auto iter = m_InstalledFileIDs.begin(); iter != m_InstalledFileIDs.end(); ++iter) {
        metaFile.setArrayIndex(idx++);
        metaFile.setValue("modid", iter->first);
        metaFile.setValue("fileid", iter->second);
      }
      metaFile.endArray();

      metaFile.sync(); // sync needs to be called to ensure the file is created

      if (metaFile.status() == QSettings::NoError) {
        m_MetaInfoChanged = false;
      } else {
        reportError(tr("failed to write %1/meta.ini: error %2").arg(absolutePath()).arg(metaFile.status()));
      }
    } else {
      reportError(tr("failed to write %1/meta.ini: error %2").arg(absolutePath()).arg(metaFile.status()));
    }
  }
}


bool ModInfoRegular::updateAvailable() const
{
  if (m_IgnoredVersion.isValid() && (m_IgnoredVersion == m_NewestVersion)) {
    return false;
  }
  return m_NewestVersion.isValid() && (m_Version < m_NewestVersion);
}


bool ModInfoRegular::downgradeAvailable() const
{
  if (m_IgnoredVersion.isValid() && (m_IgnoredVersion == m_NewestVersion)) {
    return false;
  }
  return m_NewestVersion.isValid() && (m_NewestVersion < m_Version);
}


void ModInfoRegular::nxmDescriptionAvailable(int, QVariant, QVariant resultData)
{
  QVariantMap result = resultData.toMap();
  setNewestVersion(VersionInfo(result["version"].toString()));
  setNexusDescription(result["description"].toString());

  if ((m_EndorsedState != ENDORSED_NEVER) && (result.contains("voted_by_user"))) {
    setEndorsedState(result["voted_by_user"].toBool() ? ENDORSED_TRUE : ENDORSED_FALSE);
  }
  m_LastNexusQuery = QDateTime::currentDateTime();
  //m_MetaInfoChanged = true;
  saveMeta();
  emit modDetailsUpdated(true);
}


void ModInfoRegular::nxmEndorsementToggled(int, QVariant, QVariant resultData)
{
  m_EndorsedState = resultData.toBool() ? ENDORSED_TRUE : ENDORSED_FALSE;
  m_MetaInfoChanged = true;
  saveMeta();
  emit modDetailsUpdated(true);
}


void ModInfoRegular::nxmRequestFailed(int, int, QVariant userData, const QString &errorMessage)
{
  QString fullMessage = errorMessage;
  if (userData.canConvert<int>() && (userData.toInt() == 1)) {
    fullMessage += "\nNexus will reject endorsements within 15 Minutes of a failed attempt, the error message may be misleading.";
  }
  if (QApplication::activeWindow() != nullptr) {
    MessageDialog::showMessage(fullMessage, QApplication::activeWindow());
  }
  emit modDetailsUpdated(false);
}


bool ModInfoRegular::updateNXMInfo()
{
  if (m_NexusID > 0) {
    m_NexusBridge.requestDescription(m_NexusID, QVariant());
    return true;
  }
  return false;
}


void ModInfoRegular::setCategory(int categoryID, bool active)
{
  m_MetaInfoChanged = true;

  if (active) {
    m_Categories.insert(categoryID);
    if (m_PrimaryCategory == -1) {
      m_PrimaryCategory = categoryID;
    }
  } else {
    std::set<int>::iterator iter = m_Categories.find(categoryID);
    if (iter != m_Categories.end()) {
      m_Categories.erase(iter);
    }
    if (categoryID == m_PrimaryCategory) {
      if (m_Categories.size() == 0) {
        m_PrimaryCategory = -1;
      } else {
        m_PrimaryCategory = *(m_Categories.begin());
      }
    }
  }
}


bool ModInfoRegular::setName(const QString &name)
{
  if (name.contains('/') || name.contains('\\')) {
    return false;
  }

  QString newPath = m_Path.mid(0).replace(m_Path.length() - m_Name.length(), m_Name.length(), name);
  QDir modDir(m_Path.mid(0, m_Path.length() - m_Name.length()));

  if (m_Name.compare(name, Qt::CaseInsensitive) == 0) {
    QString tempName = name;
    tempName.append("_temp");
    while (modDir.exists(tempName)) {
      tempName.append("_");
    }
    if (!modDir.rename(m_Name, tempName)) {
      return false;
    }
    if (!modDir.rename(tempName, name)) {
      qCritical("rename to final name failed after successful rename to intermediate name");
      modDir.rename(tempName, m_Name);
      return false;
    }
  } else {
    if (!shellRename(modDir.absoluteFilePath(m_Name), modDir.absoluteFilePath(name))) {
      qCritical("failed to rename mod %s (errorcode %d)",
                qPrintable(name), ::GetLastError());
      return false;
    }
  }

  std::map<QString, unsigned int>::iterator nameIter = s_ModsByName.find(m_Name);
  if (nameIter != s_ModsByName.end()) {
    unsigned int index = nameIter->second;
    s_ModsByName.erase(nameIter);

    m_Name = name;
    m_Path = newPath;

    s_ModsByName[m_Name] = index;

    std::sort(s_Collection.begin(), s_Collection.end(), ByName);
    updateIndices();
  } else { // otherwise mod isn't registered yet?
    m_Name = name;
    m_Path = newPath;
  }

  return true;
}

void ModInfoRegular::setNotes(const QString &notes)
{
  m_Notes = notes;
  m_MetaInfoChanged = true;
}

void ModInfoRegular::setNexusID(int modID)
{
  m_NexusID = modID;
  m_MetaInfoChanged = true;
}

void ModInfoRegular::setVersion(const VersionInfo &version)
{
  m_Version = version;
  m_MetaInfoChanged = true;
}

void ModInfoRegular::setNewestVersion(const VersionInfo &version)
{
  if (version != m_NewestVersion) {
    m_NewestVersion = version;
    m_MetaInfoChanged = true;
  }
}

void ModInfoRegular::setNexusDescription(const QString &description)
{
  if (qHash(description) != qHash(m_NexusDescription)) {
    m_NexusDescription = description;
    m_MetaInfoChanged = true;
  }
}

void ModInfoRegular::setEndorsedState(EEndorsedState endorsedState)
{
  if (endorsedState != m_EndorsedState) {
    m_EndorsedState = endorsedState;
    m_MetaInfoChanged = true;
  }
}

void ModInfoRegular::setInstallationFile(const QString &fileName)
{
  m_InstallationFile = fileName;
  m_MetaInfoChanged = true;
}

void ModInfoRegular::addNexusCategory(int categoryID)
{
  m_Categories.insert(CategoryFactory::instance().resolveNexusID(categoryID));
}

void ModInfoRegular::setIsEndorsed(bool endorsed)
{
  if (m_EndorsedState != ENDORSED_NEVER) {
    m_EndorsedState = endorsed ? ENDORSED_TRUE : ENDORSED_FALSE;
    m_MetaInfoChanged = true;
  }
}


void ModInfoRegular::setNeverEndorse()
{
  m_EndorsedState = ENDORSED_NEVER;
  m_MetaInfoChanged = true;
}


bool ModInfoRegular::remove()
{
  m_MetaInfoChanged = false;
  return shellDelete(QStringList(absolutePath()), true);
}

void ModInfoRegular::endorse(bool doEndorse)
{
  if (doEndorse != (m_EndorsedState == ENDORSED_TRUE)) {
    m_NexusBridge.requestToggleEndorsement(getNexusID(), doEndorse, QVariant(1));
  }
}


QString ModInfoRegular::absolutePath() const
{
  return m_Path;
}

void ModInfoRegular::ignoreUpdate(bool ignore)
{
  if (ignore) {
    m_IgnoredVersion = m_NewestVersion;
  } else {
    m_IgnoredVersion.clear();
  }
  m_MetaInfoChanged = true;
}


std::vector<ModInfo::EFlag> ModInfoRegular::getFlags() const
{
  std::vector<ModInfo::EFlag> result = ModInfoWithConflictInfo::getFlags();
  if ((m_NexusID > 0) && (endorsedState() == ENDORSED_FALSE)) {
    result.push_back(ModInfo::FLAG_NOTENDORSED);
  }
  if (!isValid()) {
    result.push_back(ModInfo::FLAG_INVALID);
  }
  if (m_Notes.length() != 0) {
    result.push_back(ModInfo::FLAG_NOTES);
  }
  return result;
}


std::vector<ModInfo::EContent> ModInfoRegular::getContents() const
{
  QTime now = QTime::currentTime();
  if (m_LastContentCheck.isNull() || (m_LastContentCheck.secsTo(now) > 60)) {
    m_CachedContent.clear();
    QDir dir(absolutePath());
    if (dir.entryList(QStringList() << "*.esp" << "*.esm").size() > 0) {
      m_CachedContent.push_back(CONTENT_PLUGIN);
    }
    if (dir.entryList(QStringList() << "*.bsa").size() > 0) {
      m_CachedContent.push_back(CONTENT_BSA);
    }

    ScriptExtender *extender = qApp->property("managed_game").value<IPluginGame*>()->feature<ScriptExtender>();

    if (extender != nullptr) {
      QString sePluginPath = extender->name() + "/plugins";
      if (dir.exists(sePluginPath)) m_CachedContent.push_back(CONTENT_SKSE);
    }
    if (dir.exists("textures"))   m_CachedContent.push_back(CONTENT_TEXTURE);
    if (dir.exists("meshes"))     m_CachedContent.push_back(CONTENT_MESH);
    if (dir.exists("interface")
        || dir.exists("menus"))   m_CachedContent.push_back(CONTENT_INTERFACE);
    if (dir.exists("music"))      m_CachedContent.push_back(CONTENT_MUSIC);
    if (dir.exists("sound"))      m_CachedContent.push_back(CONTENT_SOUND);
    if (dir.exists("scripts"))    m_CachedContent.push_back(CONTENT_SCRIPT);
    if (dir.exists("strings"))    m_CachedContent.push_back(CONTENT_STRING);
    if (dir.exists("SkyProc Patchers"))  m_CachedContent.push_back(CONTENT_SKYPROC);

    m_LastContentCheck = QTime::currentTime();
  }

  return m_CachedContent;

}


int ModInfoRegular::getHighlight() const
{
  return isValid() ? HIGHLIGHT_NONE: HIGHLIGHT_INVALID;
}


QString ModInfoRegular::getDescription() const
{
  if (!isValid()) {
    return tr("%1 contains no esp/esm and no asset (textures, meshes, interface, ...) directory").arg(name());
  } else {
    const std::set<int> &categories = getCategories();
    std::wostringstream categoryString;
    categoryString << ToWString(tr("Categories: <br>"));
    CategoryFactory &categoryFactory = CategoryFactory::instance();
    for (std::set<int>::const_iterator catIter = categories.begin();
         catIter != categories.end(); ++catIter) {
      if (catIter != categories.begin()) {
        categoryString << " , ";
      }
      categoryString << "<span style=\"white-space: nowrap;\"><i>" << ToWString(categoryFactory.getCategoryName(categoryFactory.getCategoryIndex(*catIter))) << "</font></span>";
    }

    return ToQString(categoryString.str());
  }
}

QString ModInfoRegular::notes() const
{
  return m_Notes;
}

QDateTime ModInfoRegular::creationTime() const
{
  return m_CreationTime;
}

QString ModInfoRegular::getNexusDescription() const
{
  return m_NexusDescription;
}

QString ModInfoRegular::repository() const
{
  return m_Repository;
}

ModInfoRegular::EEndorsedState ModInfoRegular::endorsedState() const
{
  return m_EndorsedState;
}

QDateTime ModInfoRegular::getLastNexusQuery() const
{
  return m_LastNexusQuery;
}


QStringList ModInfoRegular::archives() const
{
  QStringList result;
  QDir dir(this->absolutePath());
  for (const QString &archive : dir.entryList(QStringList("*.bsa"))) {
    result.append(this->absolutePath() + "/" + archive);
  }
  return result;
}

void ModInfoRegular::addInstalledFile(int modId, int fileId)
{
  m_InstalledFileIDs.insert(std::make_pair(modId, fileId));
  m_MetaInfoChanged = true;
}

std::vector<QString> ModInfoRegular::getIniTweaks() const
{
  QString metaFileName = absolutePath().append("/meta.ini");
  QSettings metaFile(metaFileName, QSettings::IniFormat);

  std::vector<QString> result;

  int numTweaks = metaFile.beginReadArray("INI Tweaks");

  if (numTweaks != 0) {
    qDebug("%d active ini tweaks in %s",
           numTweaks, QDir::toNativeSeparators(metaFileName).toUtf8().constData());
  }

  for (int i = 0; i < numTweaks; ++i) {
    metaFile.setArrayIndex(i);
    QString filename = absolutePath().append("/INI Tweaks/").append(metaFile.value("name").toString());
    result.push_back(filename);
  }
  metaFile.endArray();
  return result;
}

std::vector<ModInfo::EFlag> ModInfoBackup::getFlags() const
{
  std::vector<ModInfo::EFlag> result = ModInfoRegular::getFlags();
  result.insert(result.begin(), ModInfo::FLAG_BACKUP);
  return result;
}


QString ModInfoBackup::getDescription() const
{
  return tr("This is the backup of a mod");
}


ModInfoBackup::ModInfoBackup(const QDir &path, DirectoryEntry **directoryStructure)
  : ModInfoRegular(path, directoryStructure)
{
}


ModInfoOverwrite::ModInfoOverwrite()
{
  testValid();
}


bool ModInfoOverwrite::isEmpty() const
{
  QDirIterator iter(absolutePath(), QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs);
  if (!iter.hasNext()) return true;
  iter.next();
  if ((iter.fileName() == "meta.ini") && !iter.hasNext()) return true;
  return false;
}

QString ModInfoOverwrite::absolutePath() const
{
  return QDir::fromNativeSeparators(qApp->property("dataPath").toString() + "/" + QString::fromStdWString(AppConfig::overwritePath()));
}

std::vector<ModInfo::EFlag> ModInfoOverwrite::getFlags() const
{
  std::vector<ModInfo::EFlag> result;
  result.push_back(FLAG_OVERWRITE);
  return result;
}

int ModInfoOverwrite::getHighlight() const
{
  return (isValid() ? HIGHLIGHT_IMPORTANT : HIGHLIGHT_INVALID) | HIGHLIGHT_CENTER;
}

QString ModInfoOverwrite::getDescription() const
{
  return tr("This pseudo mod contains files from the virtual data tree that got "
            "modified (i.e. by the construction kit)");
}

QStringList ModInfoOverwrite::archives() const
{
  QStringList result;
  QDir dir(this->absolutePath());
  for (const QString &archive : dir.entryList(QStringList("*.bsa"))) {
    result.append(this->absolutePath() + "/" + archive);
  }
  return result;
}

QString ModInfoForeign::name() const
{
  return m_Name;
}

QDateTime ModInfoForeign::creationTime() const
{
  return m_CreationTime;
}

QString ModInfoForeign::absolutePath() const
{
  return QDir::fromNativeSeparators(ToQString(GameInfo::instance().getGameDirectory())) + "/data";
}

std::vector<ModInfo::EFlag> ModInfoForeign::getFlags() const
{
  std::vector<ModInfo::EFlag> result = ModInfoWithConflictInfo::getFlags();
  result.push_back(FLAG_FOREIGN);

  return result;
}

int ModInfoForeign::getHighlight() const
{
  return 0;
}

QString ModInfoForeign::getDescription() const
{
  return tr("This pseudo mod represents content managed outside MO. It isn't modified by MO.");
}

ModInfoForeign::ModInfoForeign(const QString &referenceFile, const QStringList &archives,
                               DirectoryEntry **directoryStructure)
  : ModInfoWithConflictInfo(directoryStructure)
  , m_ReferenceFile(referenceFile)
  , m_Archives(archives)
{
  m_CreationTime = QFileInfo(referenceFile).created();
  m_Name = "Unmanaged: " + QFileInfo(m_ReferenceFile).baseName();
}
