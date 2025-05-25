#ifndef MO_TEXTEDITOR_H
#define MO_TEXTEDITOR_H

#include <QPlainTextEdit>

class TextEditor;

class TextEditorToolbar : public QFrame
{
  Q_OBJECT;

public:
  TextEditorToolbar(TextEditor& editor);

private:
  TextEditor& m_editor;
  QAction* m_save;
  QAction* m_wordWrap;
  QAction* m_explore;
  QLineEdit* m_path;

  void onTextModified(bool b);
  void onWordWrap(bool b);
  void onLoaded(const QString& s);
};

// mostly from https://doc.qt.io/qt-5/qtwidgets-widgets-codeeditor-example.html
//
class TextEditorLineNumbers : public QFrame
{
  Q_OBJECT;
  Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor);
  Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor);

public:
  TextEditorLineNumbers(TextEditor& editor);

  QSize sizeHint() const override;
  int areaWidth() const;

  QColor textColor() const;
  void setTextColor(const QColor& c);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor& c);

protected:
  void paintEvent(QPaintEvent* event) override;

private:
  TextEditor& m_editor;
  QColor m_background, m_text;

  void updateAreaWidth();
  void updateArea(const QRect& rect, int dy);
};

class TextEditorHighlighter : public QSyntaxHighlighter
{
  Q_OBJECT;

public:
  TextEditorHighlighter(QTextDocument* doc);

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor& c);

  QColor textColor() const;
  void setTextColor(const QColor& c);

protected:
  void highlightBlock(const QString& text) override;

private:
  QColor m_background, m_text;

  void changed();
};

class TextEditor : public QPlainTextEdit
{
  Q_OBJECT;
  Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor);
  Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor);
  Q_PROPERTY(QColor highlightBackgroundColor READ highlightBackgroundColor WRITE
                 setHighlightBackgroundColor);

  friend class TextEditorLineNumbers;

public:
  TextEditor(QWidget* parent = nullptr);

  void setupToolbar();

  void clear();
  bool load(const QString& filename);
  bool save();

  const QString& filename() const;

  void wordWrap(bool b);
  void toggleWordWrap();
  bool wordWrap() const;

  bool dirty() const;

  QColor backgroundColor() const;
  void setBackgroundColor(const QColor& c);

  QColor textColor() const;
  void setTextColor(const QColor& c);

  QColor highlightBackgroundColor() const;
  void setHighlightBackgroundColor(const QColor& c);

  void explore();

signals:
  void loaded(QString filename);
  void modified(bool b);
  void wordWrapChanged(bool b);

protected:
  void resizeEvent(QResizeEvent* e) override;

private:
  TextEditorToolbar* m_toolbar;
  TextEditorLineNumbers* m_lineNumbers;
  TextEditorHighlighter* m_highlighter;
  QColor m_highlightBackground;
  QString m_filename;
  QString m_encoding;
  bool m_needsBOM;
  bool m_dirty;
  bool m_loading;

  void setDefaultStyle();
  void onModified(bool b);
  void dirty(bool b);

  QWidget* wrapEditWidget();

  void highlightCurrentLine();
  void paintLineNumbers(QPaintEvent* e, const QColor& textColor);
};

class HTMLEditor : public QTextEdit
{
  Q_OBJECT;

public:
  using QTextEdit::QTextEdit;

signals:
  void editingFinished();

protected:
  void focusOutEvent(QFocusEvent* e);

private:
};

#endif  // MO_TEXTEDITOR_H
