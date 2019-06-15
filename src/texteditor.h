#ifndef MO_TEXTEDITOR_H
#define MO_TEXTEDITOR_H

#include <QPlainTextEdit>

class TextEditor;

class TextEditorToolbar
{
public:
  TextEditorToolbar(TextEditor& editor);

  QWidget* widget();

private:
  TextEditor& m_editor;
  QWidget* m_widget;
  QAction* m_save;
  QAction* m_wordWrap;

  void onTextModified(bool b);
  void onSave();
  void onWordWrap();
};


class TextEditor : public QObject
{
  Q_OBJECT

public:
  TextEditor(QPlainTextEdit* edit);

  bool load(const QString& filename);
  bool save();

  const QString& filename() const;

  void wordWrap(bool b);
  bool wordWrap() const;

  bool dirty() const;

signals:
  void modified(bool b);

private:
  QPlainTextEdit* m_edit;
  TextEditorToolbar m_toolbar;
  QString m_filename;
  QString m_encoding;
  bool m_dirty;

  void onModified(bool b);
  void dirty(bool b);

  void setupToolbar();
  QWidget* wrapEditWidget();
};

#endif // MO_TEXTEDITOR_H
