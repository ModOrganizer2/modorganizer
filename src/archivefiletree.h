/*
Copyright (C) MO2 Team. All rights reserved.

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

#ifndef ARCHIVEFILENETRY_H
#define ARCHIVEFILENTRY_H

#include <archive/archive.h>
#include <uibase/ifiletree.h>

/**
 *
 */
class ArchiveFileTree : public virtual MOBase::IFileTree
{
public:
  /**
   * @brief Create a new file tree representing the given archive.
   *
   * @param archive Archive to represent by the file tree.
   *
   * @return a file tree representing the given archive.
   */
  static std::shared_ptr<ArchiveFileTree> makeTree(Archive const& archive);

  /**
   * @brief Update the given archive to reflect change in this tree.
   *
   * This method disables files that have been removed from the file
   * tree and move the ones that have been moved.
   *
   * @param archive The archive to update. Must be the one used to
   *     create the tree.
   */
  virtual void mapToArchive(Archive& archive) const = 0;

  /**
   * @brief Update the given archive to prepare for the extraction
   *     of the given entries.
   *
   * This method "enables" files that correspond to the given entry.
   *
   * @param archive The archive to update. Must be the one used to
   *     create the tree.
   * @param entries List of entries to mark for extraction. All the entries must
   *     come from a tree created with the given archive.
   */
  static void
  mapToArchive(Archive& archive,
               std::vector<std::shared_ptr<const FileTreeEntry>> const& entries);

protected:
  using IFileTree::IFileTree;
};

#endif
