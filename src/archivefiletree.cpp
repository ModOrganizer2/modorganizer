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
class ArchiveFileEntry : public virtual FileTreeEntry {
public:

  /**
   * @brief Create a new entry corresponding to a file.
   *
   * @param parent The tree containing this file.
   * @param name The name of this file.
   * @param index The index of the file in the archive.
   * @param time The modification time of this file.
   */
  ArchiveFileEntry(std::shared_ptr<IFileTree> parent, QString name, int index, QDateTime time) :
    FileTreeEntry(parent, name, time), m_Index(index) {
  }

  /**
   * @brief Create a new entry corresponding to a directory.
   *
   * @param parent The tree containing this directory.
   * @param name The name of this directory.
   * @param index The index of the directory in the archive, or -1.
   */
  ArchiveFileEntry(std::shared_ptr<IFileTree> parent, QString name, int index) :
    FileTreeEntry(parent, name), m_Index(index) {
  }

  // No private since we are in an implementation file:
  const int m_Index;
};


/**
 *
 */
class ArchiveFileTreeImpl: public virtual ArchiveFileTree, public virtual ArchiveFileEntry {
public:

  using File = std::tuple<QStringList, bool, int>;

public: // Public for make_shared (but not accessible by other since not exposed in .h):

  ArchiveFileTreeImpl(std::shared_ptr<IFileTree> parent, QString name, int index, std::vector<File>&& files)
    : FileTreeEntry(parent, name), ArchiveFileEntry(parent, name, index), IFileTree(), m_Files(std::move(files)) { }

public: // Override to avoid VS warnings:

  virtual std::shared_ptr<IFileTree> astree() override {
    return IFileTree::astree();
  }

  virtual std::shared_ptr<const IFileTree> astree() const override {
    return IFileTree::astree();
  }

public: // Overrides:

  /**
   * @override
   */
  std::shared_ptr<FileTreeEntry> addFile(QString path, QDateTime time = QDateTime()) override {
    // Cannot add file to an archive.
    throw UnsupportedOperationException(QObject::tr("Cannot create file within an archive."));
  }

  /**
   *
   */
  static void mapToArchive(IFileTree const& tree, QString path, FileData* const* data)
  {
    if (path.length() > 0) {
      // when using a long windows path (starting with \\?\) we apparently can have redundant
      // . components in the path. This wasn't a problem with "regular" path names.
      if (path == ".") {
        path.clear();
      }
      else {
        path.append("\\");
      }
    }

    for (auto const& entry : tree) {
      if (entry->isDir()) {
        const ArchiveFileTreeImpl& archiveEntry = dynamic_cast<const ArchiveFileTreeImpl&>(*entry);
        QString tmp = path + archiveEntry.name();
        if (archiveEntry.m_Index != -1) {
          data[archiveEntry.m_Index]->addOutputFileName(tmp);
        }
        mapToArchive(*archiveEntry.astree(), tmp, data);
      }
      else {
        const ArchiveFileEntry& archiveFileEntry = dynamic_cast<const ArchiveFileEntry&>(*entry);
        data[archiveFileEntry.m_Index]->addOutputFileName(path + archiveFileEntry.name());
      }
    }
  }

  /**
   *
   */
  void mapToArchive(Archive* archive) const override {
    FileData* const* data;
    size_t size;
    archive->getFileList(data, size);
    mapToArchive(*this, "", data);
  }

protected:

  /*
   * Overriding this to create custom FileTreeEntry with index set to -1. No need to
   * override makeFile() since we addFile is overriden. Note that this will not be
   * used to create existing tree since we do this manually in doPopulate.
   *
   * @override
   */
  virtual std::shared_ptr<IFileTree> makeDirectory(
    std::shared_ptr<IFileTree> parent, QString name) const override {
    return std::make_shared<ArchiveFileTreeImpl>(parent, name, -1, std::vector<File>{});
  }

