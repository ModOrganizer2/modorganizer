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

#include "modinfo.h"
#include "settings.h"
#include <utility.h>
#include <error_report.h>
#include "appconfig.h"
#include <iplugingame.h>
#include <report.h>
#include <safewritefile.h>
#include <bsainvalidation.h>
#include <dataarchives.h>

#include <QApplication>
#include <QFile>                                   // for QFile
#include <QFlags>                                  // for operator|, QFlags
#include <QIODevice>                               // for QIODevice, etc
#include <QMessageBox>
#include <QScopedArrayPointer>
#include <QStringList>                             // for QStringList
#include <QtDebug>                                 // for qDebug, qWarning, etc
#include <QtGlobal>                                // for qPrintable
#include <QBuffer>
#include <QDirIterator>

#include <Windows.h>

#include <assert.h>                                // for assert
#include <limits.h>                                // for UINT_MAX, INT_MAX, etc
#include <stddef.h>                                // for size_t
#include <string.h>                                // for wcslen

#include <algorithm>                               // for max, min
#include <exception>                               // for exception
#include <functional>
#include <set>                                     // for set
#include <utility>                                 // for find
#include <stdexcept>

using namespace MOBase;
using namespace MOShared;

void Profile::touchFile(QString fileName)
{
  QFile modList(m_Directory.filePath(fileName));
  if (!modList.open(QIODevice::ReadWrite)) {
    throw std::runtime_error(QObject::tr("failed to create %1").arg(m_Directory.filePath(fileName)).toUtf8().constData());
  }
}

Profile::Profile(const QString &name, IPluginGame const *gamePlugin, bool useDefaultSettings)
  : m_ModListWriter(std::bind(&Profile::doWriteModlist, this))
  , m_GamePlugin(gamePlugin)
{
  QString profilesDir = Settings::instance().getProfileDirectory();
  QDir profileBase(profilesDir);

  m_Settings = new QSettings(profileBase.absoluteFilePath("settings.ini"));

  QString fixedName = name;
  if (!fixDirectoryName(fixedName)) {
    throw MyException(tr("invalid profile name %1").arg(name));
  }

  if (!profileBase.exists() || !profileBase.mkdir(fixedName)) {
    throw MyException(tr("failed to create %1").arg(fixedName).toUtf8().constData());
  }
  QString fullPath = profilesDir + "/" + fixedName;
  m_Directory = QDir(fullPath);

  try {
    // create files. Needs to happen after m_Directory was set!
    touchFile("modlist.txt");
    touchFile("archives.txt");

    IPluginGame::ProfileSettings settings = IPluginGame::CONFIGURATION
                                          | IPluginGame::MODS
                                          | IPluginGame::SAVEGAMES;

    if (useDefaultSettings) {
      settings |= IPluginGame::PREFER_DEFAULTS;
    }

    gamePlugin->initializeProfile(fullPath, settings);
  } catch (...) {
    // clean up in case of an error
    shellDelete(QStringList(profileBase.absoluteFilePath(fixedName)));
    throw;
  }
  refreshModStatus();
}


Profile::Profile(const QDir &directory, IPluginGame const *gamePlugin)
  : m_Directory(directory)
  , m_GamePlugin(gamePlugin)
  , m_ModListWriter(std::bind(&Profile::doWriteModlist, this))
{
  assert(gamePlugin != nullptr);

  m_Settings = new QSettings(directory.absoluteFilePath("settings.ini"),
                             QSettings::IniFormat);

  if (!QFile::exists(m_Directory.filePath("modlist.txt"))) {
    qWarning("missing modlist.txt in %s", qPrintable(directory.path()));
    touchFile(m_Directory.filePath("modlist.txt"));
  }

  IPluginGame::ProfileSettings settings = IPluginGame::MODS
                                        | IPluginGame::SAVEGAMES;
  gamePlugin->initializeProfile(directory, settings);

  refreshModStatus();
}


