#ifndef MODINFODIALOGFILETREE_H
#define MODINFODIALOGFILETREE_H

#include "modinfodialogtab.h"

class FileTreeTab : public ModInfoDialogTab
{
public:
  FileTreeTab(
    OrganizerCore& oc, PluginContainer& plugin,
    QWidget* parent, Ui::ModInfoDialog* ui);

  void clear() override;
  void update() override;

private:
  struct Actions
  {
    QAction *newFolder = nullptr;
    QAction *open = nullptr;
    QAction *preview = nullptr;
    QAction *rename = nullptr;
    QAction *del = nullptr;
    QAction *hide = nullptr;
    QAction *unhide = nullptr;
  };

  QFileSystemModel* m_fs;
  Actions m_actions;

  QModelIndex singleSelection() const;
  void onCreateDirectory();
  void onOpen();
  void onPreview();
  void onRename();
  void onDelete();
};

#endif // MODINFODIALOGFILETREE_H
