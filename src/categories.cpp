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


using namespace MOBase;


CategoryFactory* CategoryFactory::s_Instance = nullptr;


QString CategoryFactory::categoriesFilePath()
{
  return qApp->property("dataPath").toString() + "/categories.dat";
}


CategoryFactory::CategoryFactory()
{
  atexit(&cleanup);
}

void CategoryFactory::loadCategories()
{
  reset();

  QFile categoryFile(categoriesFilePath());

  if (!categoryFile.open(QIODevice::ReadOnly)) {
    loadDefaultCategories();
  } else {
    int lineNum = 0;
    while (!categoryFile.atEnd()) {
      QByteArray line = categoryFile.readLine();
      ++lineNum;
      QList<QByteArray> cells = line.split('|');
      if (cells.count() != 4) {
        log::error(
          "invalid category line {}: {} ({} cells)",
          lineNum, line.constData(), cells.count());
      } else {
        std::vector<int> nexusIDs;
        if (cells[2].length() > 0) {
          QList<QByteArray> nexusIDStrings = cells[2].split(',');
          for (QList<QByteArray>::iterator iter = nexusIDStrings.begin();
               iter != nexusIDStrings.end(); ++iter) {
            bool ok = false;
            int temp = iter->toInt(&ok);
            if (!ok) {
              log::error("invalid category id {}", iter->constData());
            }
            nexusIDs.push_back(temp);
          }
        }
        bool cell0Ok = true;
        bool cell3Ok = true;
        int id = cells[0].toInt(&cell0Ok);
        int parentID = cells[3].trimmed().toInt(&cell3Ok);
        if (!cell0Ok || !cell3Ok) {
          log::error("invalid category line {}: {}", lineNum, line.constData());
        }
        addCategory(id, QString::fromUtf8(cells[1].constData()), nexusIDs, parentID);
      }
    }
    categoryFile.close();
  }
  std::sort(m_Categories.begin(), m_Categories.end());
  setParents();
}


CategoryFactory &CategoryFactory::instance()
{
  if (s_Instance == nullptr) {
    s_Instance = new CategoryFactory;
  }
  return *s_Instance;
}


