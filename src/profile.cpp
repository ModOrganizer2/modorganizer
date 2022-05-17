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

#include "profile.h"

#include "filesystemutilities.h"
#include "modinfo.h"
#include "modinfoforeign.h"
#include "registry.h"
#include "settings.h"
#include "shared/appconfig.h"
#include "shared/util.h"
#include <bsainvalidation.h>
#include <dataarchives.h>
#include <iplugingame.h>
#include <questionboxmemory.h>
#include <report.h>
#include <safewritefile.h>

#include <QApplication>
#include <QBuffer>
#include <QDirIterator>
#include <QFile>      // for QFile
#include <QFlags>     // for operator|, QFlags
#include <QIODevice>  // for QIODevice, etc
#include <QMessageBox>
#include <QScopedArrayPointer>
#include <QStringList>  // for QStringList
#include <QtGlobal>     // for qUtf8Printable

#include <Windows.h>

#include <assert.h>  // for assert
#include <limits.h>  // for UINT_MAX, INT_MAX, etc
#include <stddef.h>  // for size_t
#include <string.h>  // for wcslen

#include <algorithm>  // for max, min
#include <exception>  // for exception
#include <functional>
#include <set>  // for set
#include <stdexcept>
#include <utility>  // for find

using namespace MOBase;
using namespace MOShared;

void Profile::touchFile(QString fileName)
{
  QFile modList(m_Directory.filePath(fileName));
  if (!modList.open(QIODevice::ReadWrite)) {
    throw std::runtime_error(QObject::tr("failed to create %1")
                                 .arg(m_Directory.filePath(fileName))
                                 .toUtf8()
                                 .constData());
  }
}

Profile::Profile(const QString& name, IPluginGame const* gamePlugin,
                 bool useDefaultSettings)
    : m_ModListWriter(std::bind(&Profile::doWriteModlist, this)),
      m_GamePlugin(gamePlugin)
{
  QString profilesDir = Settings::instance().paths().profiles();
  QDir profileBase(profilesDir);
  QString fixedName = name;
  if (!fixDirectoryName(fixedName)) {
    throw MyException(tr("invalid profile name: %1").arg(qUtf8Printable(name)));
  }

  if (!profileBase.exists() || !profileBase.mkdir(fixedName)) {
    throw MyException(tr("failed to create %1").arg(fixedName).toUtf8().constData());
  }
  QString fullPath = profilesDir + "/" + fixedName;
  m_Directory      = QDir(fullPath);
  m_Settings =
      new QSettings(m_Directory.absoluteFilePath("settings.ini"), QSettings::IniFormat);

  try {
    // create files. Needs to happen after m_Directory was set!
    touchFile("modlist.txt");
    touchFile("archives.txt");

    IPluginGame::ProfileSettings settings =
        IPluginGame::CONFIGURATION | IPluginGame::MODS | IPluginGame::SAVEGAMES;

    if (useDefaultSettings) {
      settings |= IPluginGame::PREFER_DEFAULTS;
    }

    gamePlugin->initializeProfile(fullPath, settings);
    findProfileSettings();
  } catch (...) {
    // clean up in case of an error
    shellDelete(QStringList(profileBase.absoluteFilePath(fixedName)));
    throw;
  }
  refreshModStatus();
}

Profile::Profile(const QDir& directory, IPluginGame const* gamePlugin)
    : m_Directory(directory), m_GamePlugin(gamePlugin),
      m_ModListWriter(std::bind(&Profile::doWriteModlist, this))
{
  assert(gamePlugin != nullptr);

  m_Settings =
      new QSettings(directory.absoluteFilePath("settings.ini"), QSettings::IniFormat);
  findProfileSettings();

  if (!QFile::exists(m_Directory.filePath("modlist.txt"))) {
    log::warn("missing modlist.txt in {}", directory.path());
    touchFile(m_Directory.filePath("modlist.txt"));
  }

  IPluginGame::ProfileSettings settings = IPluginGame::MODS | IPluginGame::SAVEGAMES;
  gamePlugin->initializeProfile(directory, settings);

  refreshModStatus();
}

Profile::Profile(const Profile& reference)
    : m_Directory(reference.m_Directory),
      m_ModListWriter(std::bind(&Profile::doWriteModlist, this)),
      m_GamePlugin(reference.m_GamePlugin)

{
  m_Settings =
      new QSettings(m_Directory.absoluteFilePath("settings.ini"), QSettings::IniFormat);
  findProfileSettings();
  refreshModStatus();
}

Profile::~Profile()
{
  delete m_Settings;
  m_ModListWriter.writeImmediately(true);
}

