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
#include "modinfoforeign.h"
#include "modinfooverwrite.h"
#include "modinforegular.h"
#include "modinfoseparator.h"

#include "categories.h"
#include "modinfodialog.h"
#include "modlist.h"
#include "organizercore.h"
#include "overwriteinfodialog.h"
#include "thread_utils.h"
#include "versioninfo.h"

#include "shared/appconfig.h"
#include <iplugingame.h>
#include <log.h>
#include <report.h>
#include <scriptextender.h>
#include <unmanagedmods.h>
#include <versioninfo.h>

#include <QApplication>
#include <QDirIterator>
#include <QMutexLocker>

using namespace MOBase;
using namespace MOShared;

const std::set<unsigned int> ModInfo::s_EmptySet;
std::vector<ModInfo::Ptr> ModInfo::s_Collection;
ModInfo::Ptr ModInfo::s_Overwrite;
std::map<QString, unsigned int, MOBase::FileNameComparator> ModInfo::s_ModsByName;
std::map<std::pair<QString, int>, std::vector<unsigned int>> ModInfo::s_ModsByModID;
int ModInfo::s_NextID;
QRecursiveMutex ModInfo::s_Mutex;

QString ModInfo::s_HiddenExt(".mohidden");

bool ModInfo::ByName(const ModInfo::Ptr& LHS, const ModInfo::Ptr& RHS)
{
  return QString::compare(LHS->name(), RHS->name(), Qt::CaseInsensitive) < 0;
}

bool ModInfo::isSeparatorName(const QString& name)
{
  static QRegularExpression separatorExp(
      QRegularExpression::anchoredPattern(".*_separator"));
  return separatorExp.match(name).hasMatch();
}

bool ModInfo::isBackupName(const QString& name)
{
  static QRegularExpression backupExp(
      QRegularExpression::anchoredPattern(".*backup[0-9]*"));
  return backupExp.match(name).hasMatch();
}

bool ModInfo::isRegularName(const QString& name)
{
  return !isSeparatorName(name) && !isBackupName(name);
}

ModInfo::Ptr ModInfo::createFrom(const QDir& dir, OrganizerCore& core)
{
  QMutexLocker locker(&s_Mutex);
  ModInfo::Ptr result;

  if (isBackupName(dir.dirName())) {
    result = ModInfo::Ptr(new ModInfoBackup(dir, core));
  } else if (isSeparatorName(dir.dirName())) {
    result = Ptr(new ModInfoSeparator(dir, core));
  } else {
    result = ModInfo::Ptr(new ModInfoRegular(dir, core));
  }
  result->m_Index = static_cast<int>(s_Collection.size());
  s_Collection.push_back(result);
  return result;
}

ModInfo::Ptr ModInfo::createFromPlugin(const QString& modName, const QString& espName,
                                       const QStringList& bsaNames,
                                       ModInfo::EModType modType, OrganizerCore& core)
{
  QMutexLocker locker(&s_Mutex);
  ModInfo::Ptr result =
      ModInfo::Ptr(new ModInfoForeign(modName, espName, bsaNames, modType, core));
  result->m_Index = static_cast<int>(s_Collection.size());
  s_Collection.push_back(result);
  return result;
}