Profile::Profile(const Profile &reference)
  : m_Directory(reference.m_Directory)
  , m_ModListWriter(std::bind(&Profile::doWriteModlist, this))
  , m_GamePlugin(reference.m_GamePlugin)

{
  m_Settings = new QSettings(m_Directory.absoluteFilePath("settings.ini"),
                             QSettings::IniFormat);
  refreshModStatus();
}


Profile::~Profile()
{
  delete m_Settings;
  m_ModListWriter.writeImmediately(true);
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
  if (!m_Directory.exists()) return;

  try {
    QString fileName = getModlistFileName();
    SafeWriteFile file(fileName);

    file->write(QString("# This file was automatically generated by Mod Organizer.\r\n").toUtf8());
    if (m_ModStatus.empty()) {
      return;
    }

    for (std::map<int, unsigned int>::const_reverse_iterator iter = m_ModIndexByPriority.crbegin(); iter != m_ModIndexByPriority.crend(); iter++ ) {
      //qDebug(QString("write mod %1 to priority %2").arg(iter->first).arg(iter->second).toLocal8Bit());
      // the priority order was inverted on load so it has to be inverted again
      unsigned int index = iter->second;
      if (index != UINT_MAX) {
        ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
        std::vector<ModInfo::EFlag> flags = modInfo->getFlags();
        if ((modInfo->getFixedPriority() == INT_MIN)) {
          if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_FOREIGN) != flags.end()) {
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
    }

    if (file.commitIfDifferent(m_LastModlistHash)) {
      qDebug("%s saved", QDir::toNativeSeparators(fileName).toUtf8().constData());
    }
  } catch (const std::exception &e) {
    reportError(tr("failed to write mod list: %1").arg(e.what()));
    return;
  }
}


void Profile::createTweakedIniFile()
{
  QString tweakedIni = m_Directory.absoluteFilePath("initweaks.ini");

  if (QFile::exists(tweakedIni) && !shellDeleteQuiet(tweakedIni)) {
    reportError(tr("failed to update tweaked ini file, wrong settings may be used: %1").arg(windowsErrorString(::GetLastError())));
    return;
  }

  for (int i = getPriorityMinimum(); i < getPriorityMinimum() + (int)numRegularMods(); ++i) {
    unsigned int idx = modIndexByPriority(i);
    if (m_ModStatus[idx].m_Enabled) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(idx);
      mergeTweaks(modInfo, tweakedIni);
    }
  }

  mergeTweak(getProfileTweaks(), tweakedIni);

  bool error = false;
  if (!::WritePrivateProfileStringW(L"Archive", L"bInvalidateOlderFiles", L"1", ToWString(tweakedIni).c_str())) {
    error = true;
  }

  if (error) {
    reportError(tr("failed to create tweaked ini: %1").arg(getCurrentErrorStringA().c_str()));
  }
  qDebug("%s saved", qPrintable(QDir::toNativeSeparators(tweakedIni)));
}

// static
void Profile::renameModInAllProfiles(const QString& oldName, const QString& newName)
{
  QDir profilesDir(Settings::instance().getProfileDirectory());
  profilesDir.setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);
  QDirIterator profileIter(profilesDir);
  while (profileIter.hasNext()) {
    profileIter.next();
    QFile modList(profileIter.filePath() + "/modlist.txt");
    if (modList.exists())
      renameModInList(modList, oldName, newName);
    else
      qWarning("Profile has no modlist.txt : %s", qPrintable(profileIter.filePath()));
  }
}

// static
void Profile::renameModInList(QFile &modList, const QString &oldName, const QString &newName)
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
      qWarning("mod list contained invalid data: empty line");
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
      qWarning("mod list contained invalid data: missing mod name");
      continue;
    }

    outBuffer.write(QByteArray(1, spec));
    if (modName == oldName) {
      modName = newName;
      ++renamed;
    }
    outBuffer.write(modName.toUtf8().constData());
    outBuffer.write("\r\n");
  }
  modList.close();

  if (renamed) {
    modList.open(QIODevice::WriteOnly);
    modList.write(outBuffer.buffer());
    modList.close();
  }

  if (renamed)
    qDebug("Renamed %d \"%s\" mod to \"%s\" in %s",
      renamed, qPrintable(oldName), qPrintable(newName), qPrintable(modList.fileName()));
}

