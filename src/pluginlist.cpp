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

#include "pluginlist.h"
#include "report.h"
#include "inject.h"
#include <utility.h>
#include "settings.h"
#include <gameinfo.h>
#include <windows_error.h>

#include <QtDebug>
#include <QMessageBox>
#include <QMimeData>
#include <QCoreApplication>
#include <QDir>
#include <QTextCodec>
#include <QFileInfo>
#include <QListWidgetItem>
#include <QString>
#include <QApplication>
#include <QKeyEvent>
#include <QSortFilterProxyModel>

#include <tchar.h>
#include <ctime>
#include <algorithm>
#include <stdexcept>


using namespace MOBase;
using namespace MOShared;


bool ByName(const PluginList::ESPInfo& LHS, const PluginList::ESPInfo& RHS) {
  return LHS.m_Name.toUpper() < RHS.m_Name.toUpper();
}

bool ByPriority(const PluginList::ESPInfo& LHS, const PluginList::ESPInfo& RHS) {
  if (LHS.m_IsMaster && !RHS.m_IsMaster) {
    return true;
  } else if (!LHS.m_IsMaster && RHS.m_IsMaster) {
    return false;
  } else {
    return LHS.m_Priority < RHS.m_Priority;
  }
}

bool ByDate(const PluginList::ESPInfo& LHS, const PluginList::ESPInfo& RHS) {
  QString lhsExtension = LHS.m_Name.right(3).toLower();
  QString rhsExtension = RHS.m_Name.right(3).toLower();
  if (lhsExtension != rhsExtension) {
    return lhsExtension == "esm";
  }

  return ::CompareFileTime(&LHS.m_Time, &RHS.m_Time) < 0;
}

PluginList::PluginList(QObject *parent)
  : QAbstractTableModel(parent), m_Modified(false),
    m_FontMetrics(QFont())
{
}


QString PluginList::getColumnName(int column)
{
  switch (column) {
    case COL_NAME:     return tr("Name");
    case COL_PRIORITY: return tr("Priority");
    case COL_MODINDEX: return tr("Mod Index");
    default: return tr("unknown");
  }
}


QString PluginList::getColumnToolTip(int column)
{
  switch (column) {
    case COL_NAME:     return tr("Name of your mods");
    case COL_PRIORITY: return tr("Load priority of your mod. The higher, the more \"important\" it is and thus "
                                 "overwrites data from plugins with lower priority.");
    case COL_MODINDEX: return tr("The modindex determins the formids of objects originating from this mods.");
    default: return tr("unknown");
  }
}


