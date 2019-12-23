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

#ifndef CATEGORIES_H
#define CATEGORIES_H


#include <QString>
#include <vector>
#include <map>
#include <functional>


/**
 * @brief Manage the available mod categories
 * @warning member functions of this class currently use a wild mix of ids and indexes to look up categories,
 *          optimized to where the request comes from. Therefore be very careful which of the two you have available
 **/
class CategoryFactory : QObject {
  Q_OBJECT;

  friend class CategoriesDialog;

public:
  enum SpecialCategories
  {
    Checked = 10000,
    UpdateAvailable,
    HasCategory,
    Conflict,
    HasHiddenFiles,
    Endorsed,
    Backup,
    Managed,
    HasGameData,
    HasNexusID,
    Tracked
  };

public:
  struct Category {
    Category(int sortValue, int id, const QString &name, int parentID)
      : m_SortValue(sortValue), m_ID(id), m_Name(name), m_HasChildren(false), m_ParentID(parentID) {}
    int m_SortValue;
    int m_ID;
    int m_ParentID;
    bool m_HasChildren;
    QString m_Name;

    friend bool operator<(const Category &LHS, const Category &RHS) {
      return LHS.m_SortValue < RHS.m_SortValue;
    }
  };

  struct NexusCategory {
    NexusCategory(const QString &name, const int nexusID)
      : m_Name(name), m_ID(nexusID) {}
    QString m_Name;
    int m_ID;
  };

public:

  /**
   * @brief reset the list of categories
   **/
  void reset();

  /**
   * @brief read categories from file
   */
  void loadCategories();

  /**
   * @brief save the categories to the categories.dat file
   **/
  void saveCategories();

  int addCategory(const QString &name, const std::vector<NexusCategory> &nexusCats, int parentID);

  /**
   * @brief retrieve the number of available categories
   *
   * @return unsigned int number of categories
   **/
   size_t numCategories() const { return m_Categories.size(); }

  /**
   * @brief count all categories that match a specified filter
   * @param filter the filter to test
   * @return number of matching categories
   */
  unsigned int countCategories(std::function<bool (const Category &category)> filter);

  /**
   * @brief get the id of the parent category
   *
   * @param index the index to look up
   * @return int id of the parent category
   **/
  int getParentID(unsigned int index) const;

  /**
   * @brief determine if a category exists (by id)
   *
   * @param id the id to check for existance
   * @return true if the category exists, false otherwise
   **/
  bool categoryExists(int id) const;

  /**
   * @brief test if a category is child of a second one
   * @param id       the presumed child id
   * @param parentID the parent id to test for
   * @return true if id is a child of parentID
   **/
  bool isDecendantOf(int id, int parentID) const;

  /**
   * @brief test if the specified category has child categories
   *
   * @param index index of the category to look up
   * @return bool true if the category has child categories
   **/
  bool hasChildren(unsigned int index) const;

  /**
   * @brief retrieve the name of a category
   *
   * @param index index of the category to look up
   * @return QString name of the category
   **/
  QString getCategoryName(unsigned int index) const;
  QString getSpecialCategoryName(SpecialCategories type) const;
  QString getCategoryNameByID(int id) const;

  /**
   * @brief look up the id of a category by its index
   *
   * @param index index of the category to look up
   * @return int id of the category
   **/
  int getCategoryID(unsigned int index) const;

  /**
   * @brief look up the id of a category by its name
   * @note O(n)
   */
  int getCategoryID(const QString &name) const;

  /**
   * @brief look up the index of a category by its id
   *
   * @param id index of the category to look up
   * @return unsigned int index of the category
   **/
  int getCategoryIndex(int ID) const;

  /**
   * @brief retrieve the index of a category by its nexus id
   *
   * @param nexusID nexus id of the category to look up
   * @return unsigned int index of the category or 0 if no category matches
   **/
  unsigned int resolveNexusID(int nexusID) const;

public:

  /**
   * @brief retrieve a reference to the singleton instance
   *
   * @return the reference to the singleton
   **/
  static CategoryFactory *instance();

  /**
   * @return path to the file that contains the categories list
   */
  static QString categoriesFilePath();

  /**
   * @return path to the file that contains the nexus category mappings
   */
  static QString nexusMappingFilePath();

public slots:

  void mapNexusCategories(QString, QVariant, QVariant data);

signals:

  void requestNexusCategories();

private:

  CategoryFactory();

  void loadDefaultCategories();

  void addCategory(int id, const QString &name, const std::vector<NexusCategory> &nexusCats, int parentID);
  void addCategory(int id, const QString& name, int parentID);

  void setParents();

  static void cleanup();

private:

  static CategoryFactory *s_Instance;

  std::vector<Category> m_Categories;
  std::map<int, unsigned int> m_IDMap;
  std::map<NexusCategory, unsigned int> m_NexusMap;

private:

};


#endif // CATEGORIES_H
