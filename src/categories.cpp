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

#include "categories.h"

#include <utility.h>
#include <report.h>
#include <log.h>

#include <QObject>
#include <QFile>
#include <QDir>
#include <QList>
#include <QCoreApplication>

#include "nexusinterface.h"

using namespace MOBase;


QString CategoryFactory::categoriesFilePath()
{
  return qApp->property("dataPath").toString() + "/categories.dat";
}


QString CategoryFactory::nexusMappingFilePath()
{
  return qApp->property("dataPath").toString() + "/nexuscatmap.dat";
}


CategoryFactory::CategoryFactory() : QObject()
{
}

void CategoryFactory::loadCategories()
{
  reset();

  QFile categoryFile(categoriesFilePath());
  bool needLoad = false;

  if (!categoryFile.open(QIODevice::ReadOnly)) {
    needLoad = true;
  } else {
    int lineNum = 0;
    while (!categoryFile.atEnd()) {
      QByteArray line = categoryFile.readLine();
      ++lineNum;
      QList<QByteArray> cells = line.split('|');
      if (cells.count() == 4) {
        std::vector<NexusCategory> nexusCats;
        if (cells[2].length() > 0) {
          QList<QByteArray> nexusIDStrings = cells[2].split(',');
          for (QList<QByteArray>::iterator iter = nexusIDStrings.begin();
               iter != nexusIDStrings.end(); ++iter) {
            bool ok = false;
            int temp = iter->toInt(&ok);
            if (!ok) {
              log::error(tr("invalid category id {}").toStdString(), iter->constData());
            }
            nexusCats.push_back(NexusCategory("Unknown", temp));
          }
        }
        bool cell0Ok = true;
        bool cell3Ok = true;
        int id = cells[0].toInt(&cell0Ok);
        int parentID = cells[3].trimmed().toInt(&cell3Ok);
        if (!cell0Ok || !cell3Ok) {
          log::error(tr("invalid category line {}: {}").toStdString(), lineNum, line.constData());
        }
        addCategory(id, QString::fromUtf8(cells[1].constData()), nexusCats, parentID);
      } else if (cells.count() == 3) {
          bool cell0Ok = true;
          bool cell3Ok = true;
          int id = cells[0].toInt(&cell0Ok);
          int parentID = cells[2].trimmed().toInt(&cell3Ok);
          if (!cell0Ok || !cell3Ok) {
            log::error(tr("invalid category line {}: {}").toStdString(), lineNum, line.constData());
          }

          addCategory(id, QString::fromUtf8(cells[1].constData()), std::vector<NexusCategory>(), parentID);
      } else {
        log::error(
          tr("invalid category line {}: {} ({} cells)").toStdString(),
          lineNum, line.constData(), cells.count());
      }
    }
    categoryFile.close();

    QFile nexusMapFile(nexusMappingFilePath());
    if (!nexusMapFile.open(QIODevice::ReadOnly)) {
      needLoad = true;
    } else {
      int nexLineNum = 0;
      while (!nexusMapFile.atEnd()) {
        QByteArray nexLine = nexusMapFile.readLine();
        ++nexLineNum;
        QList<QByteArray> nexCells = nexLine.split('|');
        if (nexCells.count() == 3) {
          std::vector<NexusCategory> nexusCats;
          QString nexName = nexCells[1];
          bool ok = false;
          int nexID = nexCells[2].toInt(&ok);
          if (!ok) {
            log::error(tr("invalid nexus ID {}").toStdString(), nexCells[2].constData());
          }
          int catID = nexCells[0].toInt(&ok);
          if (!ok) {
            log::error(tr("invalid category id {}").toStdString(), nexCells[0].constData());
          }
          m_NexusMap.insert_or_assign(nexID, NexusCategory(nexName, nexID));
          m_NexusMap.at(nexID).m_CategoryID = catID;
        } else {
          log::error(
            tr("invalid nexus category line {}: {} ({} cells)").toStdString(),
            lineNum, nexLine.constData(), nexCells.count());
        }
      }
    }
    nexusMapFile.close();
  }
  std::sort(m_Categories.begin(), m_Categories.end());
  setParents();
  if (needLoad) loadDefaultCategories();
}


CategoryFactory *CategoryFactory::instance()
{
  static CategoryFactory s_Instance;
  return &s_Instance;
}


