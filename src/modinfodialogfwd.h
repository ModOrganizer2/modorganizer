#ifndef MODINFODIALOGFWD_H
#define MODINFODIALOGFWD_H

#include "filerenamer.h"
#include <QStyledItemDelegate>

class ModInfo;
using ModInfoPtr = QSharedPointer<ModInfo>;

enum class ModInfoTabIDs
{
  None      = -1,
  TextFiles = 0,
  IniFiles,
  Images,
  Esps,
  Conflicts,
  Categories,
  Nexus,
  Notes,
  Filetree
};

class PluginManager;

bool canPreviewFile(const PluginManager& pluginManager, bool isArchive,
                    const QString& filename);
bool canRunFile(bool isArchive, const QString& filename);
bool canOpenFile(bool isArchive, const QString& filename);
bool canExploreFile(bool isArchive, const QString& filename);
bool canHideFile(bool isArchive, const QString& filename);
bool canUnhideFile(bool isArchive, const QString& filename);

FileRenamer::RenameResults hideFile(FileRenamer& renamer, const QString& oldName);
FileRenamer::RenameResults unhideFile(FileRenamer& renamer, const QString& oldName);
FileRenamer::RenameResults restoreHiddenFilesRecursive(FileRenamer& renamer,
                                                       const QString& targetDir);

class ElideLeftDelegate : public QStyledItemDelegate
{
public:
  using QStyledItemDelegate::QStyledItemDelegate;

protected:
  void initStyleOption(QStyleOptionViewItem* o, const QModelIndex& i) const
  {
    QStyledItemDelegate::initStyleOption(o, i);
    o->textElideMode = Qt::ElideLeft;
  }
};

#endif  // MODINFODIALOGFWD_H