void Profile::findProfileSettings()
{
  if (setting("", "LocalSaves") == QVariant()) {
    if (m_Directory.exists("saves")) {
      storeSetting("", "LocalSaves", true);
    } else {
      if (m_Directory.exists("_saves")) {
        m_Directory.rename("_saves", "saves");
      }
      storeSetting("", "LocalSaves", false);
    }
  }

  if (setting("", "LocalSettings") == QVariant()) {
    QString backupFile = getIniFileName() + "_";
    if (m_Directory.exists(backupFile)) {
      storeSetting("", "LocalSettings", false);
      m_Directory.rename(backupFile, getIniFileName());
    } else {
      storeSetting("", "LocalSettings", true);
    }
  }

  if (setting("", "AutomaticArchiveInvalidation") == QVariant()) {
    BSAInvalidation* invalidation = m_GamePlugin->feature<BSAInvalidation>();
    DataArchives* dataArchives    = m_GamePlugin->feature<DataArchives>();
    bool found                    = false;
    if ((invalidation != nullptr) && (dataArchives != nullptr)) {
      for (const QString& archive : dataArchives->archives(this)) {
        if (invalidation->isInvalidationBSA(archive)) {
          storeSetting("", "AutomaticArchiveInvalidation", true);
          found = true;
          break;
        }
      }
    }
    if (!found) {
      storeSetting("", "AutomaticArchiveInvalidation", false);
    }
  }
}

bool Profile::exists() const
{
  return m_Directory.exists();
}

void Profile::writeModlist()
{
  m_ModListWriter.write();
}

void Profile::writeModlistNow(bool onlyIfPending)
{
  m_ModListWriter.writeImmediately(onlyIfPending);
}

void Profile::cancelModlistWrite()
{
  m_ModListWriter.cancel();
}

void Profile::doWriteModlist()
{
  if (!m_Directory.exists())
    return;

  try {
    QString fileName = getModlistFileName();
    SafeWriteFile file(fileName);

    file->write(QString("# This file was automatically generated by Mod Organizer.\r\n")
                    .toUtf8());
    if (m_ModStatus.empty()) {
      return;
    }

    for (auto iter = m_ModIndexByPriority.crbegin();
         iter != m_ModIndexByPriority.crend(); iter++) {
      // the priority order was inverted on load so it has to be inverted again
      const auto index     = iter->second;
      ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
      if (!modInfo->hasAutomaticPriority()) {
        if (modInfo->isForeign()) {
          file->write("*");
        } else if (m_ModStatus[index].m_Enabled) {
          file->write("+");
        } else {
          file->write("-");
        }
        file->write(modInfo->name().toUtf8());
        file->write("\r\n");
      }
    }

    file.commitIfDifferent(m_LastModlistHash);
  } catch (const std::exception& e) {
    reportError(tr("failed to write mod list: %1").arg(e.what()));
    return;
  }
}

void Profile::createTweakedIniFile()
{
  QString tweakedIni = m_Directory.absoluteFilePath("initweaks.ini");

  if (QFile::exists(tweakedIni) && !shellDeleteQuiet(tweakedIni)) {
    const auto e = GetLastError();
    reportError(tr("failed to update tweaked ini file, wrong settings may be used: %1")
                    .arg(QString::fromStdWString(formatSystemMessage(e))));
    return;
  }

  for (const auto& [priority, index] : m_ModIndexByPriority) {
    if (m_ModStatus[index].m_Enabled) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
      mergeTweaks(modInfo, tweakedIni);
    }
  }

  mergeTweak(getProfileTweaks(), tweakedIni);

  bool error = false;
  if (!MOBase::WriteRegistryValue(L"Archive", L"bInvalidateOlderFiles", L"1",
                                  ToWString(tweakedIni).c_str())) {
    error = true;
  }

  if (error) {
    const auto e = ::GetLastError();
    reportError(tr("failed to create tweaked ini: %1")
                    .arg(QString::fromStdWString(formatSystemMessage(e))));
  }
}

// static
void Profile::renameModInAllProfiles(const QString& oldName, const QString& newName)
{
  QDir profilesDir(Settings::instance().paths().profiles());
  profilesDir.setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);
  QDirIterator profileIter(profilesDir);
  while (profileIter.hasNext()) {
    profileIter.next();
    QFile modList(profileIter.filePath() + "/modlist.txt");
    if (modList.exists())
      renameModInList(modList, oldName, newName);
    else
      log::warn("Profile has no modlist.txt: {}", profileIter.filePath());
  }
}