void PluginList::refresh(const QString &profileName, const DirectoryEntry &baseDirectory,
                         const QString &pluginsFile, const QString &loadOrderFile,
                         const QString &lockedOrderFile)
{
  emit layoutAboutToBeChanged();
  m_ESPsByName.clear();
  m_ESPsByPriority.clear();
  m_ESPs.clear();
  std::vector<std::wstring> primaryPlugins = GameInfo::instance().getPrimaryPlugins();

  m_CurrentProfile = profileName;

  std::vector<FileEntry*> files = baseDirectory.getFiles();
  for (auto iter = files.begin(); iter != files.end(); ++iter) {
    FileEntry *current = *iter;
    if (current == NULL) {
      continue;
    }
    QString filename = QString::fromUtf16(current->getName().c_str());
    QString extension = filename.right(3).toLower();

    if ((extension == "esp") || (extension == "esm")) {
      bool forceEnabled = Settings::instance().forceEnableCoreFiles() &&
                            std::find(primaryPlugins.begin(), primaryPlugins.end(), ToWString(filename.toLower())) != primaryPlugins.end();

      bool archive = false;
      FilesOrigin &origin = baseDirectory.getOriginByID(current->getOrigin(archive));
      m_ESPs.push_back(ESPInfo(filename, forceEnabled, current->getFileTime(), ToQString(origin.getName())));
    }
  }

  if (readLoadOrder(loadOrderFile)) {
    int maxPriority = 0;
    // assign known load orders
    for (std::vector<ESPInfo>::iterator espIter = m_ESPs.begin(); espIter != m_ESPs.end(); ++espIter) {
      std::map<QString, int>::const_iterator priorityIter = m_ESPLoadOrder.find(espIter->m_Name.toLower());
      if (priorityIter != m_ESPLoadOrder.end()) {
        if (priorityIter->second > maxPriority) {
          maxPriority = priorityIter->second;
        }
        espIter->m_Priority = priorityIter->second;
      } else {
        espIter->m_Priority = -1;
      }
    }

    ++maxPriority;

    // assign maximum priorities for plugins with unknown priority
    for (std::vector<ESPInfo>::iterator espIter = m_ESPs.begin(); espIter != m_ESPs.end(); ++espIter) {
      if (espIter->m_Priority == -1) {
        espIter->m_Priority = maxPriority++;
      }
    }
  } else {
    // no load order stored, determine by date
    std::sort(m_ESPs.begin(), m_ESPs.end(), ByDate);

    for (size_t i = 0; i < m_ESPs.size(); ++i) {
      m_ESPs[i].m_Priority = i;
    }
  }

  std::sort(m_ESPs.begin(), m_ESPs.end(), ByPriority); // first, sort by priority
  // remove gaps from the priorities so we can use them as array indices without overflow
  for (int i = 0; i < static_cast<int>(m_ESPs.size()); ++i) {
    m_ESPs[i].m_Priority = i;
  }

  std::sort(m_ESPs.begin(), m_ESPs.end(), ByName); // sort by name so alphabetical sorting works

  updateIndices();

  readEnabledFrom(pluginsFile);

  readLockedOrderFrom(lockedOrderFile);

  refreshLoadOrder();

  emit layoutChanged();
  emit dataChanged(this->index(0, 0), this->index(m_ESPs.size(), 1));
}


void PluginList::enableESP(const QString &name)
{
  std::map<QString, int>::iterator iter = m_ESPsByName.find(name.toLower());

  if (iter != m_ESPsByName.end()) {
    m_ESPs[iter->second].m_Enabled = true;
    m_Modified = true;
  } else {
    reportError(tr("esp not found: %1").arg(name));
  }
}


void PluginList::enableAll()
{
  for (std::vector<ESPInfo>::iterator iter = m_ESPs.begin(); iter != m_ESPs.end(); ++iter) {
    iter->m_Enabled = true;
  }
  m_Modified = true;
  emit esplist_changed();
}


void PluginList::disableAll()
{
  for (std::vector<ESPInfo>::iterator iter = m_ESPs.begin(); iter != m_ESPs.end(); ++iter) {
    if (!iter->m_ForceEnabled) {
      iter->m_Enabled = false;
    }
  }
  m_Modified = true;
  emit esplist_changed();
}


bool PluginList::isEnabled(const QString &name)
{
  std::map<QString, int>::iterator iter = m_ESPsByName.find(name.toLower());

  if (iter != m_ESPsByName.end()) {
    return m_ESPs[iter->second].m_Enabled;
  } else {
    return false;
  }
}


bool PluginList::isEnabled(int index)
{
  return m_ESPs.at(index).m_Enabled;
}


bool PluginList::readLoadOrder(const QString &fileName)
{
  std::set<QString> availableESPs;
  for (std::vector<ESPInfo>::const_iterator iter = m_ESPs.begin(); iter != m_ESPs.end(); ++iter) {
    availableESPs.insert(iter->m_Name.toLower());
  }

  m_ESPLoadOrder.clear();

  int priority = 0;

  std::vector<std::wstring> primaryPlugins = GameInfo::instance().getPrimaryPlugins();
  for (std::vector<std::wstring>::iterator iter = primaryPlugins.begin();
       iter != primaryPlugins.end(); ++iter) {
    if (availableESPs.find(ToQString(*iter)) != availableESPs.end()) {
      m_ESPLoadOrder[ToQString(*iter)] = priority++;
    }
  }

  QFile file(fileName);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }
  while (!file.atEnd()) {
    QByteArray line = file.readLine().trimmed();
    QString modName;
    if ((line.size() > 0) && (line.at(0) != '#')) {
      modName = QString::fromUtf8(line.constData()).toLower();
    }

    if ((modName.size() > 0) &&
        (m_ESPLoadOrder.find(modName) == m_ESPLoadOrder.end()) &&
        (availableESPs.find(modName) != availableESPs.end())) {
      m_ESPLoadOrder[modName] = priority++;
    }
  }

  file.close();
  return true;
}