ModInfo::Ptr ModInfo::createFromOverwrite(OrganizerCore& core)
{
  QMutexLocker locker(&s_Mutex);
  ModInfo::Ptr overwrite = ModInfo::Ptr(new ModInfoOverwrite(core));
  overwrite->m_Index     = static_cast<int>(s_Collection.size());
  s_Collection.push_back(overwrite);
  return overwrite;
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
  if (index == ULONG_MAX)
    return s_Collection[ModInfo::getIndex("Overwrite")];
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

ModInfo::Ptr ModInfo::getByName(const QString& name)
{
  QMutexLocker locker(&s_Mutex);

  return s_Collection[ModInfo::getIndex(name)];
}

bool ModInfo::removeMod(unsigned int index)
{
  QMutexLocker locker(&s_Mutex);

  if (index >= s_Collection.size()) {
    throw Exception(tr("remove: invalid mod index %1").arg(index));
  }

  ModInfo::Ptr modInfo = s_Collection[index];

  // remove the actual mod (this is the most likely to fail so we do this first)
  if (modInfo->isRegular()) {
    if (!shellDelete(QStringList(modInfo->absolutePath()), true)) {
      reportError(
          tr("remove: failed to delete mod '%1' directory").arg(modInfo->name()));
      return false;
    }
  }

  // update the indices
  s_ModsByName.erase(s_ModsByName.find(modInfo->name()));

  auto iter = s_ModsByModID.find(
      std::pair<QString, int>(modInfo->gameName(), modInfo->nexusId()));
  if (iter != s_ModsByModID.end()) {
    std::vector<unsigned int> indices = iter->second;
    indices.erase(std::remove(indices.begin(), indices.end(), index), indices.end());
    s_ModsByModID[std::pair<QString, int>(modInfo->gameName(), modInfo->nexusId())] =
        indices;
  }

  // finally, remove the mod from the collection
  s_Collection.erase(s_Collection.begin() + index);

  // and update the indices
  updateIndices();
  return true;
}

unsigned int ModInfo::getIndex(const QString& name)
{
  QMutexLocker locker(&s_Mutex);

  std::map<QString, unsigned int>::iterator iter = s_ModsByName.find(name);
  if (iter == s_ModsByName.end()) {
    return UINT_MAX;
  }

  return iter->second;
}

unsigned int ModInfo::findMod(const boost::function<bool(ModInfo::Ptr)>& filter)
{
  for (unsigned int i = 0U; i < s_Collection.size(); ++i) {
    if (filter(s_Collection[i])) {
      return i;
    }
  }
  return UINT_MAX;
}

void ModInfo::updateFromDisc(const QString& modsDirectory, OrganizerCore& core,
                             bool displayForeign, std::size_t refreshThreadCount)
{
  TimeThis tt("ModInfo::updateFromDisc()");

  QMutexLocker lock(&s_Mutex);
  s_Collection.clear();
  s_NextID    = 0;
  s_Overwrite = nullptr;

  {  // list all directories in the mod directory and make a mod out of each
    QDir mods(QDir::fromNativeSeparators(modsDirectory));
    mods.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    QDirIterator modIter(mods);
    while (modIter.hasNext()) {
      createFrom(QDir(modIter.next()), core);
    }
  }

  auto* game               = core.managedGame();
  UnmanagedMods* unmanaged = game->feature<UnmanagedMods>();
  if (unmanaged != nullptr) {
    for (const QString& modName : unmanaged->mods(!displayForeign)) {
      ModInfo::EModType modType =
          game->DLCPlugins().contains(unmanaged->referenceFile(modName).fileName(),
                                      Qt::CaseInsensitive)
              ? ModInfo::EModType::MOD_DLC
              : (game->CCPlugins().contains(
                     unmanaged->referenceFile(modName).fileName(), Qt::CaseInsensitive)
                     ? ModInfo::EModType::MOD_CC
                     : ModInfo::EModType::MOD_DEFAULT);

      createFromPlugin(unmanaged->displayName(modName),
                       unmanaged->referenceFile(modName).absoluteFilePath(),
                       unmanaged->secondaryFiles(modName), modType, core);
    }
  }

  s_Overwrite = createFromOverwrite(core);

  std::sort(s_Collection.begin(), s_Collection.end(), ModInfo::ByName);

  parallelMap(std::begin(s_Collection), std::end(s_Collection), &ModInfo::prefetch,
              refreshThreadCount);

  updateIndices();
}

void ModInfo::updateIndices()
{
  s_ModsByName.clear();
  s_ModsByModID.clear();

  for (unsigned int i = 0; i < s_Collection.size(); ++i) {
    QString modName          = s_Collection[i]->internalName();
    QString game             = s_Collection[i]->gameName();
    int modID                = s_Collection[i]->nexusId();
    s_Collection[i]->m_Index = i;
    s_ModsByName[modName]    = i;
    s_ModsByModID[std::pair<QString, int>(game, modID)].push_back(i);
  }
}

ModInfo::ModInfo(OrganizerCore& core) : m_PrimaryCategory(-1), m_Core(core) {}

bool ModInfo::checkAllForUpdate(PluginManager* pluginManager, QObject* receiver)
{
  bool updatesAvailable = true;

  QDateTime earliest = QDateTime::currentDateTimeUtc();
  QDateTime latest   = QDateTime::fromMSecsSinceEpoch(0);
  std::set<QString> games;
  for (auto mod : s_Collection) {
    if (mod->canBeUpdated()) {
      if (mod->getLastNexusUpdate() < earliest)
        earliest = mod->getLastNexusUpdate();
      if (mod->getLastNexusUpdate() > latest)
        latest = mod->getLastNexusUpdate();
      games.insert(mod->gameName().toLower());
    }
  }

  // Detect invalid source games
  for (auto itr = games.begin(); itr != games.end();) {
    auto gamePlugins        = pluginManager->plugins<IPluginGame>();
    IPluginGame* gamePlugin = qApp->property("managed_game").value<IPluginGame*>();
    for (auto plugin : gamePlugins) {
      if (plugin != nullptr &&
          plugin->gameShortName().compare(*itr, Qt::CaseInsensitive) == 0) {
        gamePlugin = plugin;
        break;
      }
    }
    if (gamePlugin != nullptr && gamePlugin->gameNexusName().isEmpty()) {
      log::warn("{}", tr("The update check has found a mod with a Nexus ID and source "
                         "game of %1, but this game is not a valid Nexus source.")
                          .arg(gamePlugin->gameName()));
      itr = games.erase(itr);
    } else {
      ++itr;
    }
  }

  if (latest < QDateTime::currentDateTimeUtc().addMonths(-1)) {
    std::set<std::pair<QString, int>> organizedGames;
    for (auto mod : s_Collection) {
      if (mod->canBeUpdated() &&
          mod->getLastNexusUpdate() < QDateTime::currentDateTimeUtc().addMonths(-1)) {
        organizedGames.insert(
            std::make_pair<QString, int>(mod->gameName().toLower(), mod->nexusId()));
      }
    }

    if (organizedGames.empty()) {
      log::warn("{}",
                tr("All of your mods have been checked recently. We restrict update "
                   "checks to help preserve your available API requests."));
      updatesAvailable = false;
    } else {
      log::info("{}", tr("You have mods that haven't been checked within the last "
                         "month using the new API. These mods must be checked before "
                         "we can use the bulk update API. "
                         "This will consume significantly more API requests than "
                         "usual. You will need to rerun the update check once complete "
                         "in order to parse the remaining mods."));
    }

    for (auto game : organizedGames)
      NexusInterface::instance().requestUpdates(game.second, receiver, QVariant(),
                                                game.first, QString());
  } else if (earliest < QDateTime::currentDateTimeUtc().addMonths(-1)) {
    for (auto gameName : games)
      NexusInterface::instance().requestUpdateInfo(gameName,
                                                   NexusInterface::UpdatePeriod::MONTH,
                                                   receiver, QVariant(true), QString());
  } else if (earliest < QDateTime::currentDateTimeUtc().addDays(-7)) {
    for (auto gameName : games)
      NexusInterface::instance().requestUpdateInfo(
          gameName, NexusInterface::UpdatePeriod::MONTH, receiver, QVariant(false),
          QString());
  } else if (earliest < QDateTime::currentDateTimeUtc().addDays(-1)) {
    for (auto gameName : games)
      NexusInterface::instance().requestUpdateInfo(
          gameName, NexusInterface::UpdatePeriod::WEEK, receiver, QVariant(false),
          QString());
  } else {
    for (auto gameName : games)
      NexusInterface::instance().requestUpdateInfo(
          gameName, NexusInterface::UpdatePeriod::DAY, receiver, QVariant(false),
          QString());
  }

  return updatesAvailable;
}

std::set<QSharedPointer<ModInfo>> ModInfo::filteredMods(QString gameName,
                                                        QVariantList updateData,
                                                        bool addOldMods,
                                                        bool markUpdated)
{
  std::set<QSharedPointer<ModInfo>> finalMods;
  for (QVariant result : updateData) {
    QVariantMap update = result.toMap();
    std::copy_if(s_Collection.begin(), s_Collection.end(),
                 std::inserter(finalMods, finalMods.end()),
                 [=](QSharedPointer<ModInfo> info) -> bool {
                   if (info->nexusId() == update["mod_id"].toInt() &&
                       info->gameName().compare(gameName, Qt::CaseInsensitive) == 0)
                     if (info->getLastNexusUpdate().addSecs(-3600) <
                         QDateTime::fromSecsSinceEpoch(
                             update["latest_file_update"].toInt(), Qt::UTC))
                       return true;
                   return false;
                 });
  }

  if (addOldMods)
    for (auto mod : s_Collection)
      if (mod->getLastNexusUpdate() < QDateTime::currentDateTimeUtc().addMonths(-1) &&
          mod->gameName().compare(gameName, Qt::CaseInsensitive) == 0)
        finalMods.insert(mod);

  if (markUpdated) {
    std::set<QSharedPointer<ModInfo>> updates;
    std::copy_if(s_Collection.begin(), s_Collection.end(),
                 std::inserter(updates, updates.end()),
                 [=](QSharedPointer<ModInfo> info) -> bool {
                   if (info->gameName().compare(gameName, Qt::CaseInsensitive) == 0 &&
                       info->canBeUpdated())
                     return true;
                   return false;
                 });
    std::set<QSharedPointer<ModInfo>> diff;
    std::set_difference(updates.begin(), updates.end(), finalMods.begin(),
                        finalMods.end(), std::inserter(diff, diff.end()));
    for (auto skipped : diff) {
      skipped->setLastNexusUpdate(QDateTime::currentDateTimeUtc());
    }
  }
  return finalMods;
}

void ModInfo::manualUpdateCheck(QObject* receiver, std::multimap<QString, int> IDs)
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
  mods.erase(std::remove_if(mods.begin(), mods.end(),
                            [](ModInfo::Ptr mod) -> bool {
                              return mod->nexusId() <= 0;
                            }),
             mods.end());
  for (auto mod : mods) {
    mod->setLastNexusUpdate(QDateTime());
  }

  std::sort(mods.begin(), mods.end(),
            [](QSharedPointer<ModInfo> a, QSharedPointer<ModInfo> b) -> bool {
              return a->getLastNexusUpdate() < b->getLastNexusUpdate();
            });

  if (mods.size()) {
    log::info("Checking updates for {} mods...", mods.size());

    for (auto mod : mods) {
      organizedGames.insert(
          std::make_pair<QString, int>(mod->gameName().toLower(), mod->nexusId()));
    }

    for (auto game : organizedGames) {
      NexusInterface::instance().requestUpdates(game.second, receiver, QVariant(),
                                                game.first, QString());
    }
  } else {
    log::info("None of the selected mods can be updated.");
  }
}