void Profile::refreshModStatus()
{
  writeModlistNow(true); // if there are pending changes write them first

  QFile file(getModlistFileName());
  if (!file.open(QIODevice::ReadOnly)) {
    throw MyException(tr("\"%1\" is missing or inaccessible").arg(getModlistFileName()));
  }

  bool modStatusModified = false;
  m_ModStatus.clear();
  m_ModStatus.resize(ModInfo::getNumMods());

  std::set<QString> namesRead;

  // load mods from file and update enabled state and priority for them
  int index = 0;
  while (!file.atEnd()) {
    QByteArray line = file.readLine().trimmed();
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
    } else if ((line.at(0) == '+')
               || (line.at(0) == '*')) {
      modName = QString::fromUtf8(line.mid(1).trimmed().constData());
    } else {
      modName = QString::fromUtf8(line.trimmed().constData());
    }
    if (modName.size() > 0) {
      QString lookupName = modName;
      if (namesRead.find(lookupName) != namesRead.end()) {
        continue;
      } else {
        namesRead.insert(lookupName);
      }
      unsigned int modIndex = ModInfo::getIndex(lookupName);
      if (modIndex != UINT_MAX) {
        ModInfo::Ptr info = ModInfo::getByIndex(modIndex);
        if ((modIndex < m_ModStatus.size())
            && (info->getFixedPriority() == INT_MIN)) {
          m_ModStatus[modIndex].m_Enabled = enabled;
          if (m_ModStatus[modIndex].m_Priority == -1) {
            if (static_cast<size_t>(index) >= m_ModStatus.size()) {
              throw MyException(tr("invalid index %1").arg(index));
            }
            m_ModStatus[modIndex].m_Priority = index++;
          }
        } else {
          qWarning("no mod state for \"%s\" (profile \"%s\")",
                   qPrintable(modName), m_Directory.path().toUtf8().constData());
          // need to rewrite the modlist to fix this
          modStatusModified = true;
        }
      } else {
        qDebug("mod \"%s\" (profile \"%s\") not found",
               qPrintable(modName), m_Directory.path().toUtf8().constData());
        // need to rewrite the modlist to fix this
        modStatusModified = true;
      }
    }
  }

  int numKnownMods = index;

  int topInsert = 0;

  // invert priority order to match that of the pluginlist. Also
  // give priorities to mods not referenced in the profile
  for (size_t i = 0; i < m_ModStatus.size(); ++i) {
    ModInfo::Ptr modInfo = ModInfo::getByIndex(static_cast<int>(i));
    if (modInfo->alwaysEnabled()) {
      m_ModStatus[i].m_Enabled = true;
    }

    if (modInfo->getFixedPriority() == INT_MAX) {
      continue;
    }

    if (m_ModStatus[i].m_Priority != -1) {
      m_ModStatus[i].m_Priority = numKnownMods - m_ModStatus[i].m_Priority - 1;
    } else {
      if (static_cast<size_t>(index) >= m_ModStatus.size()) {
        throw MyException(tr("invalid index %1").arg(index));
      }
      if (modInfo->hasFlag(ModInfo::FLAG_FOREIGN)) {
        m_ModStatus[i].m_Priority = --topInsert;
      } else {
        m_ModStatus[i].m_Priority = index++;
      }
      // also, mark the mod-list as changed
      modStatusModified = true;
    }
  }
  // to support insertion of new mods at the top we may now have mods with negative priority. shift them all up
  // to align priority with 0
  if (topInsert < 0) {
    int offset = topInsert * -1;
    for (size_t i = 0; i < m_ModStatus.size(); ++i) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(static_cast<unsigned int>(i));
      if (modInfo->getFixedPriority() == INT_MAX) {
        continue;
      }

      m_ModStatus[i].m_Priority += offset;
    }
  }

  file.close();
  updateIndices();
  if (modStatusModified) {
    m_ModListWriter.write();
  }
}


