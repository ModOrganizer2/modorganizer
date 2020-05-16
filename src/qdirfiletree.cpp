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
QDirFileTree::QDirFileTree(std::shared_ptr<const IFileTree> parent, QDir directory) : FileTreeEntry(parent, directory.dirName()), IFileTree(), qDir(directory) {
  qDir.setFilter(qDir.filter() | QDir::NoDotAndDotDot);
}

/**
 * No mutable operations allowed.
 */
bool QDirFileTree::beforeReplace(IFileTree const* dstTree, FileTreeEntry const* destination, FileTreeEntry const* source) { return false; }
bool QDirFileTree::beforeInsert(IFileTree const* entry, FileTreeEntry const* name) { return false; }
bool QDirFileTree::beforeRemove(IFileTree const* entry, FileTreeEntry const* name) { return false; }
std::shared_ptr<FileTreeEntry> QDirFileTree::makeFile(std::shared_ptr<const IFileTree> parent, QString name, QDateTime time) const { return nullptr; }
std::shared_ptr<IFileTree> QDirFileTree::makeDirectory(std::shared_ptr<const IFileTree> parent, QString name) const { return nullptr; }

void QDirFileTree::doPopulate(std::shared_ptr<const IFileTree> parent, std::vector<std::shared_ptr<FileTreeEntry>>& entries) const {
  QDirIterator iter(qDir);
  while (iter.hasNext()) {
    QString name = iter.next();
    QFileInfo info = iter.fileInfo();
    if (info.isDir()) {
      entries.push_back(std::shared_ptr<QDirFileTree>(new QDirFileTree(parent, QDir(info.absoluteFilePath()))));
    }
    else {
      entries.push_back(createFileEntry(parent, info.fileName(), info.fileTime(QFileDevice::FileModificationTime)));
    }
  }
}

std::shared_ptr<IFileTree> QDirFileTree::doClone() const {
  return std::shared_ptr<QDirFileTree>(new QDirFileTree(nullptr, qDir));
}