// static
void Profile::renameModInList(QFile& modList, const QString& oldName,
                              const QString& newName)
{
  if (!modList.open(QIODevice::ReadOnly)) {
    reportError(tr("failed to open %1").arg(modList.fileName()));
    return;
  }

  QBuffer outBuffer;
  outBuffer.open(QIODevice::WriteOnly);

  int renamed = 0;
  while (!modList.atEnd()) {
    QByteArray line = modList.readLine();

    if (line.length() == 0) {
      // ignore empty lines
      log::warn("mod list contained invalid data: empty line");
      continue;
    }

    char spec = line.at(0);
    if (spec == '#') {
      // don't touch comments
      outBuffer.write(line);
      continue;
    }

    QString modName = QString::fromUtf8(line).mid(1).trimmed();

    if (modName.isEmpty()) {
      // file broken?
      log::warn("mod list contained invalid data: missing mod name");
      continue;
    }

    outBuffer.write(QByteArray(1, spec));
    if (modName == oldName) {
      modName = newName;
      ++renamed;
    }
    outBuffer.write(qUtf8Printable(modName));
    outBuffer.write("\r\n");
  }
  modList.close();

  if (renamed) {
    modList.open(QIODevice::WriteOnly);
    modList.write(outBuffer.buffer());
    modList.close();
  }

  if (renamed)
    log::debug("Renamed {} \"{}\" mod to \"{}\" in {}", renamed, oldName, newName,
               modList.fileName());
}

void Profile::refreshModStatus()
{
  // this function refreshes mod status (enabled/disabled) and priority
  // using the profile mod list file and the mods in the mods folder using
  // the following steps
  //
  // 1) the mod list file is read and mods status/priority are updated by
  //    considering the content of the file (for status) and the order (for
  //    priority), missing or invalid mods are discarded (with a warning)
  // 2) the priority are reversed to match the plugin list (highest wins)
  //    since the mod list is written in reverse order
  // 3) at the same time, new mods (not in the mod list file) are added
  //    - foreign mods are given low priority (below 0)
  //    - regular mods are given high priority (above mods from the mod list)
  // 4) the priority are shifted to ensure that the minimum priority is 0
  // 5) the priority of backups are computed such that the first backup is
  //    above all regular mods
  //
  // in the context of the profile, "regular mods" means a mod whose priority
  // can be set by the user (i.e. not a backup or overwrite)
  //
  // this method ensures that the mods priority is as follow
  //
  //   0   mod1
  //   1   mod2
  //       ...
  //   K-1 modK (K = m_NumRegularMods)
  //   K   backup1
  //   K+1 backup2
  //       ...
  //   N-2 backupX
  //   N-1 overwrite (N = number of mods)
  //

  writeModlistNow(true);  // if there are pending changes write them first

  QFile file(getModlistFileName());
  if (!file.open(QIODevice::ReadOnly)) {
    throw MyException(
        tr("\"%1\" is missing or inaccessible").arg(getModlistFileName()));
  }

  bool modStatusModified = false;
  m_ModStatus.clear();
  m_ModStatus.resize(ModInfo::getNumMods());

  std::set<QString> namesRead;

  bool warnAboutOverwrite = false;

  // load mods from file and update enabled state and priority for them
  int index = 0;
  while (!file.atEnd()) {
    QByteArray line = file.readLine().trimmed();

    // find the mod name and the enabled status
    bool enabled = true;
    QString modName;
    if (line.length() == 0) {
      // empty line
      continue;
    } else if (line.at(0) == '#') {
      // comment line
      continue;
    } else if (line.at(0) == '-') {
      enabled = false;
      modName = QString::fromUtf8(line.mid(1).trimmed().constData());
    } else if (line.at(0) == '+' || line.at(0) == '*') {
      modName = QString::fromUtf8(line.mid(1).trimmed().constData());
    } else {
      modName = QString::fromUtf8(line.trimmed().constData());
    }

    if (modName.isEmpty()) {
      continue;
    }

    if (modName.compare("overwrite", Qt::CaseInsensitive) == 0) {
      warnAboutOverwrite = true;
    }

    // check if the name was already read
    if (namesRead.find(modName) != namesRead.end()) {
      continue;
    }
    namesRead.insert(modName);

    unsigned int modIndex = ModInfo::getIndex(modName);
    if (modIndex == UINT_MAX) {
      log::debug("mod not found: \"{}\" (profile \"{}\")", modName, m_Directory.path());
      // need to rewrite the modlist to fix this
      modStatusModified = true;
      continue;
    }

    // find the mod and check that this is a regular mod (and not a backup)
    ModInfo::Ptr info = ModInfo::getByIndex(modIndex);
    if (modIndex < m_ModStatus.size() && !info->hasAutomaticPriority()) {
      m_ModStatus[modIndex].m_Enabled = enabled;
      if (m_ModStatus[modIndex].m_Priority == -1) {
        if (static_cast<size_t>(index) >= m_ModStatus.size()) {
          throw Exception(tr("invalid mod index: %1").arg(index));
        }
        m_ModStatus[modIndex].m_Priority = index++;
      }
    } else {
      log::warn("no mod state for \"{}\" (profile \"{}\")", modName,
                m_Directory.path());
      // need to rewrite the modlist to fix this
      modStatusModified = true;
    }

  }  // while (!file.atEnd())

  file.close();

  const int numKnownMods = index;
  int topInsert          = 0;

  // invert priority order to match that of the pluginlist, also
  // give priorities to mods not referenced in the profile and
  // count the number of regular mods
  m_NumRegularMods = 0;
  for (size_t i = 0; i < m_ModStatus.size(); ++i) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(static_cast<int>(i));
    if (modInfo->alwaysEnabled()) {
      m_ModStatus[i].m_Enabled = true;
    }

    if (modInfo->isOverwrite()) {
      m_ModStatus[i].m_Priority = m_ModStatus.size() - 1;
      continue;
    }

    if (m_ModStatus[i].m_Priority != -1) {
      m_ModStatus[i].m_Priority = numKnownMods - m_ModStatus[i].m_Priority - 1;
      ++m_NumRegularMods;
    } else {
      if (static_cast<size_t>(index) >= m_ModStatus.size()) {
        throw Exception(tr("invalid mod index: %1").arg(index));
      }

      // skip backups on purpose to avoid inserting backups in-between
      // regular mods
      if (modInfo->isForeign()) {
        m_ModStatus[i].m_Priority = --topInsert;
        ++m_NumRegularMods;
      } else if (!modInfo->isBackup()) {
        m_ModStatus[i].m_Priority = index++;
        ++m_NumRegularMods;
      }

      // also, mark the mod-list as changed
      modStatusModified = true;
    }
  }

  // to support insertion of new mods at the top we may now have mods with negative
  // priority, so shift them all up to align priority with 0
  if (topInsert < 0) {
    int offset = topInsert * -1;
    for (size_t i = 0; i < m_ModStatus.size(); ++i) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(static_cast<unsigned int>(i));
      if (modInfo->hasAutomaticPriority()) {
        continue;
      }

      m_ModStatus[i].m_Priority += offset;
    }
  }

  // set the backups priority
  int backupPriority = m_NumRegularMods;
  for (size_t i = 0; i < m_ModStatus.size(); ++i) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(static_cast<unsigned int>(i));
    if (modInfo->isBackup()) {
      m_ModStatus[i].m_Priority = backupPriority++;
    }
  }

  updateIndices();

  // User has a mod named some variation of "overwrite".  Tell them about it.
  if (warnAboutOverwrite) {
    reportError(tr("A mod named \"overwrite\" was detected, disabled, and moved to the "
                   "highest priority on the mod list. "
                   "You may want to rename this mod and enable it again."));
    // also, mark the mod-list as changed
    modStatusModified = true;
  }

  if (modStatusModified) {
    m_ModListWriter.write();
  }
}

