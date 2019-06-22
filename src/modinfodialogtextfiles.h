#ifndef MODINFODIALOGTEXTFILES_H
#define MODINFODIALOGTEXTFILES_H

#include "modinfodialogtab.h"
#include <QSplitter>
#include <QListWidget>

class FileListItem;
class TextEditor;

class GenericFilesTab : public ModInfoDialogTab
{
  Q_OBJECT;

public:
  void clear() override;
  bool canClose() override;
  bool feedFile(const QString& rootPath, const QString& fullPath) override;

protected:
  QWidget* m_parent;
  QListWidget* m_list;
  TextEditor* m_editor;

  GenericFilesTab(
    QWidget* parent,
    QListWidget* list, QSplitter* splitter, TextEditor* editor);

  virtual bool wantsFile(const QString& rootPath, const QString& fullPath) const = 0;

private:
  void onSelection(QListWidgetItem* current, QListWidgetItem* previous);
  void select(FileListItem* item);
};


class TextFilesTab : public GenericFilesTab
{
public:
  TextFilesTab(QWidget* parent, Ui::ModInfoDialog* ui);

protected:
  bool wantsFile(const QString& rootPath, const QString& fullPath) const override;
};


class IniFilesTab : public GenericFilesTab
{
public:
  IniFilesTab(QWidget* parent, Ui::ModInfoDialog* ui);

protected:
  bool wantsFile(const QString& rootPath, const QString& fullPath) const override;
};

#endif // MODINFODIALOGTEXTFILES_H
