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

#include <QDir>

#include "ifiletree.h"

/**
 * @brief Class that expose a directory on the drive, using QDir, as a
 * `MOBase::IFileTree`.
 *
 * The tree is lazily populated: each subtree is only populated (from the disk) when
 * needed, as specified by IFileTree.
 *
 * This class does not expose mutable operations, so any mutable operations will
 * fail.
 */
class QDirFileTree : public MOBase::IFileTree
{
public:
  /**
   * @brief Create a new file tree representing the given directory.
   *
   * @param directory Directory to represent.
   * @param ignoreRootMeta If true, the meta.ini file in the root folder will
   *   be ignored.
   *
   * @return a file tree representing the given directory.
   */
  static std::shared_ptr<const QDirFileTree> makeTree(QDir directory,
                                                      bool ignoreRootMeta = true);

protected:
  using IFileTree::IFileTree;

  virtual bool
  doPopulate(std::shared_ptr<const IFileTree> parent,
             std::vector<std::shared_ptr<FileTreeEntry>>& entries) const = 0;
};

#endif
