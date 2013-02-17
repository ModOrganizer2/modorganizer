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

#include <gameinfo.h>
#include <versioninfo.h>

#include <QApplication>

#include <QDirIterator>
#include <QMutexLocker>
#include <QSettings>
#include <sstream>


using namespace MOBase;
using namespace MOShared;


std::vector<ModInfo::Ptr> ModInfo::s_Collection;
std::map<QString, unsigned int> ModInfo::s_ModsByName;
std::map<int, unsigned int> ModInfo::s_ModsByModID;
int ModInfo::s_NextID;
QMutex ModInfo::s_Mutex(QMutex::Recursive);

QString ModInfo::s_HiddenExt(".mohidden");


static bool ByName(const ModInfo::Ptr &LHS, const ModInfo::Ptr &RHS)
{
  return QString::compare(LHS->name(), RHS->name(), Qt::CaseInsensitive) < 0;
}

ModInfo::NexusFileInfo::NexusFileInfo(const QString &data)
{
  QVariantList result = QtJson::Json::parse(data).toList();

  while (result.length() < 7) {
    result.append(QVariant());
  }
  id          = result.at(0).toInt();
  name        = result.at(1).toString();
  url         = result.at(2).toString();
  version     = result.at(3).toString();
  description = result.at(4).toString();
  category    = result.at(5).toInt();
  size        = result.at(6).toInt();
}