void Profile::dumpModStatus() const
{
  for (unsigned int i = 0; i < m_ModStatus.size(); ++i) {
    ModInfo::Ptr info = ModInfo::getByIndex(i);
    log::warn("{}: {} - {} ({})", i, info->name(), m_ModStatus[i].m_Priority,
              m_ModStatus[i].m_Enabled ? "enabled" : "disabled");
  }
}

void Profile::updateIndices()
{
  m_ModIndexByPriority.clear();
  for (unsigned int i = 0; i < m_ModStatus.size(); ++i) {
    int priority                   = m_ModStatus[i].m_Priority;
    m_ModIndexByPriority[priority] = i;
  }
}

std::vector<std::tuple<QString, QString, int>> Profile::getActiveMods()
{
  std::vector<std::tuple<QString, QString, int>> result;
  for (const auto& [priority, index] : m_ModIndexByPriority) {
    if (m_ModStatus[index].m_Enabled) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
      result.push_back(std::make_tuple(modInfo->internalName(), modInfo->absolutePath(),
                                       m_ModStatus[index].m_Priority));
    }
  }

  return result;
}

void Profile::setModEnabled(unsigned int index, bool enabled)
{
  if (index >= m_ModStatus.size()) {
    throw MyException(tr("invalid mod index: %1").arg(index));
  }

  ModInfo::Ptr modInfo = ModInfo::getByIndex(index);

  // we could quit in the following case, this shouldn't be a change anyway,
  // but at least this allows the situation to be fixed in case of an error
  if (modInfo->alwaysEnabled()) {
    enabled = true;
  }
  if (modInfo->alwaysDisabled()) {
    enabled = false;
  }

  if (enabled != m_ModStatus[index].m_Enabled) {
    m_ModStatus[index].m_Enabled = enabled;
    emit modStatusChanged(index);
  }
}

void Profile::setModsEnabled(const QList<unsigned int>& modsToEnable,
                             const QList<unsigned int>& modsToDisable)
{
  QList<unsigned int> dirtyMods;
  for (auto idx : modsToEnable) {
    if (idx >= m_ModStatus.size()) {
      log::error("invalid mod index: {}", idx);
      continue;
    }
    if (ModInfo::getByIndex(idx)->alwaysDisabled()) {
      continue;
    }
    if (!m_ModStatus[idx].m_Enabled) {
      m_ModStatus[idx].m_Enabled = true;
      dirtyMods.append(idx);
    }
  }
  for (auto idx : modsToDisable) {
    if (idx >= m_ModStatus.size()) {
      log::error("invalid mod index: {}", idx);
      continue;
    }
    if (ModInfo::getByIndex(idx)->alwaysEnabled()) {
      continue;
    }
    if (m_ModStatus[idx].m_Enabled) {
      m_ModStatus[idx].m_Enabled = false;
      dirtyMods.append(idx);
    }
  }
  if (!dirtyMods.isEmpty()) {
    emit modStatusChanged(dirtyMods);
  }
}

