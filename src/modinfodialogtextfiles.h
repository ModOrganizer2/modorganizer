#ifndef MODINFODIALOGTEXTFILES_H
#define MODINFODIALOGTEXTFILES_H

#include "filterwidget.h"
#include "modinfodialogtab.h"
#include <QListView>
#include <QSplitter>

using namespace MOBase;

class FileListItem;
class FileListModel;
class TextEditor;

class GenericFilesTab : public ModInfoDialogTab
{
  Q_OBJECT;

public:
  void clear() override;
  bool canClose() override;
  bool feedFile(const QString& rootPath, const QString& fullPath) override;
  void update() override;
  void saveState(Settings& s) override;
  void restoreState(const Settings& s) override;

protected:
  QListView* m_list;
  TextEditor* m_editor;
  QSplitter* m_splitter;
  FileListModel* m_model;
  FilterWidget m_filter;

  GenericFilesTab(ModInfoDialogTabContext cx, QListView* list, QSplitter* splitter,
                  TextEditor* editor, QLineEdit* filter);

  virtual bool wantsFile(const QString& rootPath, const QString& fullPath) const = 0;

private:
  void onSelection(const QModelIndex& current, const QModelIndex& previous);
  void select(const QModelIndex& index);
};

class TextFilesTab : public GenericFilesTab
{
public:
  TextFilesTab(ModInfoDialogTabContext cx);

protected:
  bool wantsFile(const QString& rootPath, const QString& fullPath) const override;
};

class IniFilesTab : public GenericFilesTab
{
public:
  IniFilesTab(ModInfoDialogTabContext cx);

protected:
  bool wantsFile(const QString& rootPath, const QString& fullPath) const override;
};

#endif  // MODINFODIALOGTEXTFILES_H
