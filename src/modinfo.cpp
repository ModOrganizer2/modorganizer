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
#include "modinfoseparator.h"

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
#include <unmanagedmods.h>

#include <QApplication>
#include <QDirIterator>
#include <QMutexLocker>
#include <QSettings>

using namespace MOBase;
using namespace MOShared;


std::vector<ModInfo::Ptr> ModInfo::s_Collection;
std::map<QString, unsigned int> ModInfo::s_ModsByName;
std::map<std::pair<QString, int>, std::vector<unsigned int>> ModInfo::s_ModsByModID;
int ModInfo::s_NextID;
QMutex ModInfo::s_Mutex(QMutex::Recursive);

QString ModInfo::s_HiddenExt(".mohidden");


bool ModInfo::ByName(const ModInfo::Ptr &LHS, const ModInfo::Ptr &RHS)
{
  return QString::compare(LHS->name(), RHS->name(), Qt::CaseInsensitive) < 0;
}


ModInfo::Ptr ModInfo::createFrom(PluginContainer *pluginContainer, const MOBase::IPluginGame *game, const QDir &dir, DirectoryEntry **directoryStructure)
{
  QMutexLocker locker(&s_Mutex);
  //  int id = s_NextID++;
  static QRegExp backupExp(".*backup[0-9]*");
  static QRegExp separatorExp(".*_separator");
  ModInfo::Ptr result;
  if (backupExp.exactMatch(dir.dirName())) {
    result = ModInfo::Ptr(new ModInfoBackup(pluginContainer, game, dir, directoryStructure));
  } else if(separatorExp.exactMatch(dir.dirName())){
    result = Ptr(new ModInfoSeparator(pluginContainer, game, dir, directoryStructure));
  } else {
    result = ModInfo::Ptr(new ModInfoRegular(pluginContainer, game, dir, directoryStructure));
  }
  s_Collection.push_back(result);
  return result;
}

ModInfo::Ptr ModInfo::createFromPlugin(const QString &modName,
                                       const QString &espName,
                                       const QStringList &bsaNames,
                                       ModInfo::EModType modType,
                                       DirectoryEntry **directoryStructure,
                                       PluginContainer *pluginContainer) {
  QMutexLocker locker(&s_Mutex);
  ModInfo::Ptr result = ModInfo::Ptr(
      new ModInfoForeign(modName, espName, bsaNames, modType, directoryStructure, pluginContainer));
  s_Collection.push_back(result);
  return result;
}

QString ModInfo::getContentTypeName(int contentType)
{
  switch (contentType) {
    case CONTENT_PLUGIN:    return tr("Plugins");
    case CONTENT_TEXTURE:   return tr("Textures");
    case CONTENT_MESH:      return tr("Meshes");
    case CONTENT_BSA:       return tr("Bethesda Archive");
    case CONTENT_INTERFACE: return tr("UI Changes");
    case CONTENT_SOUND:     return tr("Sound Effects");
    case CONTENT_SCRIPT:    return tr("Scripts");
    case CONTENT_SKSE:      return tr("Script Extender");
    case CONTENT_SKSEFILES: return tr("Script Extender Files");
    case CONTENT_SKYPROC:   return tr("SkyProc Tools");
    case CONTENT_MCM:       return tr("MCM Data");
    case CONTENT_INI:       return tr("INI files");
    case CONTENT_MODGROUP:  return tr("ModGroup files");

    default: throw MyException(tr("invalid content type: %1").arg(contentType));
  }
}

void ModInfo::createFromOverwrite(PluginContainer *pluginContainer,
                                  MOShared::DirectoryEntry **directoryStructure)
{
  QMutexLocker locker(&s_Mutex);

  s_Collection.push_back(ModInfo::Ptr(new ModInfoOverwrite(pluginContainer, directoryStructure)));
}

unsigned int ModInfo::getNumMods()
{
  QMutexLocker locker(&s_Mutex);
  return static_cast<unsigned int>(s_Collection.size());
}


ModInfo::Ptr ModInfo::getByIndex(unsigned int index)
{
  QMutexLocker locker(&s_Mutex);

  if (index >= s_Collection.size() && index != ULONG_MAX) {
    throw MyException(tr("invalid mod index: %1").arg(index));
  }
  if (index == ULONG_MAX) return s_Collection[ModInfo::getIndex("Overwrite")];
  return s_Collection[index];
}