bool Profile::modEnabled(unsigned int index) const
{
  if (index >= m_ModStatus.size()) {
    throw MyException(tr("invalid mod index: %1").arg(index));
  }

  return m_ModStatus[index].m_Enabled;
}

int Profile::getModPriority(unsigned int index) const
{
  if (index >= m_ModStatus.size()) {
    throw MyException(tr("invalid mod index: %1").arg(index));
  }

  return m_ModStatus[index].m_Priority;
}

bool Profile::setModPriority(unsigned int index, int& newPriority)
{
  if (ModInfo::getByIndex(index)->hasAutomaticPriority()) {
    // can't change priority of overwrite/backups
    return false;
  }

  newPriority = std::clamp(newPriority, 0, static_cast<int>(m_NumRegularMods) - 1);

  int oldPriority  = m_ModStatus.at(index).m_Priority;
  int lastPriority = INT_MIN;

  if (newPriority == oldPriority) {
    // nothing to do
    return false;
  }

  for (const auto& [priority, index] : m_ModIndexByPriority) {
    if (newPriority < oldPriority && priority >= newPriority &&
        priority < oldPriority) {
      m_ModStatus.at(index).m_Priority += 1;
    } else if (newPriority > oldPriority && priority <= newPriority &&
               priority > oldPriority) {
      m_ModStatus.at(index).m_Priority -= 1;
    }
    lastPriority = std::max(lastPriority, priority);
  }

  newPriority                      = std::min(newPriority, lastPriority);
  m_ModStatus.at(index).m_Priority = std::min(newPriority, lastPriority);

  updateIndices();
  m_ModListWriter.write();

  return true;
}

Profile* Profile::createPtrFrom(const QString& name, const Profile& reference,
                                MOBase::IPluginGame const* gamePlugin)
{
  QString profileDirectory = Settings::instance().paths().profiles() + "/" + name;
  reference.copyFilesTo(profileDirectory);
  return new Profile(QDir(profileDirectory), gamePlugin);
}

void Profile::copyFilesTo(QString& target) const
{
  copyDir(m_Directory.absolutePath(), target, false);
}

std::vector<std::wstring> Profile::splitDZString(const wchar_t* buffer) const
{
  std::vector<std::wstring> result;
  const wchar_t* pos = buffer;
  size_t length      = wcslen(pos);
  while (length != 0U) {
    result.push_back(pos);
    pos += length + 1;
    length = wcslen(pos);
  }
  return result;
}

void Profile::mergeTweak(const QString& tweakName, const QString& tweakedIni) const
{
  static const int bufferSize = 32768;

  std::wstring tweakNameW  = ToWString(tweakName);
  std::wstring tweakedIniW = ToWString(tweakedIni);
  QScopedArrayPointer<wchar_t> buffer(new wchar_t[bufferSize]);

  // retrieve a list of sections
  DWORD size =
      ::GetPrivateProfileSectionNamesW(buffer.data(), bufferSize, tweakNameW.c_str());

  if (size == bufferSize - 2) {
    // unfortunately there is no good way to find the required size
    // of the buffer
    throw MyException(QString("Buffer too small. Please report this as a bug. "
                              "For now you might want to split up %1")
                          .arg(tweakName));
  }

  std::vector<std::wstring> sections = splitDZString(buffer.data());

  // now iterate over all sections and retrieve a list of keys in each
  for (std::vector<std::wstring>::iterator iter = sections.begin();
       iter != sections.end(); ++iter) {
    // retrieve the names of all keys
    size = ::GetPrivateProfileStringW(iter->c_str(), nullptr, nullptr, buffer.data(),
                                      bufferSize, tweakNameW.c_str());
    if (size == bufferSize - 2) {
      throw MyException(QString("Buffer too small. Please report this as a bug. "
                                "For now you might want to split up %1")
                            .arg(tweakName));
    }

    std::vector<std::wstring> keys = splitDZString(buffer.data());

    for (std::vector<std::wstring>::iterator keyIter = keys.begin();
         keyIter != keys.end(); ++keyIter) {
      // TODO this treats everything as strings but how could I differentiate the type?
      ::GetPrivateProfileStringW(iter->c_str(), keyIter->c_str(), nullptr,
                                 buffer.data(), bufferSize,
                                 ToWString(tweakName).c_str());
      MOBase::WriteRegistryValue(iter->c_str(), keyIter->c_str(), buffer.data(),
                                 tweakedIniW.c_str());
    }
  }
}