void CategoryFactory::reset()
{
  m_Categories.clear();
  m_IDMap.clear();
  // 28 =
  // 43 = Savegames (makes no sense to install them through MO)
  // 45 = Videos and trailers
  // 87 = Miscelanous
  addCategory(0, "None", { 4, 28, 43, 45, 87 }, 0);
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

void CategoryFactory::cleanup()
{
  delete s_Instance;
  s_Instance = nullptr;
}


void CategoryFactory::saveCategories()
{
  QFile categoryFile(categoriesFilePath());

  if (!categoryFile.open(QIODevice::WriteOnly)) {
    reportError(QObject::tr("Failed to save custom categories"));
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
        .append(VectorJoin(iter->m_NexusIDs, ",").toUtf8()).append("|")
        .append(QByteArray::number(iter->m_ParentID)).append("\n");
    categoryFile.write(line);
  }
  categoryFile.close();
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

int CategoryFactory::addCategory(const QString &name, const std::vector<int> &nexusIDs, int parentID)
{
  int id = 1;
  while (m_IDMap.find(id) != m_IDMap.end()) {
    ++id;
  }
  addCategory(id, name, nexusIDs, parentID);

  saveCategories();
  return id;
}

void CategoryFactory::addCategory(int id, const QString &name, const std::vector<int> &nexusIDs, int parentID)
{
  int index = static_cast<int>(m_Categories.size());
  m_Categories.push_back(Category(index, id, name, nexusIDs, parentID));
  for (int nexusID : nexusIDs) {
    m_NexusMap[nexusID] = index;
  }
  m_IDMap[id] = index;
}


void CategoryFactory::loadDefaultCategories()
{
  // the order here is relevant as it defines the order in which the
  // mods appear in the combo box
  addCategory(1, "Animations", { 2, 4, 51 }, 0);
  addCategory(52, "Poses", { 1, 29 }, 1);
  addCategory(2, "Armour", { 2, 5, 54 }, 0);
  addCategory(53, "Power Armor", { 1, 53 }, 2);
  addCategory(3, "Audio", { 3, 33, 35, 106 }, 0);
  addCategory(38, "Music", { 2, 34, 61 }, 0);
  addCategory(39, "Voice", { 2, 36, 107 }, 0);
  addCategory(5, "Clothing", { 2, 9, 60 }, 0);
  addCategory(41, "Jewelry", { 1, 102 }, 5);
  addCategory(42, "Backpacks", { 1, 49 }, 5);
  addCategory(6, "Collectables", { 2, 10, 92 }, 0);
  addCategory(28, "Companions", { 3, 11, 66, 96 }, 0);
  addCategory(7, "Creatures, Mounts, & Vehicles", { 4, 12, 65, 83, 101 }, 0);
  addCategory(8, "Factions", { 2, 16, 25 }, 0);
  addCategory(9, "Gameplay", { 2, 15, 24 }, 0);
  addCategory(27, "Combat", { 1, 77 }, 9);
  addCategory(43, "Crafting", { 2, 50, 100 }, 9);
  addCategory(48, "Overhauls", { 2, 24, 79 }, 9);
  addCategory(49, "Perks", { 1, 27 }, 9);
  addCategory(54, "Radio", { 1, 31 }, 9);
  addCategory(55, "Shouts", { 1, 104 }, 9);
  addCategory(22, "Skills & Levelling", { 2, 46, 73 }, 9);
  addCategory(58, "Weather & Lighting", { 1, 56 }, 9);
  addCategory(44, "Equipment", { 1, 44 }, 43);
  addCategory(45, "Home/Settlement", { 1, 45 }, 43);
  addCategory(10, "Body, Face, & Hair", { 2, 17, 26 }, 0);
  addCategory(56, "Tattoos", { 1, 57 }, 10);
  addCategory(40, "Character Presets", { 1, 58 }, 0);
  addCategory(11, "Items", { 2, 27, 85 }, 0);
  addCategory(32, "Mercantile", { 2, 23, 69 }, 0);
  addCategory(37, "Ammo", { 1, 3 }, 11);
  addCategory(19, "Weapons", { 2, 41, 55 }, 11);
  addCategory(36, "Weapon & Armour Sets", { 1, 42 }, 11);
  addCategory(23, "Player Homes", { 2, 28, 67 }, 0);
  addCategory(25, "Castles & Mansions", { 1, 68 }, 23);
  addCategory(51, "Settlements", { 1, 48 }, 23);
  addCategory(12, "Locations", { 10, 20, 21, 22, 30, 47, 70, 88, 89, 90, 91 }, 0);
  addCategory(4, "Cities", { 1, 53 }, 12);
  addCategory(31, "Landscape Changes", { 1, 58 }, 0);
  addCategory(29, "Environment", { 2, 14, 74 }, 0);
  addCategory(30, "Immersion", { 2, 51, 78 }, 0);
  addCategory(20, "Magic", { 3, 75, 93, 94 }, 0);
  addCategory(21, "Models & Textures", { 2, 19, 29 }, 0);
  addCategory(33, "Modders resources", { 2, 18, 82 }, 0);
  addCategory(13, "NPCs", { 3, 22, 33, 99 }, 0);
  addCategory(24, "Bugfixes", { 2, 6, 95 }, 0);
  addCategory(14, "Patches", { 2, 25, 84 }, 24);
  addCategory(35, "Utilities", { 2, 38, 39 }, 0);
  addCategory(26, "Cheats", { 1, 8 }, 0);
  addCategory(15, "Quests", { 2, 30, 35 }, 0);
  addCategory(16, "Races & Classes", { 1, 34 }, 0);
  addCategory(34, "Stealth", { 1, 76 }, 0);
  addCategory(17, "UI", { 2, 37, 42 }, 0);
  addCategory(18, "Visuals", { 2, 40, 62 }, 0);
  addCategory(50, "Pip-Boy", { 1, 52 }, 18);
  addCategory(46, "Shader Presets", { 3, 13, 97, 105 }, 0);
  addCategory(47, "Miscellaneous", { 2, 2, 28 }, 0);
}


int CategoryFactory::getParentID(unsigned int index) const
{
  if (index >= m_Categories.size()) {
    throw MyException(QObject::tr("invalid category index: %1").arg(index));
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
    log::warn("{} is no valid category id", id);
    return false;
  }
}


bool CategoryFactory::hasChildren(unsigned int index) const
{
  if (index >= m_Categories.size()) {
    throw MyException(QObject::tr("invalid category index: %1").arg(index));
  }

  return m_Categories[index].m_HasChildren;
}


QString CategoryFactory::getCategoryName(unsigned int index) const
{
  if (index >= m_Categories.size()) {
    throw MyException(QObject::tr("invalid category index: %1").arg(index));
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
    throw MyException(QObject::tr("invalid category index: %1").arg(index));
  }

  return m_Categories[index].m_ID;
}


int CategoryFactory::getCategoryIndex(int ID) const
{
  std::map<int, unsigned int>::const_iterator iter = m_IDMap.find(ID);
  if (iter == m_IDMap.end()) {
    throw MyException(QObject::tr("invalid category id: %1").arg(ID));
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
  std::map<int, unsigned int>::const_iterator iter = m_NexusMap.find(nexusID);
  if (iter != m_NexusMap.end()) {
    log::debug("nexus category id {} maps to internal {}", nexusID, iter->second);
    return iter->second;
  } else {
    log::debug("nexus category id {} not mapped", nexusID);
    return 0U;
  }
}