void PluginList::readEnabledFrom(const QString &fileName)
{
  for (std::vector<ESPInfo>::iterator iter = m_ESPs.begin(); iter != m_ESPs.end(); ++iter) {
    if (!iter->m_ForceEnabled) {
      iter->m_Enabled = false;
    }
    iter->m_LoadOrder = -1;
  }

  QFile file(fileName);
  if (!file.exists()) {
    throw std::runtime_error(QObject::tr("failed to find \"%1\"").arg(fileName).toUtf8().constData());
  }

  file.open(QIODevice::ReadOnly);
  while (!file.atEnd()) {
    QByteArray line = file.readLine();
    QString modName;
    if ((line.size() > 0) && (line.at(0) != '#')) {
      modName = QString::fromUtf8(line.trimmed().constData());
    }
    if (modName.size() > 0) {
      std::map<QString, int>::iterator iter = m_ESPsByName.find(modName.toLower());
      if (iter != m_ESPsByName.end()) {
        m_ESPs[iter->second].m_Enabled = true;
      } else {
        qWarning("plugin %s not found", modName.toUtf8().constData());
        m_Modified = true;
      }
    }
  }

  file.close();
}


void PluginList::readLockedOrderFrom(const QString &fileName)
{
  m_LockedOrder.clear();

  QFile file(fileName);
  if (!file.exists()) {
    // no locked load order, that's ok
    return;
  }

  file.open(QIODevice::ReadOnly);
  while (!file.atEnd()) {
    QByteArray line = file.readLine();
    if ((line.size() > 0) && (line.at(0) != '#')) {
      QList<QByteArray> fields = line.split('|');
      if (fields.count() == 2) {
        m_LockedOrder[QString::fromUtf8(fields.at(0))] = fields.at(1).trimmed().toInt();
      } else {
        reportError(tr("The file containing locked plugin indices is broken"));
        break;
      }
    }
  }
  file.close();
}


void PluginList::writePlugins(const QString &fileName, bool writeUnchecked) const
{
  QFile file(fileName);
  if (!file.open(QIODevice::WriteOnly)) {
    throw MyException(tr("failed to open output file: %1").arg(fileName));
  }

  QTextCodec *textCodec = writeUnchecked ? QTextCodec::codecForName("utf-8")
                                         : QTextCodec::codecForName("Windows-1252");

  if (textCodec == NULL) {
    QList<QByteArray> encodingList = QTextCodec::availableCodecs();
    QString encodings;
    QTextStream temp(&encodings);
    foreach (QByteArray encoding, encodingList) {
      temp << encoding << ", ";
    }
    qCritical("required string-encoding not supported. Available codecs: %s",
              encodings.toUtf8().constData());

    throw std::runtime_error(QObject::tr("encoding error, please report this as a bug and include the file "
                                         "mo_interface.log!").toUtf8().constData());
  }

  file.resize(0);

  file.write(textCodec->fromUnicode("# This file was automatically generated by Mod Organizer.\r\n"));

  bool invalidFileNames = false;

  for (size_t i = 0; i < m_ESPs.size(); ++i) {
    int priority = m_ESPsByPriority[i];
    if ((m_ESPs[priority].m_Enabled || writeUnchecked) && !m_ESPs[priority].m_Removed) {
      //file.write(m_ESPs[priority].m_Name.toUtf8());
      if (!textCodec->canEncode(m_ESPs[priority].m_Name)) {
        invalidFileNames = true;
        qCritical("invalid plugin name %s", m_ESPs[priority].m_Name.toUtf8().constData());
      } else {
        file.write(textCodec->fromUnicode(m_ESPs[priority].m_Name));
      }
      file.write("\r\n");
    }
  }
  file.close();

  if (invalidFileNames) {
    reportError(tr("Some of your plugins have invalid names! These plugins can not be loaded by the game. "
                   "Please see mo_interface.log for a list of affected plugins and rename them."));
  }

  qDebug("%s saved", QDir::toNativeSeparators(fileName).toUtf8().constData());
}