void Profile::mergeTweaks(ModInfo::Ptr modInfo, const QString& tweakedIni) const
{
  std::vector<QString> iniTweaks = modInfo->getIniTweaks();
  for (std::vector<QString>::iterator iter = iniTweaks.begin(); iter != iniTweaks.end();
       ++iter) {
    mergeTweak(*iter, tweakedIni);
  }
}

bool Profile::invalidationActive(bool* supported) const
{
  BSAInvalidation* invalidation = m_GamePlugin->feature<BSAInvalidation>();
  DataArchives* dataArchives    = m_GamePlugin->feature<DataArchives>();

  if (supported != nullptr) {
    *supported = ((invalidation != nullptr) && (dataArchives != nullptr));
  }

  return setting("", "AutomaticArchiveInvalidation", false).toBool();
}

void Profile::deactivateInvalidation()
{
  BSAInvalidation* invalidation = m_GamePlugin->feature<BSAInvalidation>();

  if (invalidation != nullptr) {
    invalidation->deactivate(this);
  }

  storeSetting("", "AutomaticArchiveInvalidation", false);
}

void Profile::activateInvalidation()
{
  BSAInvalidation* invalidation = m_GamePlugin->feature<BSAInvalidation>();

  if (invalidation != nullptr) {
    invalidation->activate(this);
  }

  storeSetting("", "AutomaticArchiveInvalidation", true);
}

bool Profile::localSavesEnabled() const
{
  return setting("", "LocalSaves", false).toBool();
}

bool Profile::enableLocalSaves(bool enable)
{
  if (enable) {
    if (!m_Directory.exists("saves")) {
      m_Directory.mkdir("saves");
    }
  } else {
    QDialogButtonBox::StandardButton res;
    res = QuestionBoxMemory::query(
        QApplication::activeModalWidget(), "deleteSavesQuery",
        tr("Delete profile-specific save games?"),
        tr("Do you want to delete the profile-specific save games? (If you select "
           "\"No\", the "
           "save games will show up again if you re-enable profile-specific save "
           "games)"),
        QDialogButtonBox::No | QDialogButtonBox::Yes | QDialogButtonBox::Cancel,
        QDialogButtonBox::No);
    if (res == QMessageBox::Yes) {
      shellDelete(QStringList(m_Directory.absoluteFilePath("saves")));
    } else if (res == QMessageBox::No) {
      // No action
    } else {
      return false;
    }
  }
  storeSetting("", "LocalSaves", enable);
  return true;
}

bool Profile::localSettingsEnabled() const
{
  bool enabled = setting("", "LocalSettings", false).toBool();
  if (enabled) {
    QStringList missingFiles;
    for (QString file : m_GamePlugin->iniFiles()) {
      QString fileName = QFileInfo(file).fileName();
      if (!QFile::exists(m_Directory.filePath(fileName))) {
        log::warn("missing {} in {}", fileName, m_Directory.path());
        missingFiles << fileName;
      }
    }
    if (!missingFiles.empty()) {
      m_GamePlugin->initializeProfile(m_Directory, IPluginGame::CONFIGURATION);
      QMessageBox::StandardButton res =
          QMessageBox::warning(QApplication::activeModalWidget(),
                               tr("Missing profile-specific game INI files!"),
                               tr("Some of your profile-specific game INI files were "
                                  "missing.  They will now be copied "
                                  "from the vanilla game folder.  You might want to "
                                  "double-check your settings.\n\n"
                                  "Missing files:\n") +
                                   missingFiles.join("\n"));
    }
  }
  return enabled;
}

bool Profile::enableLocalSettings(bool enable)
{
  if (enable) {
    m_GamePlugin->initializeProfile(m_Directory.absolutePath(),
                                    IPluginGame::CONFIGURATION);
  } else {
    QDialogButtonBox::StandardButton res;
    res = QuestionBoxMemory::query(QApplication::activeModalWidget(), "deleteINIQuery",
                                   tr("Delete profile-specific game INI files?"),
                                   tr("Do you want to delete the profile-specific game "
                                      "INI files? (If you select \"No\", the "
                                      "INI files will be used again if you re-enable "
                                      "profile-specific game INI files.)"),
                                   QDialogButtonBox::No | QDialogButtonBox::Yes |
                                       QDialogButtonBox::Cancel,
                                   QDialogButtonBox::No);
    if (res == QMessageBox::Yes) {
      QStringList filesToDelete;
      for (QString file : m_GamePlugin->iniFiles()) {
        filesToDelete << m_Directory.absoluteFilePath(QFileInfo(file).fileName());
      }
      shellDelete(filesToDelete, true);
    } else if (res == QMessageBox::No) {
      // No action
    } else {
      return false;
    }
  }
  storeSetting("", "LocalSettings", enable);
  return true;
}