void CategoryFactory::reset()
{
  m_Categories.clear();
  m_IDMap.clear();
  // 28 =
  // 43 = Savegames (makes no sense to install them through MO)
  // 45 = Videos and trailers
  // 87 = Miscelanous
  addCategory(0, "None", std::vector<NexusCategory>(), 0);
}


void CategoryFactory::setParents()
{
  for (std::vector<Category>::iterator iter = m_Categories.begin();
       iter != m_Categories.end(); ++iter) {
    iter->m_HasChildren = false;
  }

  for (std::vector<Category>::const_iterator categoryIter = m_Categories.begin();
       categoryIter != m_Categories.end(); ++categoryIter) {
    if (categoryIter->m_ParentID != 0) {
      std::map<int, unsigned int>::const_iterator iter = m_IDMap.find(categoryIter->m_ParentID);
      if (iter != m_IDMap.end()) {
        m_Categories[iter->second].m_HasChildren = true;
      }
    }
  }
}


void CategoryFactory::saveCategories()
{
  QFile categoryFile(categoriesFilePath());

  if (!categoryFile.open(QIODevice::WriteOnly)) {
    reportError(tr("Failed to save custom categories"));
    return;
  }

  categoryFile.resize(0);
  for (std::vector<Category>::const_iterator iter = m_Categories.begin();
       iter != m_Categories.end(); ++iter) {
    if (iter->m_ID == 0) {
      continue;
    }
    QByteArray line;
    line.append(QByteArray::number(iter->m_ID)).append("|")
        .append(iter->m_Name.toUtf8()).append("|")
        .append(QByteArray::number(iter->m_ParentID)).append("\n");
    categoryFile.write(line);
  }
  categoryFile.close();

  QFile nexusMapFile(nexusMappingFilePath());

  if (!nexusMapFile.open(QIODevice::WriteOnly)) {
    reportError(tr("Failed to save nexus category mappings"));
    return;
  }

  nexusMapFile.resize(0);
  for (auto iter = m_NexusMap.begin(); iter != m_NexusMap.end(); ++iter) {
    QByteArray line;
    line.append(QByteArray::number(iter->second.m_CategoryID)).append("|");
    line.append(iter->second.m_Name.toUtf8()).append("|");
    line.append(QByteArray::number(iter->second.m_ID)).append("\n");
    nexusMapFile.write(line);
  }
  nexusMapFile.close();
}


unsigned int CategoryFactory::countCategories(std::function<bool (const Category &category)> filter)
{
  unsigned int result = 0;
  for (const Category &cat : m_Categories) {
    if (filter(cat)) {
      ++result;
    }
  }
  return result;
}

int CategoryFactory::addCategory(const QString &name, const std::vector<NexusCategory>& nexusCats, int parentID)
{
  int id = 1;
  while (m_IDMap.find(id) != m_IDMap.end()) {
    ++id;
  }
  addCategory(id, name, nexusCats, parentID);

  saveCategories();
  return id;
}

void CategoryFactory::addCategory(int id, const QString &name, int parentID)
{
  int index = static_cast<int>(m_Categories.size());
  m_Categories.push_back(Category(index, id, name, parentID, std::vector<NexusCategory>()));
  m_IDMap[id] = index;
}

void CategoryFactory::addCategory(int id, const QString& name, const std::vector<NexusCategory>& nexusCats, int parentID)
{
  for (auto nexusCat : nexusCats) {
    m_NexusMap.insert_or_assign(nexusCat.m_ID, nexusCat);
    m_NexusMap.at(nexusCat.m_ID).m_CategoryID = id;
  }
  int index = static_cast<int>(m_Categories.size());
  m_Categories.push_back(Category(index, id, name, parentID, nexusCats));
  m_IDMap[id] = index;
}


void CategoryFactory::loadDefaultCategories()
{
  // the order here is relevant as it defines the order in which the
  // mods appear in the combo box
  //if (QMessageBox::question(nullptr, tr("Load Nexus Categories?"),
  //  tr("This is either a new or old instance which lacks modern Nexus category mappings. Would you like to import and map categories from Nexus now?"),
  //  QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
  //  emit requestNexusCategories();
  //}
}


void CategoryFactory::mapNexusCategories(QString, QVariant, QVariant result)
{

}