void PluginList::writeLockedOrder(const QString &fileName) const
{
  QFile file(fileName);
  if (!file.open(QIODevice::WriteOnly)) {
    throw MyException(tr("failed to open output file: %1").arg(fileName));
  }

  file.resize(0);
  file.write(QString("# This file was automatically generated by Mod Organizer.\r\n").toUtf8());
  for (auto iter = m_LockedOrder.begin(); iter != m_LockedOrder.end(); ++iter) {
    file.write(QString("%1|%2\r\n").arg(iter->first).arg(iter->second).toUtf8());
  }
  file.close();
}


void PluginList::saveTo(const QString &pluginFileName, const QString &loadOrderFileName, const QString &lockedOrderFileName, const QString& deleterFileName, bool hideUnchecked) const
{
  if (!m_Modified) {
    return;
  }

  writePlugins(pluginFileName, false);
  writePlugins(loadOrderFileName, true);
  writeLockedOrder(lockedOrderFileName);

  if (hideUnchecked) {
    QFile deleterFile(deleterFileName);
    deleterFile.open(QIODevice::WriteOnly);
    deleterFile.resize(0);
    deleterFile.write(QString("# This file was automatically generated by Mod Organizer.\r\n").toUtf8());

    for (size_t i = 0; i < m_ESPs.size(); ++i) {
      int priority = m_ESPsByPriority[i];
      if (m_ESPs[priority].m_Removed) {
        deleterFile.write(m_ESPs[priority].m_Name.toUtf8());
        deleterFile.write("\r\n");
      }
    }
    deleterFile.close();
    qDebug("%s saved", QDir::toNativeSeparators(deleterFileName).toUtf8().constData());
  }

  m_Modified = false;
}