QString Profile::getModlistFileName() const
{
  return QDir::cleanPath(m_Directory.absoluteFilePath("modlist.txt"));
}

QString Profile::getPluginsFileName() const
{
  return QDir::cleanPath(m_Directory.absoluteFilePath("plugins.txt"));
}

QString Profile::getLoadOrderFileName() const
{
  return QDir::cleanPath(m_Directory.absoluteFilePath("loadorder.txt"));
}

QString Profile::getLockedOrderFileName() const
{
  return QDir::cleanPath(m_Directory.absoluteFilePath("lockedorder.txt"));
}

QString Profile::getArchivesFileName() const
{
  return QDir::cleanPath(m_Directory.absoluteFilePath("archives.txt"));
}

QString Profile::getIniFileName() const
{
  auto iniFiles = m_GamePlugin->iniFiles();
  if (iniFiles.isEmpty())
    return "";
  else
    return m_Directory.absoluteFilePath(QFileInfo(iniFiles[0]).fileName());
}

QString Profile::absoluteIniFilePath(QString iniFile) const
{
  // This is the file to which the given iniFile would be mapped, as
  // an absolute file path:
  QFileInfo targetIniFile(m_GamePlugin->documentsDirectory(), iniFile);

  bool isGameIni = false;
  for (auto gameIni : m_GamePlugin->iniFiles()) {
    // We compare the target file, not the actual ones:
    if (QFileInfo(m_GamePlugin->documentsDirectory(), gameIni) == targetIniFile) {
      isGameIni = true;
      break;
    }
  }

  // Local-settings are not enabled, or the iniFile is not in the list of INI
  // files for the current game.
  if (!localSettingsEnabled() || !isGameIni) {
    return targetIniFile.absoluteFilePath();
  }

  // If we reach here, the file is in the profile:
  return m_Directory.absoluteFilePath(targetIniFile.fileName());
}

QString Profile::getProfileTweaks() const
{
  return QDir::cleanPath(
      m_Directory.absoluteFilePath(ToQString(AppConfig::profileTweakIni())));
}

QString Profile::absolutePath() const
{
  return QDir::cleanPath(m_Directory.absolutePath());
}

QString Profile::savePath() const
{
  return QDir::cleanPath(m_Directory.absoluteFilePath("saves"));
}

void Profile::rename(const QString& newName)
{
  QDir profileDir(Settings::instance().paths().profiles());
  profileDir.rename(name(), newName);
  m_Directory.setPath(profileDir.absoluteFilePath(newName));
}

QString keyName(const QString& section, const QString& name)
{
  QString key = section;

  if (!name.isEmpty()) {
    if (!key.isEmpty()) {
      key += "/";
    }

    key += name;
  }

  return key;
}

QVariant Profile::setting(const QString& section, const QString& name,
                          const QVariant& fallback) const
{
  return m_Settings->value(keyName(section, name), fallback);
}

void Profile::storeSetting(const QString& section, const QString& name,
                           const QVariant& value)
{
  m_Settings->setValue(keyName(section, name), value);
}

void Profile::removeSetting(const QString& section, const QString& name)
{
  m_Settings->remove(keyName(section, name));
}

QVariantMap Profile::settingsByGroup(const QString& section) const
{
  QVariantMap results;
  m_Settings->beginGroup(section);
  for (auto key : m_Settings->childKeys()) {
    results[key] = m_Settings->value(key);
  }
  m_Settings->endGroup();
  return results;
}

void Profile::storeSettingsByGroup(const QString& section, const QVariantMap& values)
{
  m_Settings->beginGroup(section);
  for (auto key : values.keys()) {
    m_Settings->setValue(key, values[key]);
  }
  m_Settings->endGroup();
}

QList<QVariantMap> Profile::settingsByArray(const QString& prefix) const
{
  QList<QVariantMap> results;
  int size = m_Settings->beginReadArray(prefix);
  for (int i = 0; i < size; i++) {
    m_Settings->setArrayIndex(i);
    QVariantMap item;
    for (auto key : m_Settings->childKeys()) {
      item[key] = m_Settings->value(key);
    }
    results.append(item);
  }
  m_Settings->endArray();
  return results;
}

void Profile::storeSettingsByArray(const QString& prefix,
                                   const QList<QVariantMap>& values)
{
  m_Settings->beginWriteArray(prefix);
  for (int i = 0; i < values.length(); i++) {
    m_Settings->setArrayIndex(i);
    for (auto key : values.at(i).keys()) {
      m_Settings->setValue(key, values.at(i)[key]);
    }
  }
  m_Settings->endArray();
}

bool Profile::forcedLibrariesEnabled(const QString& executable) const
{
  return setting("forced_libraries", executable + "/enabled", true).toBool();
}

void Profile::setForcedLibrariesEnabled(const QString& executable, bool enabled)
{
  storeSetting("forced_libraries", executable + "/enabled", enabled);
}

