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

#include "modinfobackup.h"
#include "modinforegular.h"
#include "modinfoforeign.h"
#include "modinfooverwrite.h"

#include "installationtester.h"
#include "categories.h"
#include "modinfodialog.h"
#include "overwriteinfodialog.h"
#include "filenamestring.h"
#include "versioninfo.h"

#include <iplugingame.h>
#include <versioninfo.h>
#include <appconfig.h>
#include <scriptextender.h>

#include <QApplication>
#include <QDirIterator>
#include <QMutexLocker>
#include <QSettings>

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


void ModInfo::updateFromDisc(const QString &modDirectory,
                             DirectoryEntry **directoryStructure,
                             bool displayForeign,
                             MOBase::IPluginGame const *game)
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
    QStringList dlcPlugins = game->getDLCPlugins();
    QStringList mainPlugins = game->getPrimaryPlugins();
    QDir dataDir(game->dataDirectory());
    for (const QString &file : dataDir.entryList({ "*.esp", "*.esm" })) {
      if (std::find_if(mainPlugins.begin(), mainPlugins.end(),
                       [&file](QString const &p) {
                          return p.compare(file, Qt::CaseInsensitive) == 0; }) == mainPlugins.end()
          && (displayForeign // show non-dlc bundles only if the user wants them
              || std::find_if(dlcPlugins.begin(), dlcPlugins.end(),
                              [&file](QString const &p) {
                                  return p.compare(file, Qt::CaseInsensitive) == 0; }) != dlcPlugins.end())) {

        QFileInfo f(file); //Just so I can get a basename...
        QStringList archives;
        for (const QString &archiveName : dataDir.entryList({ f.baseName() + "*.bsa" })) {
          archives.append(dataDir.absoluteFilePath(archiveName));
        }

        createFromPlugin(file, archives, directoryStructure);
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

  //I ought to store this, it's used elsewhere
  IPluginGame const *game = qApp->property("managed_game").value<IPluginGame const *>();

  modIDs.push_back(game->getNexusModOrganizerID());

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