void Profile::dumpModStatus() const
{
  for (unsigned int i = 0; i < m_ModStatus.size(); ++i) {
    ModInfo::Ptr info = ModInfo::getByIndex(i);
    qWarning("%d: %s - %d (%s)", i, info->name().toUtf8().constData(), m_ModStatus[i].m_Priority,
             m_ModStatus[i].m_Enabled ? "enabled" : "disabled");
  }
}


void Profile::updateIndices()
{
  m_NumRegularMods = 0;
  m_ModIndexByPriority.clear();
  for (unsigned int i = 0; i < m_ModStatus.size(); ++i) {
    int priority = m_ModStatus[i].m_Priority;
    if (priority == INT_MIN) {
      // don't assign this to mapping at all, it's probably the overwrite mod
      continue;
    } else {
      ++m_NumRegularMods;
      m_ModIndexByPriority[priority] = i;
    }
  }
}


std::vector<std::tuple<QString, QString, int> > Profile::getActiveMods()
{
  std::vector<std::tuple<QString, QString, int> > result;
  for (std::map<int, unsigned int>::const_iterator iter = m_ModIndexByPriority.begin(); iter != m_ModIndexByPriority.end(); iter++ ) {
    if ((iter->second != UINT_MAX) && m_ModStatus[iter->second].m_Enabled) {
      ModInfo::Ptr modInfo = ModInfo::getByIndex(iter->second);
      result.push_back(std::make_tuple(modInfo->internalName(), modInfo->absolutePath(), m_ModStatus[iter->second].m_Priority));
    }
  }

  unsigned int overwriteIndex = ModInfo::findMod([](ModInfo::Ptr mod) -> bool {
    std::vector<ModInfo::EFlag> flags = mod->getFlags();
    return std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end(); });

  if (overwriteIndex != UINT_MAX) {
    ModInfo::Ptr overwriteInfo = ModInfo::getByIndex(overwriteIndex);
    result.push_back(std::make_tuple(overwriteInfo->name(), overwriteInfo->absolutePath(), UINT_MAX));
  } else {
    reportError(tr("Overwrite directory couldn't be parsed"));
  }
  return result;
}


unsigned int Profile::modIndexByPriority(int priority) const
{
  try {
    return m_ModIndexByPriority.at(priority);
  } catch (std::out_of_range) {
    throw MyException(tr("invalid priority %1").arg(priority));
  }
}


void Profile::setModEnabled(unsigned int index, bool enabled)
{
  if (index >= m_ModStatus.size()) {
    throw MyException(tr("invalid index %1").arg(index));
  }

  ModInfo::Ptr modInfo = ModInfo::getByIndex(index);
  // we could quit in the following case, this shouldn't be a change anyway,
  // but at least this allows the situation to be fixed in case of an error
  if (modInfo->alwaysEnabled()) {
    enabled = true;
  }

  if (enabled != m_ModStatus[index].m_Enabled) {
    m_ModStatus[index].m_Enabled = enabled;
    emit modStatusChanged(index);
  }
}

bool Profile::modEnabled(unsigned int index) const
{
  if (index >= m_ModStatus.size()) {
    throw MyException(tr("invalid index %1").arg(index));
  }

  return m_ModStatus[index].m_Enabled;
}


int Profile::getModPriority(unsigned int index) const
{
  if (index >= m_ModStatus.size()) {
    throw MyException(tr("invalid index %1").arg(index));
  }

  return m_ModStatus[index].m_Priority;
}