bool PluginList::saveLoadOrder(DirectoryEntry &directoryStructure)
{
  if (GameInfo::instance().getLoadOrderMechanism() != GameInfo::TYPE_FILETIME) {
    // nothing to do
    return true;
  }

  for (std::vector<ESPInfo>::iterator iter = m_ESPs.begin(); iter != m_ESPs.end(); ++iter) {
    std::wstring espName = ToWString(iter->m_Name);
    const FileEntry *fileEntry = directoryStructure.findFile(espName);
    if (fileEntry != NULL) {
      QString fileName;
      bool archive = false;
      int originid = fileEntry->getOrigin(archive);
      fileName = QString("%1\\%2").arg(QDir::toNativeSeparators(ToQString(directoryStructure.getOriginByID(originid).getPath()))).arg(iter->m_Name);

      HANDLE file = ::CreateFile(ToWString(fileName).c_str(), GENERIC_READ | GENERIC_WRITE,
                                 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
      if (file == INVALID_HANDLE_VALUE) {
        if (::GetLastError() == ERROR_SHARING_VIOLATION) {
          // file is locked, probably the game is running
          return false;
        } else {
          throw windows_error(QObject::tr("failed to access %1").arg(fileName).toUtf8().constData());
        }
      }

      ULONGLONG temp = 0;
      temp = (145731ULL + iter->m_Priority) * 24 * 60 * 60 * 10000000ULL;

      FILETIME newWriteTime;

      newWriteTime.dwLowDateTime  = (DWORD)(temp & 0xFFFFFFFF);
      newWriteTime.dwHighDateTime = (DWORD)(temp >> 32);
      iter->m_Time = newWriteTime;
      fileEntry->setFileTime(newWriteTime);
      if (!::SetFileTime(file, NULL, NULL, &newWriteTime)) {
        throw windows_error(QObject::tr("failed to set file time %1").arg(fileName).toUtf8().constData());
      }

      CloseHandle(file);
    }
  }
  return true;
}

bool PluginList::isESPLocked(int index) const
{
  return m_LockedOrder.find(m_ESPs.at(index).m_Name.toLower()) != m_LockedOrder.end();
}


void PluginList::lockESPIndex(int index, bool lock)
{
  if (lock) {
    m_LockedOrder[getName(index).toLower()] = m_ESPs.at(index).m_LoadOrder;
  } else {
    auto iter = m_LockedOrder.find(getName(index).toLower());
    if (iter != m_LockedOrder.end()) {
      m_LockedOrder.erase(iter);
    }
  }
  m_Modified = true;
  emit esplist_changed();
}


void PluginList::syncLoadOrder()
{
  int loadOrder = 0;
  for (unsigned int i = 0; i < m_ESPs.size(); ++i) {
    int index = m_ESPsByPriority[i];

    if (m_ESPs[index].m_Enabled) {
      m_ESPs[index].m_LoadOrder = loadOrder++;
    } else {
      m_ESPs[index].m_LoadOrder = -1;
    }
  }
}

void PluginList::refreshLoadOrder()
{
  syncLoadOrder();
  // set priorities according to locked load order
  std::map<int, QString> lockedLoadOrder;
  std::for_each(m_LockedOrder.begin(), m_LockedOrder.end(),
                [&lockedLoadOrder] (const std::pair<QString, int> &ele) { lockedLoadOrder[ele.second] = ele.first; });

  int targetPrio = 0;
  // this is guaranteed to iterate from lowest key (load order) to highest
  for (auto iter = lockedLoadOrder.begin(); iter != lockedLoadOrder.end(); ++iter) {
    auto nameIter = m_ESPsByName.find(iter->second);
    if (nameIter != m_ESPsByName.end()) {
      // locked esp exists

      // find the location to insert at
      while ((targetPrio < static_cast<int>(m_ESPs.size())) &&
             (m_ESPs[m_ESPsByPriority[targetPrio]].m_LoadOrder < iter->first)) {
        if (QString::compare(m_ESPs[m_ESPsByPriority[targetPrio]].m_Name, iter->second) != 0) {
          ++targetPrio;
        }
      }

      if (static_cast<size_t>(targetPrio) >= m_ESPs.size()) {
        continue;
      }

      int temp = targetPrio;
      int index = nameIter->second;
      if (m_ESPs[index].m_Priority != temp) {
        setPluginPriority(index, temp);
        m_ESPs[index].m_LoadOrder = iter->first;
        syncLoadOrder();
        m_Modified = true;
      }
    }
  }
}


void PluginList::updateIndices()
{
  m_ESPsByName.clear();
  m_ESPsByPriority.clear();
  m_ESPsByPriority.resize(m_ESPs.size());

  for (unsigned int i = 0; i < m_ESPs.size(); ++i) {
    m_ESPsByName[m_ESPs[i].m_Name.toLower()] = i;
    m_ESPsByPriority[m_ESPs[i].m_Priority] = i;
  }
}


int PluginList::rowCount(const QModelIndex &parent) const
{
  if (!parent.isValid()) {
    return m_ESPs.size();
  } else {
    return 0;
  }
}

int PluginList::columnCount(const QModelIndex &) const
{
  return 3;
}


QVariant PluginList::data(const QModelIndex &modelIndex, int role) const
{
  int index = modelIndex.row();

  if (role == Qt::DisplayRole) {
    switch (modelIndex.column()) {
      case COL_NAME: {
        return m_ESPs[index].m_Name;
      } break;
      case COL_PRIORITY: {
        if (m_ESPs[index].m_Priority == 0) {
          return tr("min");
        } else if (m_ESPs[index].m_Priority == static_cast<int>(m_ESPs.size()) - 1) {
          return tr("max");
        } else {
          return QString::number(m_ESPs[index].m_Priority);
        }
      } break;
      case COL_MODINDEX: {
        if (m_ESPs[index].m_LoadOrder == -1) {
          return QString();
        } else {
          return QString("%1").arg(m_ESPs[index].m_LoadOrder, 2, 16, QChar('0')).toUpper();
        }
      } break;
      default: {
          return QVariant();
      } break;
    }
  } else if ((role == Qt::DecorationRole) && (modelIndex.column() == 0)) {
    if (m_LockedOrder.find(m_ESPs[index].m_Name.toLower()) != m_LockedOrder.end()) {
      return QIcon(":/MO/gui/locked");
    } else {
      return QVariant();
    }
  } else if ((role == Qt::CheckStateRole) && (modelIndex.column() == 0)) {
    return m_ESPs[index].m_Enabled ? Qt::Checked : Qt::Unchecked;
  } else if (role == Qt::TextAlignmentRole) {
    if (modelIndex.column() == 0) {
      return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
    } else {
      return QVariant(Qt::AlignHCenter | Qt::AlignVCenter);
    }
  } else if (role == Qt::ToolTipRole) {
    if (m_ESPs[index].m_ForceEnabled) {
      return tr("This plugin can't be disabled (enforced by the game)");
    } else {
      return tr("Origin: %1").arg(m_ESPs[index].m_OriginName);
    }
  } else {
    return QVariant();
  }
}


bool PluginList::setData(const QModelIndex &index, const QVariant &value, int role)
{
  if (role == Qt::CheckStateRole) {
    m_ESPs[index.row()].m_Enabled = value.toInt() == Qt::Checked;

    emit layoutAboutToBeChanged();

    refreshLoadOrder();
    m_Modified = true;
    emit esplist_changed();

    emit layoutChanged();
    return true;
  } else {
    return false;
  }
}


QVariant PluginList::headerData(int section, Qt::Orientation orientation,
                             int role) const
{
  if (orientation == Qt::Horizontal) {
    if (role == Qt::DisplayRole) {
      return getColumnName(section);
    } else if (role == Qt::ToolTipRole) {
      return getColumnToolTip(section);
    } else if (role == Qt::SizeHintRole) {
      QSize temp = m_FontMetrics.size(Qt::TextSingleLine, getColumnName(section));
      temp.rwidth() += 25;
      temp.rheight() += 12;
      return temp;
    }
  }
  return QAbstractItemModel::headerData(section, orientation, role);
}


Qt::ItemFlags PluginList::flags(const QModelIndex &modelIndex) const
{
  int index = modelIndex.row();
  Qt::ItemFlags result = QAbstractTableModel::flags(modelIndex);

  if (modelIndex.isValid())  {
    if ((m_ESPs[index].m_ForceEnabled)) {
      result &= ~Qt::ItemIsEnabled;
    }
    result |= Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled;
  } else {
    result |= Qt::ItemIsDropEnabled;
  }

  return result;
}


void PluginList::setPluginPriority(int row, int &newPriority)
{
  int newPriorityTemp = newPriority;

  QString sourceExtension = m_ESPs[row].m_Name.right(3).toLower();
  if (sourceExtension == "esp") {
    // don't allow esps to be moved above esms
    while ((newPriorityTemp < static_cast<int>(m_ESPsByPriority.size() - 1)) &&
           (m_ESPs[m_ESPsByPriority[newPriorityTemp]].m_Name.right(3).toLower() == "esm")) {
      ++newPriorityTemp;
    }
  } else {
    // don't allow esms to be moved below esps
    while ((newPriorityTemp > 0) &&
           (m_ESPs[m_ESPsByPriority[newPriorityTemp - 1]].m_Name.right(3).toLower() == "esp")) {
      --newPriorityTemp;
    }
    // also don't allow "regular" esms to be moved above primary plugins
    while ((newPriorityTemp < static_cast<int>(m_ESPsByPriority.size() - 1)) &&
           (m_ESPs[m_ESPsByPriority[newPriorityTemp]].m_ForceEnabled)) {
      ++newPriorityTemp;
    }
  }

  int oldPriority = m_ESPs[row].m_Priority;
  if (newPriorityTemp > oldPriority) {
    // priority is higher than the old, so the gap we left is in lower priorities
    for (int i = oldPriority + 1; i <= newPriorityTemp; ++i) {
      --m_ESPs[m_ESPsByPriority[i]].m_Priority;
    }
  } else {
    for (int i = newPriorityTemp; i < oldPriority; ++i) {
      ++m_ESPs[m_ESPsByPriority[i]].m_Priority;
    }
    ++newPriority;
  }

  m_ESPs[row].m_Priority = newPriorityTemp;
  updateIndices();
}


void PluginList::changePluginPriority(std::vector<int> rows, int newPriority)
{
  emit layoutAboutToBeChanged();
  // sort rows to insert by their old priority (ascending) and insert them move them in that order
  const std::vector<ESPInfo> &esp = m_ESPs;
  std::sort(rows.begin(), rows.end(),
            [&esp](const int &LHS, const int &RHS) {
              return esp[LHS].m_Priority < esp[RHS].m_Priority;
            });

  // odd stuff: if any of the dragged sources has priority lower than the destination then the
  // target idx is that of the row BELOW the dropped location, otherwise it's the one above. why?
  for (std::vector<int>::const_iterator iter = rows.begin();
       iter != rows.end(); ++iter) {
    if (m_ESPs[*iter].m_Priority < newPriority) {
      --newPriority;
      break;
    }
  }

  for (std::vector<int>::const_iterator iter = rows.begin(); iter != rows.end(); ++iter) {
    setPluginPriority(*iter, newPriority);
  }
  refreshLoadOrder();

  emit esplist_changed();

  m_Modified = true;

  emit layoutChanged();
}


bool PluginList::dropMimeData(const QMimeData *mimeData, Qt::DropAction action, int row, int, const QModelIndex &parent)
{
  if (action == Qt::IgnoreAction) {
    return true;
  }

  QByteArray encoded = mimeData->data("application/x-qabstractitemmodeldatalist");
  QDataStream stream(&encoded, QIODevice::ReadOnly);

  std::vector<int> sourceRows;

  while (!stream.atEnd()) {
    int sourceRow, col;
    QMap<int,  QVariant> roleDataMap;
    stream >> sourceRow >> col >> roleDataMap;
    if (col == 0) { // only add each row once
      sourceRows.push_back(sourceRow);
    }
  }

  if (row == -1) {
    row = parent.row();
  }

  int newPriority = 0;

  if ((row < 0) ||
      (row >= static_cast<int>(m_ESPs.size()))) {
    newPriority = m_ESPs.size();
  } else {
    newPriority = m_ESPs[row].m_Priority;
  }
  changePluginPriority(sourceRows, newPriority);


  return false;
}


bool PluginList::eventFilter(QObject *obj, QEvent *event)
{
  if (event->type() == QEvent::KeyPress) {
    QAbstractItemView *itemView = qobject_cast<QAbstractItemView*>(obj);
    QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
    if ((itemView != NULL) &&
        (keyEvent->modifiers() == Qt::ControlModifier) &&
        ((keyEvent->key() == Qt::Key_Up) || (keyEvent->key() == Qt::Key_Down))) {
      QItemSelectionModel *selectionModel = itemView->selectionModel();
      const QSortFilterProxyModel *proxyModel = qobject_cast<const QSortFilterProxyModel*>(selectionModel->model());
      int diff = -1;
      if (((keyEvent->key() == Qt::Key_Up) && (proxyModel->sortOrder() == Qt::DescendingOrder)) ||
          ((keyEvent->key() == Qt::Key_Down) && (proxyModel->sortOrder() == Qt::AscendingOrder))) {
        diff = 1;
      }
      QModelIndexList rows = selectionModel->selectedRows();
      if (keyEvent->key() == Qt::Key_Down) {
        for (int i = 0; i < rows.size() / 2; ++i) {
          rows.swap(i, rows.size() - i - 1);
        }
      }
      foreach (QModelIndex idx, rows) {
        if (proxyModel != NULL) {
          idx = proxyModel->mapToSource(idx);
        }
        int newPriority = m_ESPs[idx.row()].m_Priority + diff;
        if ((newPriority >= 0) && (newPriority < rowCount())) {
          setPluginPriority(idx.row(), newPriority);
          emit dataChanged(this->index(idx.row(), 0), this->index(idx.row(), this->columnCount() - 1));
        }
      }
      refreshLoadOrder();
      return true;
    }
  }
  return QObject::eventFilter(obj, event);
}
