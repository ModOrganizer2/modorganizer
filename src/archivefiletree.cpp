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

// For QObject::tr:
#include <QObject>

#include "archivefiletree.h"

#include "log.h"

using namespace MOBase;

/**
 * We use custom file entries to store the index.
 */
class ArchiveFileEntry : public virtual FileTreeEntry
{
public:
  /**
   * @brief Create a new entry.
   *
   * @param parent The tree containing this entry.
   * @param name The name of this entry.
   * @param index The index of the entry in the archive.
   */
  ArchiveFileEntry(std::shared_ptr<const IFileTree> parent, QString name, int index)
      : FileTreeEntry(parent, name), m_Index(index)
  {}

  virtual std::shared_ptr<FileTreeEntry> clone() const override
  {
    return std::make_shared<ArchiveFileEntry>(nullptr, name(), m_Index);
  }

  // No private since we are in an implementation file:
  const int m_Index;
};

/**
 *
 */
class ArchiveFileTreeImpl : public virtual ArchiveFileTree,
                            public virtual ArchiveFileEntry
{
public:
  using File = std::tuple<QStringList, bool, int>;

public
    :  // Public for make_shared (but not accessible by other since not exposed in .h):
  ArchiveFileTreeImpl(std::shared_ptr<const IFileTree> parent, QString name, int index,
                      std::vector<File> files)
      : FileTreeEntry(parent, name), ArchiveFileEntry(parent, name, index), IFileTree(),
        m_Files(std::move(files))
  {}

public:  // Override to avoid VS warnings:
  virtual std::shared_ptr<IFileTree> astree() override { return IFileTree::astree(); }

  virtual std::shared_ptr<const IFileTree> astree() const override
  {
    return IFileTree::astree();
  }

protected:
  virtual std::shared_ptr<FileTreeEntry> clone() const override
  {
    return IFileTree::clone();
  }

public:  // Overrides:
  /**
   *
   */
  static void mapToArchive(IFileTree const& tree, QString path,
                           std::vector<FileData*> const& data)
  {
    if (path.length() > 0) {
      // when using a long windows path (starting with \\?\) we apparently can have
      // redundant . components in the path. This wasn't a problem with "regular" path
      // names.
      if (path == ".") {
        path.clear();
      } else {
        path.append("\\");
      }
    }

    for (auto const& entry : tree) {
      if (entry->isDir()) {
        const ArchiveFileTreeImpl& archiveEntry =
            dynamic_cast<const ArchiveFileTreeImpl&>(*entry);
        QString tmp = path + archiveEntry.name();
        if (archiveEntry.m_Index != -1) {
          data[archiveEntry.m_Index]->addOutputFilePath(tmp.toStdWString());
        }
        mapToArchive(*archiveEntry.astree(), tmp, data);
      } else {
        const ArchiveFileEntry& archiveFileEntry =
            dynamic_cast<const ArchiveFileEntry&>(*entry);
        if (archiveFileEntry.m_Index != -1) {
          data[archiveFileEntry.m_Index]->addOutputFilePath(
              (path + archiveFileEntry.name()).toStdWString());
        }
      }
    }
  }

  /**
   *
   */
  void mapToArchive(Archive& archive) const override
  {
    mapToArchive(*this, "", archive.getFileList());
  }

protected:
  /**
   * Overriding makeDirectory and makeFile to create file tree or file entry with index
   * -1.
   *
   */
  virtual std::shared_ptr<IFileTree>
  makeDirectory(std::shared_ptr<const IFileTree> parent, QString name) const override
  {
    return std::make_shared<ArchiveFileTreeImpl>(parent, name, -1, std::vector<File>{});
  }

  virtual std::shared_ptr<FileTreeEntry>
  makeFile(std::shared_ptr<const IFileTree> parent, QString name) const override
  {
    return std::make_shared<ArchiveFileEntry>(parent, name, -1);
  }

  virtual bool
  doPopulate(std::shared_ptr<const IFileTree> parent,
             std::vector<std::shared_ptr<FileTreeEntry>>& entries) const override
  {

    // Sort by name:
    std::sort(std::begin(m_Files), std::end(m_Files), [](const auto& a, const auto& b) {
      return std::get<0>(a)[0].compare(std::get<0>(b)[0], Qt::CaseInsensitive) < 0;
    });

    // We know that the files are sorted:
    QString currentName = "";
    int currentIndex    = -1;
    std::vector<File> currentFiles;
    for (auto& p : m_Files) {

      // At the start or if we have reset, just retrieve the current name:
      if (currentName == "") {
        currentName = std::get<0>(p)[0];
      }

      // If the name is different, we need to create a directory from what we have
      // accumulated:
      if (currentName != std::get<0>(p)[0]) {

        // We may or may not have an index here, it depends on the type of archive (some
        // archives list intermediate non-empty folders, some don't):
        entries.push_back(std::make_shared<ArchiveFileTreeImpl>(
            parent, currentName, currentIndex, std::move(currentFiles)));

        currentFiles.clear();  // Back to a valid state.

        // Reset the index:
        currentIndex = -1;
      }

      // We can always override the current name:
      currentName = std::get<0>(p)[0];

      // If the current path contains only one components:
      if (std::get<0>(p).size() == 1) {

        // If it is not a directory, then it is a file in directly under this tree:
        if (!std::get<1>(p)) {
          entries.push_back(
              std::make_shared<ArchiveFileEntry>(parent, currentName, std::get<2>(p)));
          currentName = "";
        } else {
          // Otherwize, it is the actual "file" corresponding to the directory we are
          // listing, so we can retrieve the index here:
          currentIndex = std::get<2>(p);
        }
      } else {
        currentFiles.push_back(
            {QStringList(std::get<0>(p).begin() + 1, std::get<0>(p).end()),
             std::get<1>(p), std::get<2>(p)});
      }
    }

    if (currentName != "") {
      entries.push_back(std::make_shared<ArchiveFileTreeImpl>(
          parent, currentName, currentIndex, std::move(currentFiles)));
    }

    // Let the parent class sort the entries:
    return false;
  }

  virtual std::shared_ptr<IFileTree> doClone() const override
  {
    return std::make_shared<ArchiveFileTreeImpl>(nullptr, name(), m_Index, m_Files);
  }

private:
  mutable std::vector<File> m_Files;
};