std::vector<ModInfo::Ptr> ModInfo::getByModID(QString game, int modID)
{
  QMutexLocker locker(&s_Mutex);

  std::vector<unsigned int> match;
  for (auto iter : s_ModsByModID) {
    if (iter.first.second == modID) {
      if (iter.first.first.compare(game, Qt::CaseInsensitive) == 0) {
        match.insert(match.end(), iter.second.begin(), iter.second.end());
      }
    }
  }
  if (match.empty()) {
    return std::vector<ModInfo::Ptr>();
  }

  std::vector<ModInfo::Ptr> result;
  for (auto iter : match) {
    result.push_back(getByIndex(iter));
  }

  return result;
}


bool ModInfo::removeMod(unsigned int index)
{
  QMutexLocker locker(&s_Mutex);

  if (index >= s_Collection.size()) {
    throw MyException(tr("remove: invalid mod index %1").arg(index));
  }
  // update the indices first
  ModInfo::Ptr modInfo = s_Collection[index];
  s_ModsByName.erase(s_ModsByName.find(modInfo->name()));

  auto iter = s_ModsByModID.find(std::pair<QString, int>(modInfo->getGameName(), modInfo->getNexusID()));
  if (iter != s_ModsByModID.end()) {
    std::vector<unsigned int> indices = iter->second;
    indices.erase(std::remove(indices.begin(), indices.end(), index), indices.end());
    s_ModsByModID[std::pair<QString, int>(modInfo->getGameName(), modInfo->getNexusID())] = indices;
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
                             PluginContainer *pluginContainer,
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
      createFrom(pluginContainer, game, QDir(modIter.next()), directoryStructure);
    }
  }

  UnmanagedMods *unmanaged = game->feature<UnmanagedMods>();
  if (unmanaged != nullptr) {
    for (const QString &modName : unmanaged->mods(!displayForeign)) {
      ModInfo::EModType modType = game->DLCPlugins().contains(unmanaged->referenceFile(modName).fileName(), Qt::CaseInsensitive) ? ModInfo::EModType::MOD_DLC :
                         (game->CCPlugins().contains(unmanaged->referenceFile(modName).fileName(), Qt::CaseInsensitive) ? ModInfo::EModType::MOD_CC : ModInfo::EModType::MOD_DEFAULT);

      createFromPlugin(unmanaged->displayName(modName),
                       unmanaged->referenceFile(modName).absoluteFilePath(),
                       unmanaged->secondaryFiles(modName),
                       modType,
                       directoryStructure,
                       pluginContainer);
    }
  }

  createFromOverwrite(pluginContainer, directoryStructure);

  std::sort(s_Collection.begin(), s_Collection.end(), ModInfo::ByName);

  updateIndices();
}


void ModInfo::updateIndices()
{
  s_ModsByName.clear();
  s_ModsByModID.clear();

  for (unsigned int i = 0; i < s_Collection.size(); ++i) {
    QString modName = s_Collection[i]->internalName();
    QString game = s_Collection[i]->getGameName();
    int modID = s_Collection[i]->getNexusID();
    s_ModsByName[modName] = i;
    s_ModsByModID[std::pair<QString, int>(game, modID)].push_back(i);
  }
}


ModInfo::ModInfo(PluginContainer *pluginContainer)
  : m_Valid(false), m_PrimaryCategory(-1)
{
}


void ModInfo::checkAllForUpdate(PluginContainer *pluginContainer, QObject *receiver)
{
  QDateTime earliest = QDateTime::currentDateTimeUtc();
  QDateTime latest;
  std::set<QString> games;
  for (auto mod : s_Collection) {
    if (mod->canBeUpdated()) {
      if (mod->getLastNexusUpdate() < earliest)
        earliest = mod->getLastNexusUpdate();
      if (mod->getLastNexusUpdate() > latest)
        latest = mod->getLastNexusUpdate();
      games.insert(mod->getGameName().toLower());
    }
  }

  if (latest < QDateTime::currentDateTimeUtc().addDays(-30)) {
    std::set<std::pair<QString, int>> organizedGames;
    for (auto mod : s_Collection) {
      if (mod->canBeUpdated()) {
        organizedGames.insert(std::make_pair<QString, int>(mod->getGameName().toLower(), mod->getNexusID()));
      }
    }

    if (organizedGames.empty())
      qWarning("All of your mods have been checked recently. We restrict update checks to help preserve your available API requests.");

    for (auto game : organizedGames) {
      NexusInterface::instance(pluginContainer)->requestUpdates(game.second, receiver, QVariant(), game.first, QString());
    }
  } else if (earliest < QDateTime::currentDateTimeUtc().addDays(-30)) {
    for (auto gameName : games)
      NexusInterface::instance(pluginContainer)->requestUpdateInfo(gameName, NexusInterface::UpdatePeriod::MONTH, receiver, QVariant(true), QString());
  } else if (earliest < QDateTime::currentDateTimeUtc().addDays(-7)) {
    for (auto gameName : games)
      NexusInterface::instance(pluginContainer)->requestUpdateInfo(gameName, NexusInterface::UpdatePeriod::MONTH, receiver, QVariant(false), QString());
  } else if (earliest < QDateTime::currentDateTimeUtc().addDays(-1)) {
    for (auto gameName : games)
      NexusInterface::instance(pluginContainer)->requestUpdateInfo(gameName, NexusInterface::UpdatePeriod::WEEK, receiver, QVariant(false), QString());
  } else {
    for (auto gameName : games)
      NexusInterface::instance(pluginContainer)->requestUpdateInfo(gameName, NexusInterface::UpdatePeriod::DAY, receiver, QVariant(false), QString());
  }
}