QList<ExecutableForcedLoadSetting>
Profile::determineForcedLibraries(const QString& executable) const
{
  QList<ExecutableForcedLoadSetting> results;

  auto rawSettings = settingsByArray("forced_libraries/" + executable);
  auto forcedLoads = m_GamePlugin->executableForcedLoads();

  // look for enabled status on forced loads and add those
  for (auto forcedLoad : forcedLoads) {
    bool found = false;
    for (auto rawSetting : rawSettings) {
      if ((rawSetting.value("process").toString().compare(forcedLoad.process(),
                                                          Qt::CaseInsensitive) == 0) &&
          (rawSetting.value("library").toString().compare(forcedLoad.library(),
                                                          Qt::CaseInsensitive) == 0)) {
        results.append(
            forcedLoad.withEnabled(rawSetting.value("enabled", false).toBool()));
        found = true;
      }
    }
    if (!found) {
      results.append(forcedLoad);
    }
  }

  // add everything else
  for (auto rawSetting : rawSettings) {
    bool add = true;
    for (auto forcedLoad : forcedLoads) {
      if ((rawSetting.value("process").toString().compare(forcedLoad.process(),
                                                          Qt::CaseInsensitive) == 0) &&
          (rawSetting.value("library").toString().compare(forcedLoad.library(),
                                                          Qt::CaseInsensitive) == 0)) {
        add = false;
      }
    }
    if (add) {
      results.append(ExecutableForcedLoadSetting(rawSetting.value("process").toString(),
                                                 rawSetting.value("library").toString())
                         .withEnabled(rawSetting.value("enabled", false).toBool()));
    }
  }

  return results;
}

void Profile::storeForcedLibraries(const QString& executable,
                                   const QList<ExecutableForcedLoadSetting>& values)
{
  QList<QVariantMap> rawSettings;
  for (auto setting : values) {
    QVariantMap rawSetting;
    rawSetting["enabled"] = setting.enabled();
    rawSetting["process"] = setting.process();
    rawSetting["library"] = setting.library();
    rawSettings.append(rawSetting);
  }
  storeSettingsByArray("forced_libraries/" + executable, rawSettings);
}

void Profile::removeForcedLibraries(const QString& executable)
{
  m_Settings->remove("forced_libraries/" + executable);
}

void Profile::debugDump() const
{
  struct Pair
  {
    std::size_t enabled = 0;
    std::size_t total   = 0;
  };

  Pair total;
  Pair real;
  Pair backup;
  Pair separators;
  Pair dlc;
  Pair cc;
  Pair unmanaged;

  auto add = [](Pair& p, const ModStatus& status) {
    ++p.total;

    if (status.m_Enabled) {
      ++p.enabled;
    }
  };

  for (const auto& status : m_ModStatus) {
    auto index = m_ModIndexByPriority.find(status.m_Priority);
    if (index == m_ModIndexByPriority.end()) {
      log::error("mod with priority {} not in priority map", status.m_Priority);
      continue;
    }

    auto m = ModInfo::getByIndex(index->second);
    if (!m) {
      log::error("mod index {} with priority {} not found", index->second,
                 status.m_Priority);
      continue;
    }

    if (m->hasFlag(ModInfo::FLAG_OVERWRITE)) {
      continue;
    }

    add(total, status);

    if (m->hasFlag(ModInfo::FLAG_BACKUP)) {
      add(backup, status);
    }

    if (m->hasFlag(ModInfo::FLAG_SEPARATOR)) {
      add(separators, status);
    }

    if (m->hasFlag(ModInfo::FLAG_FOREIGN)) {
      if (auto* f = dynamic_cast<ModInfoForeign*>(m.get())) {
        switch (f->modType()) {
        case ModInfo::MOD_DLC:
          add(dlc, status);
          break;

        case ModInfo::MOD_CC:
          add(cc, status);
          break;

        default:
          add(unmanaged, status);
          break;
        }
      }
    }

    if (!m->hasAnyOfTheseFlags({ModInfo::FLAG_BACKUP, ModInfo::FLAG_FOREIGN,
                                ModInfo::FLAG_SEPARATOR, ModInfo::FLAG_OVERWRITE})) {
      add(real, status);
    }
  }

  log::debug("profile '{}' in '{}': "
             "mods={}/{} backup={}/{} separators={}/{} real={}/{} dlc={}/{} "
             "cc={}/{} unmanaged={}/{} localsaves={}, localsettings={}",
             name(), absolutePath(), total.enabled, total.total, backup.enabled,
             backup.total, separators.enabled, separators.total, real.enabled,
             real.total, dlc.enabled, dlc.total, cc.enabled, cc.total,
             unmanaged.enabled, unmanaged.total, localSavesEnabled() ? "yes" : "no",
             localSettingsEnabled() ? "yes" : "no");
}