std::shared_ptr<ArchiveFileTree> ArchiveFileTree::makeTree(Archive const& archive)
{
  auto const& data = archive.getFileList();

  std::vector<ArchiveFileTreeImpl::File> files;
  files.reserve(data.size());

  for (size_t i = 0; i < data.size(); ++i) {
    // Ignore "." and ".." as they're useless and muck things up
    if (data[i]->getArchiveFilePath().compare(L".") == 0 ||
        data[i]->getArchiveFilePath().compare(L"..") == 0) {
      continue;
    }

    files.push_back(
        std::make_tuple(QString::fromStdWString(data[i]->getArchiveFilePath())
                            .replace("\\", "/")
                            .split("/", Qt::SkipEmptyParts),
                        data[i]->isDirectory(), (int)i));
  }

  auto tree = std::make_shared<ArchiveFileTreeImpl>(nullptr, "", -1, std::move(files));
  return tree;
}

/**
 * @brief Recursive function for the ArchiveFileTree::mapToArchive method. Need a
 * template here because iterators from a vector of entries are not exactly the same as
 * the iterators returned by a IFileTree.
 *
 */
template <class It>
void mapToArchive(std::vector<FileData*> const& data, It begin, It end)
{
  for (auto it = begin; it != end; ++it) {
    auto entry   = *it;
    auto* aentry = dynamic_cast<const ArchiveFileEntry*>(entry.get());

    if (aentry->m_Index != -1) {
      data[aentry->m_Index]->addOutputFilePath(aentry->path().toStdWString());
    }

    if (entry->isDir()) {
      auto tree = entry->astree();
      mapToArchive(data, tree->begin(), tree->end());
    }
  }
}

void ArchiveFileTree::mapToArchive(
    Archive& archive, std::vector<std::shared_ptr<const FileTreeEntry>> const& entries)
{
  ::mapToArchive(archive.getFileList(), entries.cbegin(), entries.cend());
}