std::set<QSharedPointer<ModInfo>> ModInfo::filteredMods(QString gameName, QVariantList updateData, bool addOldMods, bool markUpdated)
{
  std::set<QSharedPointer<ModInfo>> finalMods;
  for (QVariant result : updateData) {
    QVariantMap update = result.toMap();
    std::copy_if(s_Collection.begin(), s_Collection.end(), std::inserter(finalMods, finalMods.end()), [=](QSharedPointer<ModInfo> info) -> bool {
      if (info->getNexusID() == update["mod_id"].toInt() && info->getGameName().compare(gameName, Qt::CaseInsensitive) == 0)
        if (info->getLastNexusUpdate().addSecs(-3600) > QDateTime::fromSecsSinceEpoch(update["latest_file_update"].toInt(), Qt::UTC))
          return true;
      return false;
    });
  }

  if (addOldMods)
    for (auto mod : s_Collection)
      if (mod->getLastNexusUpdate() < QDateTime::currentDateTimeUtc().addDays(-30) && mod->getGameName().compare(gameName, Qt::CaseInsensitive) == 0)
        finalMods.insert(mod);

  if (markUpdated) {
    std::set<QSharedPointer<ModInfo>> updates;
    std::copy_if(s_Collection.begin(), s_Collection.end(), std::inserter(updates, updates.end()), [=](QSharedPointer<ModInfo> info) -> bool {
      if (info->getGameName().compare(gameName, Qt::CaseInsensitive) == 0 && info->canBeUpdated())
        return true;
      return false;
    });
    std::set<QSharedPointer<ModInfo>> diff;
    std::set_difference(updates.begin(), updates.end(), finalMods.begin(), finalMods.end(), std::inserter(diff, diff.end()));
    for (auto skipped : diff) {
      skipped->setLastNexusUpdate(QDateTime::currentDateTimeUtc());
    }
  }
  return finalMods;
}

void ModInfo::manualUpdateCheck(PluginContainer *pluginContainer, QObject *receiver, std::multimap<QString, int> IDs)
{
  std::vector<QSharedPointer<ModInfo>> mods;
  std::set<std::pair<QString, int>> organizedGames;

  for (auto ID : IDs) {
    for (auto matchedMod : getByModID(ID.first, ID.second)) {
      bool alreadyMatched = false;
      for (auto mod : mods) {
        if (mod == matchedMod) {
          alreadyMatched = true;
          break;
        }
      }
      if (!alreadyMatched)
        mods.push_back(matchedMod);
    }
  }
  mods.erase(
    std::remove_if(mods.begin(), mods.end(), [](ModInfo::Ptr mod) -> bool { return mod->getNexusID() <= 0; }),
    mods.end()
  );
  for (auto mod : mods) {
    mod->setLastNexusUpdate(QDateTime());
  }

  std::sort(mods.begin(), mods.end(), [](QSharedPointer<ModInfo> a, QSharedPointer<ModInfo> b) -> bool {
    return a->getLastNexusUpdate() < b->getLastNexusUpdate();
  });

  if (mods.size()) {
    qInfo("Checking updates for %d mods...", mods.size());

    for (auto mod : mods) {
      organizedGames.insert(std::make_pair<QString, int>(mod->getGameName().toLower(), mod->getNexusID()));
    }

    for (auto game : organizedGames) {
      NexusInterface::instance(pluginContainer)->requestUpdates(game.second, receiver, QVariant(), game.first, QString());
    }
  } else {
    qInfo("None of the selected mods can be updated.");
  }
}


void ModInfo::setVersion(const VersionInfo &version)
{
  m_Version = version;
}

void ModInfo::setPluginSelected(const bool &isSelected)
{
  m_PluginSelected = isSelected;
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
