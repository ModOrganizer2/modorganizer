#include "qdirfiletree.h"

#include <QDirIterator>

using namespace MOBase;

class QDirFileTreeImpl : public QDirFileTree {
public:


  /**
   *
   */
  QDirFileTreeImpl(std::shared_ptr<const IFileTree> parent, QDir dir) :
    FileTreeEntry(parent, dir.dirName()), QDirFileTree(), qDir(dir) { }

protected:

  /**
   * No mutable operations allowed.
   */
  bool beforeReplace(IFileTree const* dstTree, FileTreeEntry const* destination, FileTreeEntry const* source) override { return false; }
  bool beforeInsert(IFileTree const* entry, FileTreeEntry const* name) override { return false; }
  bool beforeRemove(IFileTree const* entry, FileTreeEntry const* name) override { return false; }
  std::shared_ptr<FileTreeEntry> makeFile(std::shared_ptr<const IFileTree> parent, QString name) const override { return nullptr; }
  std::shared_ptr<IFileTree> makeDirectory(std::shared_ptr<const IFileTree> parent, QString name) const override { return nullptr; }

  bool doPopulate(std::shared_ptr<const IFileTree> parent, std::vector<std::shared_ptr<FileTreeEntry>>& entries) const override {
    auto infoList = qDir.entryInfoList(qDir.filter() | QDir::NoDotAndDotDot, QDir::Name | QDir::DirsFirst | QDir::IgnoreCase);
    for (auto& info : infoList) {
      if (info.isDir()) {
        entries.push_back(std::make_shared<QDirFileTreeImpl>(parent, QDir(info.absoluteFilePath())));
      }
      else {
        entries.push_back(createFileEntry(parent, info.fileName()));
      }
    }

    // Vector is already sorted:
    return true;
  }

  std::shared_ptr<IFileTree> QDirFileTree::doClone() const {
    return std::make_shared<QDirFileTreeImpl>(nullptr, qDir);
  }

private:
  QDir qDir;

};

/**
 *
 */
std::shared_ptr<const QDirFileTree> QDirFileTree::makeTree(QDir directory) {
  return std::make_shared<QDirFileTreeImpl>(nullptr, directory);
}