  virtual void doPopulate(std::shared_ptr<IFileTree> parent, std::vector<std::shared_ptr<FileTreeEntry>>& entries) const override {

    // Sort by name:
    std::sort(std::begin(m_Files), std::end(m_Files),
      [](const auto& a, const auto& b) {
        return std::get<0>(a)[0].compare(std::get<0>(b)[0], Qt::CaseInsensitive) < 0; });

    // We know that the files are sorted:
    QString currentName = "";
    int currentIndex = -1;
    std::vector<File> currentFiles;
    for (auto& p : m_Files) {

      // At the start or if we have reset, just retrieve the current name and index - The
      // index might not be valid in this case (e.g., if the path is a/b, the index is the 
      // one for a/b while we would want the one for a, but we correct that later):
      if (currentName == "") {
        currentName = std::get<0>(p)[0];
      }

      // If the name is different, we need to create a directory from what we have 
      // accumulated:
      if (currentName != std::get<0>(p)[0]) {
        // No index here since this is not an empty tree:
        entries.push_back(std::make_shared<ArchiveFileTreeImpl>(parent, currentName, currentIndex, std::move(currentFiles)));
        currentFiles.clear(); // Back to a valid state.

        // Retrieve the next index:
        currentIndex = -1;
      }

      // We can always override the current name:
      currentName = std::get<0>(p)[0];

      // If the current path contains only one components:
      if (std::get<0>(p).size() == 1) {
        // If it is not a directory, then it is a file in directly under this tree:
        if (!std::get<1>(p)) {
          entries.push_back(
            std::make_shared<ArchiveFileEntry>(parent, currentName, std::get<2>(p), QDateTime()));
          currentName = "";
        }
        else {
          // Otherwize, it is the actual "file" corresponding to the directory, so we can retrieve
          // the index here:
          currentIndex = std::get<2>(p);
        }
      }
      else {
        currentFiles.push_back({
          QStringList(std::get<0>(p).begin() + 1, std::get<0>(p).end()), std::get<1>(p), std::get<2>(p)
        });
      }
    }

    if (currentName != "") {
      entries.push_back(std::make_shared<ArchiveFileTreeImpl>(parent, currentName, currentIndex, std::move(currentFiles)));
    }
  }

private:

  mutable std::vector<File> m_Files;
};

std::shared_ptr<ArchiveFileTree> ArchiveFileTree::makeTree(Archive* archive) {

  FileData* const* data;
  size_t size;
  archive->getFileList(data, size);

  std::vector<ArchiveFileTreeImpl::File> files;
  files.reserve(size);

  for (size_t i = 0; i < size; ++i) {
    files.push_back(std::make_tuple(data[i]->getFileName().replace("\\", "/").split("/", Qt::SkipEmptyParts), data[i]->isDirectory(), (int) i));
  }

  auto tree = std::make_shared<ArchiveFileTreeImpl>(nullptr, "", -1, std::move(files));
  return tree;
}

/**
 * @brief Recursive function for the ArchiveFileTree::mapToArchive method. Need a template
 * here because iterators from a vector of entries are not exactly the same as the iterators
 * returned by a IFileTree.
 *
 */
template <class It>
void mapToArchive(FileData* const* data, It begin, It end) {
  for (auto it = begin; it != end; ++it) {
    auto entry = *it;
    auto* aentry = dynamic_cast<const ArchiveFileEntry*>(entry.get());

    if (aentry->m_Index != -1) {
      data[aentry->m_Index]->addOutputFileName(aentry->path());
    }

    if (entry->isDir()) {
      auto tree = entry->astree();
      mapToArchive(data, tree->begin(), tree->end());
    }
  }
}

void ArchiveFileTree::mapToArchive(Archive* archive, std::vector<std::shared_ptr<const FileTreeEntry>> const& entries) {
  FileData* const* data;
  size_t size;
  archive->getFileList(data, size);

  ::mapToArchive(data, entries.cbegin(), entries.cend());
}