void ModInfo::setVersion(const VersionInfo& version)
{
  m_Version = version;
}

void ModInfo::setPluginSelected(const bool& isSelected)
{
  m_PluginSelected = isSelected;
}

void ModInfo::addCategory(const QString& categoryName)
{
  int id = CategoryFactory::instance().getCategoryID(categoryName);
  if (id == -1) {
    id = CategoryFactory::instance().addCategory(
        categoryName, std::vector<CategoryFactory::NexusCategory>(), 0);
  }
  setCategory(id, true);
}

bool ModInfo::removeCategory(const QString& categoryName)
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

QStringList ModInfo::categories() const
{
  QStringList result;

  CategoryFactory& catFac = CategoryFactory::instance();
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

bool ModInfo::hasAnyOfTheseFlags(std::vector<ModInfo::EFlag> flags) const
{
  std::vector<EFlag> modFlags = getFlags();
  for (auto modFlag : modFlags) {
    for (auto flag : flags) {
      if (modFlag == flag) {
        return true;
      }
    }
  }
  return false;
}

bool ModInfo::categorySet(int categoryID) const
{
  for (std::set<int>::const_iterator iter = m_Categories.begin();
       iter != m_Categories.end(); ++iter) {
    if ((*iter == categoryID) ||
        (CategoryFactory::instance().isDescendantOf(*iter, categoryID))) {
      return true;
    }
  }

  return false;
}

QUrl ModInfo::parseCustomURL() const
{
  if (!hasCustomURL() || url().isEmpty()) {
    return {};
  }

  const auto url = QUrl::fromUserInput(this->url());

  if (!url.isValid()) {
    log::error("mod '{}' has an invalid custom url '{}'", name(), this->url());
    return {};
  }

  return url;
}
