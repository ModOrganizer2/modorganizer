#include "qdirfiletree.h"

#include <QDirIterator>

using namespace MOBase;

/**
 *
 */
std::shared_ptr<const QDirFileTree> QDirFileTree::makeTree(QDir directory) {
  return std::shared_ptr<const QDirFileTree>(new QDirFileTree(nullptr, directory));
}

/**
 *
 */
QDirFileTree::QDirFileTree(std::shared_ptr<const IFileTree> parent, QDir directory) : FileTreeEntry(parent, directory.dirName()), IFileTree(), qDir(directory) { }

/**
 * No mutable operations allowed.
 */
bool QDirFileTree::beforeReplace(IFileTree const* dstTree, FileTreeEntry const* destination, FileTreeEntry const* source) { return false; }
bool QDirFileTree::beforeInsert(IFileTree const* entry, FileTreeEntry const* name) { return false; }
bool QDirFileTree::beforeRemove(IFileTree const* entry, FileTreeEntry const* name) { return false; }
std::shared_ptr<FileTreeEntry> QDirFileTree::makeFile(std::shared_ptr<const IFileTree> parent, QString name) const { return nullptr; }
std::shared_ptr<IFileTree> QDirFileTree::makeDirectory(std::shared_ptr<const IFileTree> parent, QString name) const { return nullptr; }

bool QDirFileTree::doPopulate(std::shared_ptr<const IFileTree> parent, std::vector<std::shared_ptr<FileTreeEntry>>& entries) const {
  auto infoList = qDir.entryInfoList(qDir.filter() | QDir::NoDotAndDotDot, QDir::Name | QDir::DirsFirst | QDir::IgnoreCase);
  for (auto& info : infoList) {
    if (info.isDir()) {
      entries.push_back(std::shared_ptr<QDirFileTree>(new QDirFileTree(parent, QDir(info.absoluteFilePath()))));
    }
    else {
      entries.push_back(createFileEntry(parent, info.fileName()));
    }
  }

  // Vector is already sorted:
  return true;
}

std::shared_ptr<IFileTree> QDirFileTree::doClone() const {
  return std::shared_ptr<QDirFileTree>(new QDirFileTree(nullptr, qDir));
}