QString ModInfo::NexusFileInfo::toString() const
{
  return QString("[ %1,\"%2\",\"%3\",\"%4\",\"%5\",%6,%7 ]")
            .arg(id)
            .arg(name)
            .arg(url)
            .arg(version)
            .arg(description.mid(0).replace("\"", "'"))
            .arg(category)
            .arg(size);
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


ModInfo::Ptr ModInfo::getByModID(int modID, bool missingAcceptable)
{
  QMutexLocker locker(&s_Mutex);

  std::map<int, unsigned int>::iterator iter = s_ModsByModID.find(modID);
  if (iter == s_ModsByModID.end()) {
    if (missingAcceptable) {
      return ModInfo::Ptr();
    } else {
      throw MyException(tr("invalid mod id %1").arg(modID));
    }
  }

  return getByIndex(iter->second);
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

  //TODO this is a bit more complicated since multiple mods may have the
  // same mod id but only one appears in the index
  std::map<int, unsigned int>::iterator iter = s_ModsByModID.find(modInfo->getNexusID())  ;
  if ((iter != s_ModsByModID.end()) &&
      (iter->second == index)) {
    s_ModsByModID.erase(iter);
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


void ModInfo::updateFromDisc(const QString &modDirectory, DirectoryEntry **directoryStructure)
{
  QMutexLocker lock(&s_Mutex);
  s_Collection.clear();
  s_NextID = 0;
  // list all directories in the mod directory and make a mod out of each
  QDir mods(QDir::fromNativeSeparators(modDirectory));
  mods.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
  QDirIterator modIter(mods);
  while (modIter.hasNext()) {
    createFrom(QDir(modIter.next()), directoryStructure);
  }

  createFromOverwrite();

  std::sort(s_Collection.begin(), s_Collection.end(), ByName);

  updateIndices();
}


void ModInfo::updateIndices()
{
  s_ModsByName.clear();
  s_ModsByModID.clear();
  QRegExp backupRegEx(".*backup[0-9]*$");

  for (unsigned int i = 0; i < s_Collection.size(); ++i) {
    QString modName = s_Collection[i]->name();
    int modID = s_Collection[i]->getNexusID();
    s_ModsByName[modName] = i;

    // don't overwrite a modid-entry with a backup entry. This is a bit of a workaround
    if ((s_ModsByModID.find(modID) == s_ModsByModID.end()) ||
        !backupRegEx.exactMatch(modName)) {
      s_ModsByModID[modID] = i;
    }
  }
}


ModInfo::ModInfo()
  : m_Valid(false), m_PrimaryCategory(-1)
{
}


void ModInfo::checkChunkForUpdate(const std::vector<int> &modIDs, QObject *receiver)
{
  if (modIDs.size() != 0) {
    NexusInterface::instance()->requestUpdates(modIDs, receiver, QVariant());
  }
}


int ModInfo::checkAllForUpdate(QObject *receiver)
{
  int result = 0;
  std::vector<int> modIDs;

  modIDs.push_back(GameInfo::instance().getNexusModID());

  for (std::vector<ModInfo::Ptr>::iterator iter = s_Collection.begin();
       iter != s_Collection.end(); ++iter) {
    if ((*iter)->canBeUpdated()) {
      modIDs.push_back((*iter)->getNexusID());
      if (modIDs.size() >= 255) {
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


ModInfoRegular::ModInfoRegular(const QDir &path, DirectoryEntry **directoryStructure)
  : ModInfo(), m_Name(path.dirName()), m_Path(path.absolutePath()), m_MetaInfoChanged(false),
    m_EndorsedState(ENDORSED_UNKNOWN), m_DirectoryStructure(directoryStructure)
{
  testValid();

  // read out the meta-file for information
  QString metaFileName = path.absoluteFilePath("meta.ini");
  QSettings metaFile(metaFileName, QSettings::IniFormat);

  m_Notes           = metaFile.value("notes", "").toString();
  m_NexusID         = metaFile.value("modid", -1).toInt();
  m_Version.parse(metaFile.value("version", "").toString());
  m_NewestVersion = metaFile.value("newestVersion", "").toString();
  m_InstallationFile = metaFile.value("installationFile", "").toString();
  m_NexusDescription = metaFile.value("nexusDescription", "").toString();
  m_LastNexusQuery = QDateTime::fromString(metaFile.value("lastNexusQuery", "").toString(), Qt::ISODate);
  if (metaFile.contains("endorsed")) {
    m_EndorsedState = metaFile.value("endorsed", false).toBool() ? ENDORSED_TRUE : ENDORSED_FALSE;
  }

  int numNexusFiles = metaFile.beginReadArray("nexusFiles");
  for (int i = 0; i < numNexusFiles; ++i) {
    metaFile.setArrayIndex(i);
    QString infoString = metaFile.value("info", "").toString();
    NexusFileInfo info(infoString);
    m_NexusFileInfos.push_back(info);
  }
  metaFile.endArray();

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

  connect(&m_NexusBridge, SIGNAL(nxmDescriptionAvailable(int,QVariant,QVariant)), this, SLOT(nxmDescriptionAvailable(int,QVariant,QVariant)));
  connect(&m_NexusBridge, SIGNAL(nxmFilesAvailable(int,QVariant,QVariant)), this, SLOT(nxmFilesAvailable(int,QVariant,QVariant)));
  connect(&m_NexusBridge, SIGNAL(nxmEndorsementToggled(int,QVariant,QVariant)), this, SLOT(nxmEndorsementToggled(int,QVariant,QVariant)));
  connect(&m_NexusBridge, SIGNAL(nxmRequestFailed(int,QVariant,QString)), this, SLOT(nxmRequestFailed(int,QVariant,QString)));
}


ModInfoRegular::~ModInfoRegular()
{
  try {
    //TODO this may cause the meta-file and the directory to be
    // re-created after a remove
    saveMeta();
  } catch (const std::exception &e) {
    qCritical("failed to save meta information for \"%s\": %s",
              m_Name.toUtf8().constData(), e.what());
  }
}


void ModInfoRegular::saveMeta()
{
  if (m_MetaInfoChanged) {
    if (QFile::exists(absolutePath().append("/meta.ini"))) {
      QSettings metaFile(absolutePath().append("/meta.ini"), QSettings::IniFormat);
      if (metaFile.status() == QSettings::NoError) {
        std::set<int> temp = m_Categories;
        temp.erase(m_PrimaryCategory);
        metaFile.setValue("category", QString("%1").arg(m_PrimaryCategory) + "," + SetJoin(temp, ","));
        metaFile.setValue("newestVersion", m_NewestVersion.canonicalString());
        metaFile.setValue("version", m_Version.canonicalString());
        metaFile.setValue("modid", m_NexusID);
        metaFile.setValue("notes", m_Notes);
        metaFile.setValue("nexusDescription", m_NexusDescription);
        metaFile.setValue("lastNexusQuery", m_LastNexusQuery.toString(Qt::ISODate));
        if (m_EndorsedState != ENDORSED_UNKNOWN) {
          metaFile.setValue("endorsed", m_EndorsedState == ENDORSED_TRUE ? true : false);
        }

        metaFile.beginWriteArray("nexusFiles");
        for (int i = 0; i < m_NexusFileInfos.size(); ++i) {
          metaFile.setArrayIndex(i);
          metaFile.setValue("info", m_NexusFileInfos.at(i).toString());
        }
        metaFile.endArray();
      } else {
        reportError(tr("failed to write %1/meta.ini: %2").arg(absolutePath()).arg(metaFile.status()));
      }
    } else {
      qWarning("mod %s has no meta.ini at %s/meta.ini", m_Name.toUtf8().constData(), absolutePath().toUtf8().constData());
    }
    m_MetaInfoChanged = false;
  }
}


bool ModInfoRegular::updateAvailable() const
{
  return m_NewestVersion.isValid() && (m_Version < m_NewestVersion);
}


void ModInfoRegular::nxmDescriptionAvailable(int, QVariant, QVariant resultData)
{
  QVariantMap result = resultData.toMap();
  m_NewestVersion.parse(result["version"].toString());
  m_NexusDescription = result["description"].toString();
  m_EndorsedState = result["voted_by_user"].toBool() ? ENDORSED_TRUE : ENDORSED_FALSE;
  m_NexusBridge.requestFiles(m_NexusID, QVariant());
}


void ModInfoRegular::nxmFilesAvailable(int, QVariant, QVariant resultData)
{
  m_NexusFileInfos.clear();

  QVariantList result = resultData.toList();

  foreach(QVariant file, result) {
    QVariantMap fileInfo = file.toMap();

    m_NexusFileInfos.push_back(NexusFileInfo(fileInfo["id"].toInt(),
                                             fileInfo["name"].toString(),
                                             fileInfo["uri"].toString(),
                                             fileInfo["version"].toString(),
                                             fileInfo["description"].toString(),
                                             fileInfo["category_id"].toInt(),
                                             fileInfo["size"].toInt()));
  }

  m_LastNexusQuery = QDateTime::currentDateTime();
  m_MetaInfoChanged = true;
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


void ModInfoRegular::nxmRequestFailed(int, QVariant userData, const QString &errorMessage)
{
  QString fullMessage = errorMessage;
  if (userData.canConvert<int>() && (userData.toInt() == 1)) {
    fullMessage += "\nNexus will reject endorsements within 15 Minutes of a failed attempt, the error message may be misleading.";
  }
  if (QApplication::activeWindow() != NULL) {
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
    if (!modDir.rename(m_Name, name)) {
      return false;
    }
  }

  std::map<QString, unsigned int>::iterator nameIter = s_ModsByName.find(m_Name);
  unsigned int index = nameIter->second;
  s_ModsByName.erase(nameIter);

  m_Name = name;
  m_Path = newPath;

  s_ModsByName[m_Name] = index;

  std::sort(s_Collection.begin(), s_Collection.end(), ByName);

  updateIndices();

  return true;
}

void ModInfoRegular::setNotes(const QString &notes)
{
  m_Notes = notes;
}

void ModInfoRegular::setVersion(const VersionInfo &version)
{
  m_Version = version;
  m_MetaInfoChanged = true;
}

void ModInfoRegular::setNexusDescription(const QString &description)
{
  m_NexusDescription = description;
  m_MetaInfoChanged = true;
}


void ModInfoRegular::setIsEndorsed(bool endorsed)
{
  m_EndorsedState = endorsed ? ENDORSED_TRUE : ENDORSED_FALSE;
  m_MetaInfoChanged = true;
}


bool ModInfoRegular::remove()
{
  m_MetaInfoChanged = false;
  return removeDir(absolutePath());
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


std::vector<ModInfo::EFlag> ModInfoRegular::getFlags() const
{
  std::vector<ModInfo::EFlag> result;
  if (!isValid()) {
    result.push_back(ModInfo::FLAG_INVALID);
  }
  if ((m_NexusID != -1) && (endorsedState() == ENDORSED_FALSE)) {
    result.push_back(ModInfo::FLAG_NOTENDORSED);
  }
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
  if (m_Notes.length() != 0) {
    result.push_back(ModInfo::FLAG_NOTES);
  }
  return result;
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

void ModInfoRegular::getNexusFiles(QList<ModInfo::NexusFileInfo>::const_iterator &begin, QList<ModInfo::NexusFileInfo>::const_iterator &end)
{
  begin = m_NexusFileInfos.begin();
  end = m_NexusFileInfos.end();
}

QString ModInfoRegular::getNexusDescription() const
{
  return m_NexusDescription;
}


ModInfoRegular::EEndorsedState ModInfoRegular::endorsedState() const
{
  return m_EndorsedState;
}


ModInfoRegular::EConflictType ModInfoRegular::isConflicted() const
{
  bool overwrite = false;
  bool overwritten = false;
  bool regular = false;

  int dataID = 0;
  if ((*m_DirectoryStructure)->originExists(L"data")) {
    dataID = (*m_DirectoryStructure)->getOriginByName(L"data").getID();
  }

  std::wstring name = ToWString(m_Name);
  if ((*m_DirectoryStructure)->originExists(name)) {
    FilesOrigin &origin = (*m_DirectoryStructure)->getOriginByName(name);
    std::vector<FileEntry*> files = origin.getFiles();
    for (auto iter = files.begin(); iter != files.end() && (!overwrite || !overwritten || !regular); ++iter) {
      const std::vector<int> &alternatives = (*iter)->getAlternatives();
      if (alternatives.size() == 0) {
        // no alternatives -> no conflict
        regular = true;
      } else {
        for (auto altIter = alternatives.begin(); altIter != alternatives.end(); ++altIter) {
          // don't treat files overwritten in data as "conflict"
          if (*altIter != dataID) {
            bool ignore = false;
            if ((*iter)->getOrigin(ignore) == origin.getID()) {
              overwrite = true;
              break;
            } else {
              overwritten = true;
              break;
            }
          }
        }
      }
    }
  }

  if (overwrite && overwritten) return CONFLICT_MIXED;
  else if (overwrite) return CONFLICT_OVERWRITE;
  else if (overwritten) {
    if (!regular) {
      return CONFLICT_REDUNDANT;
    } else {
      return CONFLICT_OVERWRITTEN;
    }
  }
  else return CONFLICT_NONE;
}


bool ModInfoRegular::isRedundant() const
{
  std::wstring name = ToWString(m_Name);
  if ((*m_DirectoryStructure)->originExists(name)) {
    FilesOrigin &origin = (*m_DirectoryStructure)->getOriginByName(name);
    std::vector<FileEntry*> files = origin.getFiles();
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


QDateTime ModInfoRegular::getLastNexusQuery() const
{
  return m_LastNexusQuery;
}

std::vector<QString> ModInfoRegular::getIniTweaks() const
{
  QString metaFileName = absolutePath().append("/meta.ini");
  QSettings metaFile(metaFileName, QSettings::IniFormat);

  std::vector<QString> result;

  int numTweaks = metaFile.beginReadArray("INI Tweaks");

  if (numTweaks != 0) {
    qDebug("%d active ini tweaks in %s",
           numTweaks, metaFileName.toUtf8().constData());
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


QString ModInfoOverwrite::absolutePath() const
{
  return QDir::fromNativeSeparators(ToQString(GameInfo::instance().getOverwriteDir()));
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
