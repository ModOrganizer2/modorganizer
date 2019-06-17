#ifndef MODINFODIALOGTEXTFILES_H
#define MODINFODIALOGTEXTFILES_H

#include "modinfodialog.h"
#include <QSplitter>
#include <QListWidget>

class FileListItem;
class TextEditor;

class GenericFilesTab : public ModInfoDialogTab
{
public:
  GenericFilesTab(QListWidget* list, QSplitter* splitter, TextEditor* editor);

  void clear() override;
  bool canClose() override;
  bool feedFile(const QString& rootPath, const QString& fullPath) override;

protected:
  QListWidget* m_list;
  TextEditor* m_editor;

  virtual bool wantsFile(const QString& rootPath, const QString& fullPath) const = 0;

private:
  void onSelection(QListWidgetItem* current, QListWidgetItem* previous);
  void select(FileListItem* item);
};


class TextFilesTab : public GenericFilesTab
{
public:
  TextFilesTab(Ui::ModInfoDialog* ui);

protected:
  bool wantsFile(const QString& rootPath, const QString& fullPath) const override;
};


class IniFilesTab : public GenericFilesTab
{
public:
  IniFilesTab(Ui::ModInfoDialog* ui);

protected:
  bool wantsFile(const QString& rootPath, const QString& fullPath) const override;
};

#endif // MODINFODIALOGTEXTFILES_H