void Profile::setModPriority(unsigned int index, int &newPriority)
{
  if (m_ModStatus.at(index).m_Overwrite) {
    // can't change priority of the overwrite
    return;
  }

  int oldPriority = m_ModStatus.at(index).m_Priority;
  int lastPriority = INT_MIN;

  if (newPriority == oldPriority) {
    // nothing to do
    return;
  }

  for (std::map<int, unsigned int>::iterator iter = m_ModIndexByPriority.begin(); iter != m_ModIndexByPriority.end(); iter++) {
    if (newPriority < oldPriority && iter->first >= newPriority && iter->first < oldPriority) {
      m_ModStatus.at(iter->second).m_Priority += 1;
    }
    else if (newPriority > oldPriority && iter->first <= newPriority && iter->first > oldPriority) {
      m_ModStatus.at(iter->second).m_Priority -= 1;
    }
    lastPriority = std::max(lastPriority, iter->first);
  }

  newPriority = std::min(newPriority, lastPriority);
  m_ModStatus.at(index).m_Priority = std::min(newPriority, lastPriority);

  updateIndices();
  m_ModListWriter.write();
}

Profile *Profile::createPtrFrom(const QString &name, const Profile &reference, MOBase::IPluginGame const *gamePlugin)
{
  QString profileDirectory = Settings::instance().getProfileDirectory() + "/" + name;
  reference.copyFilesTo(profileDirectory);
  return new Profile(QDir(profileDirectory), gamePlugin);
}

void Profile::copyFilesTo(QString &target) const
{
  copyDir(m_Directory.absolutePath(), target, false);
}

std::vector<std::wstring> Profile::splitDZString(const wchar_t *buffer) const
{
  std::vector<std::wstring> result;
  const wchar_t *pos = buffer;
  size_t length = wcslen(pos);
  while (length != 0U) {
    result.push_back(pos);
    pos += length + 1;
    length = wcslen(pos);
  }
  return result;
}

void Profile::mergeTweak(const QString &tweakName, const QString &tweakedIni) const
{
  static const int bufferSize = 32768;

  std::wstring tweakNameW  = ToWString(tweakName);
  std::wstring tweakedIniW = ToWString(tweakedIni);
  QScopedArrayPointer<wchar_t> buffer(new wchar_t[bufferSize]);

  // retrieve a list of sections
  DWORD size = ::GetPrivateProfileSectionNamesW(
        buffer.data(), bufferSize, tweakNameW.c_str());

  if (size == bufferSize - 2) {
    // unfortunately there is no good way to find the required size
    // of the buffer
    throw MyException(QString("Buffer too small. Please report this as a bug. "
                        "For now you might want to split up %1").arg(tweakName));
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
                          "For now you might want to split up %1").arg(tweakName));
    }

    std::vector<std::wstring> keys = splitDZString(buffer.data());

    for (std::vector<std::wstring>::iterator keyIter = keys.begin();
         keyIter != keys.end(); ++keyIter) {
       //TODO this treats everything as strings but how could I differentiate the type?
      ::GetPrivateProfileStringW(iter->c_str(), keyIter->c_str(),
                                 nullptr, buffer.data(), bufferSize, ToWString(tweakName).c_str());
      ::WritePrivateProfileStringW(iter->c_str(), keyIter->c_str(),
                                   buffer.data(), tweakedIniW.c_str());
    }
  }
}

void Profile::mergeTweaks(ModInfo::Ptr modInfo, const QString &tweakedIni) const
{
  std::vector<QString> iniTweaks = modInfo->getIniTweaks();
  for (std::vector<QString>::iterator iter = iniTweaks.begin();
       iter != iniTweaks.end(); ++iter) {
    mergeTweak(*iter, tweakedIni);
  }
}