int CategoryFactory::getParentID(unsigned int index) const
{
  if (index >= m_Categories.size()) {
    throw MyException(tr("invalid category index: %1").arg(index));
  }

  return m_Categories[index].m_ParentID;
}


bool CategoryFactory::categoryExists(int id) const
{
  return m_IDMap.find(id) != m_IDMap.end();
}


bool CategoryFactory::isDescendantOf(int id, int parentID) const
{
  // handles cycles
  std::set<int> seen;
  return isDescendantOfImpl(id, parentID, seen);
}

bool CategoryFactory::isDescendantOfImpl(
  int id, int parentID, std::set<int>& seen) const
{
  if (!seen.insert(id).second) {
    log::error("cycle in category: {}", id);
    return false;
  }

  std::map<int, unsigned int>::const_iterator iter = m_IDMap.find(id);

  if (iter != m_IDMap.end()) {
    unsigned int index = iter->second;
    if (m_Categories[index].m_ParentID == 0) {
      return false;
    } else if (m_Categories[index].m_ParentID == parentID) {
      return true;
    } else {
      return isDescendantOfImpl(m_Categories[index].m_ParentID, parentID, seen);
    }
  } else {
    log::warn(tr("{} is no valid category id").toStdString(), id);
    return false;
  }
}


bool CategoryFactory::hasChildren(unsigned int index) const
{
  if (index >= m_Categories.size()) {
    throw MyException(tr("invalid category index: %1").arg(index));
  }

  return m_Categories[index].m_HasChildren;
}


QString CategoryFactory::getCategoryName(unsigned int index) const
{
  if (index >= m_Categories.size()) {
    throw MyException(tr("invalid category index: %1").arg(index));
  }

  return m_Categories[index].m_Name;
}

QString CategoryFactory::getSpecialCategoryName(SpecialCategories type) const
{
  QString label;
  switch (type)
  {
    case Checked:         label = QObject::tr("Active"); break;
    case UpdateAvailable: label = QObject::tr("Update available"); break;
    case HasCategory:     label = QObject::tr("Has category"); break;
    case Conflict:        label = QObject::tr("Conflicted"); break;
    case HasHiddenFiles:  label = QObject::tr("Has hidden files"); break;
    case Endorsed:        label = QObject::tr("Endorsed"); break;
    case Backup:          label = QObject::tr("Has backup"); break;
    case Managed:         label = QObject::tr("Managed"); break;
    case HasGameData:     label = QObject::tr("Has valid game data"); break;
    case HasNexusID:      label = QObject::tr("Has Nexus ID"); break;
    case Tracked:         label = QObject::tr("Tracked on Nexus"); break;
    default: return {};
  }
  return QString("<%1>").arg(label);
}

QString CategoryFactory::getCategoryNameByID(int id) const
{
  auto itor = m_IDMap.find(id);

  if (itor == m_IDMap.end()) {
    return getSpecialCategoryName(static_cast<SpecialCategories>(id));
  } else {
    const auto index = itor->second;
    if (index >= m_Categories.size()) {
      return {};
    }

    return m_Categories[index].m_Name;
  }
}

int CategoryFactory::getCategoryID(unsigned int index) const
{
  if (index >= m_Categories.size()) {
    throw MyException(tr("invalid category index: %1").arg(index));
  }

  return m_Categories[index].m_ID;
}


int CategoryFactory::getCategoryIndex(int ID) const
{
  std::map<int, unsigned int>::const_iterator iter = m_IDMap.find(ID);
  if (iter == m_IDMap.end()) {
    throw MyException(tr("invalid category id: %1").arg(ID));
  }
  return iter->second;
}


int CategoryFactory::getCategoryID(const QString &name) const
{
  auto iter = std::find_if(m_Categories.begin(), m_Categories.end(),
                           [name] (const Category &cat) -> bool {
    return cat.m_Name == name;
  });

  if (iter != m_Categories.end()) {
    return iter->m_ID;
  } else {
    return -1;
  }
}


unsigned int CategoryFactory::resolveNexusID(int nexusID) const
{
  auto result = m_NexusMap.find(nexusID);
  if (result != m_NexusMap.end()) {
    log::debug(tr("nexus category id {} maps to internal {}").toStdString(), nexusID, result->second.m_ID);
    return m_IDMap.at(result->second.m_CategoryID);
  } else {
    log::debug(tr("nexus category id {} not mapped").toStdString(), nexusID);
    return 0U;
  }
}
