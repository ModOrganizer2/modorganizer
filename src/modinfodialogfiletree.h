#ifndef MODINFODIALOGFILETREE_H
#define MODINFODIALOGFILETREE_H

#include "modinfodialogtab.h"
#include <QFileSystemModel>

class FileTreeTab : public ModInfoDialogTab
{
  Q_OBJECT;

public:
  FileTreeTab(ModInfoDialogTabContext cx);

  void clear() override;
  void update() override;
  bool deleteRequested() override;

private:
  struct Actions
  {
    QAction *newFolder = nullptr;
    QAction *open = nullptr;
    QAction *preview = nullptr;
    QAction *explore = nullptr;
    QAction *rename = nullptr;
    QAction *del = nullptr;
    QAction *hide = nullptr;
    QAction *unhide = nullptr;
  };

  QFileSystemModel* m_fs;
  Actions m_actions;

  void onCreateDirectory();
  void onOpen();
  void onPreview();
  void onExplore();
  void onRename();
  void onDelete();
  void onHide();
  void onUnhide();
  void onOpenInExplorer();
  void onContextMenu(const QPoint &pos);

  QModelIndex singleSelection() const;
  bool deleteFile(const QModelIndex& index);
  bool deleteFileRecursive(const QModelIndex& index);
  void changeVisibility(bool visible);
};

#endif // MODINFODIALOGFILETREE_H