bool Profile::invalidationActive(bool *supported) const
{
  BSAInvalidation *invalidation = m_GamePlugin->feature<BSAInvalidation>();
  DataArchives *dataArchives = m_GamePlugin->feature<DataArchives>();

  if ((invalidation != nullptr) && (dataArchives != nullptr)) {
    if (supported != nullptr) {
      *supported = true;
    }

    for (const QString &archive : dataArchives->archives(this)) {
      if (invalidation->isInvalidationBSA(archive)) {
        return true;
      }
    }
    return false;
  } else {
    if (supported != nullptr) {
      *supported = false;
    }
  }
  return false;
}


void Profile::deactivateInvalidation()
{
  BSAInvalidation *invalidation = m_GamePlugin->feature<BSAInvalidation>();

  if (invalidation != nullptr) {
    invalidation->deactivate(this);
  }
}


void Profile::activateInvalidation()
{
  BSAInvalidation *invalidation = m_GamePlugin->feature<BSAInvalidation>();

  if (invalidation != nullptr) {
    invalidation->activate(this);
  }
}


bool Profile::localSavesEnabled() const
{
  return m_Directory.exists("saves");
}


bool Profile::enableLocalSaves(bool enable)
{
  if (enable) {
    if (m_Directory.exists("_saves")) {
      m_Directory.rename("_saves", "saves");
    } else {
      m_Directory.mkdir("saves");
    }
  } else {
    QMessageBox::StandardButton res = QMessageBox::question(
        QApplication::activeModalWidget(), tr("Delete savegames?"),
        tr("Do you want to delete local savegames? (If you select \"No\", the "
           "save games will show up again if you re-enable local savegames)"),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (res == QMessageBox::Yes) {
      shellDelete(QStringList(m_Directory.absoluteFilePath("saves")));
    } else if (res == QMessageBox::No) {
      m_Directory.rename("saves", "_saves");
    } else {
      return false;
    }
  }

  // default: assume success
  return true;
}

bool Profile::localSettingsEnabled() const
{
  return m_Directory.exists(getIniFileName());
}

bool Profile::enableLocalSettings(bool enable)
{
  // TODO: this currently assumes game settings are stored in an ini file.
  // This shall become very interesting when a game stores its settings in the
  // registry
  QString backupFile = getIniFileName() + "_";
  if (enable) {
    if (m_Directory.exists(backupFile)) {

      shellRename(backupFile, getIniFileName());
    } else {
      IPluginGame *game = qApp->property("managed_game").value<IPluginGame *>();
      game->initializeProfile(m_Directory, IPluginGame::CONFIGURATION);
    }
  } else {
    shellRename(getIniFileName(), backupFile);
  }

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

QString Profile::getDeleterFileName() const
{
  return QDir::cleanPath(m_Directory.absoluteFilePath("hide_plugins.txt"));
}

QString Profile::getIniFileName() const
{
  return m_Directory.absoluteFilePath(m_GamePlugin->iniFiles()[0]);
}

QString Profile::getProfileTweaks() const
{
  return QDir::cleanPath(m_Directory.absoluteFilePath(ToQString(AppConfig::profileTweakIni())));
}

QString Profile::absolutePath() const
{
  return QDir::cleanPath(m_Directory.absolutePath());
}

QString Profile::savePath() const
{
  return QDir::cleanPath(m_Directory.absoluteFilePath("saves"));

}

void Profile::rename(const QString &newName)
{
  QDir profileDir(Settings::instance().getProfileDirectory());
  profileDir.rename(name(), newName);
  m_Directory = profileDir.absoluteFilePath(newName);
}

QVariant Profile::setting(const QString &section, const QString &name,
                          const QVariant &fallback)
{
  return m_Settings->value(section + "/" + name, fallback);
}

void Profile::storeSetting(const QString &section, const QString &name,
                           const QVariant &value)
{
  m_Settings->setValue(section + "/" + name, value);
}

void Profile::removeSetting(const QString &section, const QString &name)
{
	m_Settings->remove(section + "/" + name);
}

int Profile::getPriorityMinimum() const
{
  return m_ModIndexByPriority.begin()->first;
}
