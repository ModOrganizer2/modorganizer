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

namespace MOShared
{
class DirectoryEntry;
}

/**
 * @brief Class that expose the VFS main directory structure as a `MOBase::IFileTree`.
 *
 * The tree is lazily populated: each subtree is only populated when needed,
 * as specified by IFileTree.
 *
 * This class does not expose mutable operations, so any mutable operations will
 * fail.
 */
class VirtualFileTree : public MOBase::IFileTree
{
public:
  /**
   * @brief Create a new file tree representing the given VFS directory.
   *
   * @param root Root directory.
   *
   * @return a file tree representing the VFS directory.
   */
  static std::shared_ptr<const VirtualFileTree>
  makeTree(const MOShared::DirectoryEntry* root);

protected:
  using IFileTree::IFileTree;

  virtual bool
  doPopulate(std::shared_ptr<const IFileTree> parent,
             std::vector<std::shared_ptr<FileTreeEntry>>& entries) const = 0;
};

#endif
