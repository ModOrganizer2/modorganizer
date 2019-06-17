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
  void onWordWrap(bool b);
};


// mostly from https://doc.qt.io/qt-5/qtwidgets-widgets-codeeditor-example.html
//
class TextEditorLineNumbers : public QWidget
{
public:
  TextEditorLineNumbers(TextEditor& editor);
  QSize sizeHint() const override;
  int areaWidth() const;

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  TextEditor& m_editor;

  void updateAreaWidth();
  void updateArea(const QRect &rect, int dy);
  void highlightCurrentLine();
};


class TextEditorHighlighter : public QSyntaxHighlighter
{
public:
  TextEditorHighlighter(QTextDocument* doc);

  QColor backgroundColor();
  void setBackgroundColor(const QColor& c);

  QColor textColor();
  void setTextColor(const QColor& c);

protected:
  void highlightBlock(const QString& text) override;

private:
  QColor m_background, m_text;

  void changed();
};


class TextEditor : public QPlainTextEdit
{
  Q_OBJECT
  Q_PROPERTY(QString textColor READ textColor WRITE setTextColor)
  Q_PROPERTY(QString backgroundColor READ backgroundColor WRITE setBackgroundColor)

  friend class TextEditorLineNumbers;

public:
  TextEditor(QWidget* parent=nullptr);

  void setupToolbar();

  bool load(const QString& filename);
  bool save();

  const QString& filename() const;

  void wordWrap(bool b);
  void toggleWordWrap();
  bool wordWrap() const;

  bool dirty() const;

  QString textColor() const;
  void setTextColor(const QString& s);

  QString backgroundColor() const;
  void setBackgroundColor(const QString& s);

signals:
  void modified(bool b);
  void wordWrapChanged(bool b);

protected:
  void resizeEvent(QResizeEvent* e) override;

private:
  TextEditorToolbar m_toolbar;
  TextEditorLineNumbers* m_lineNumbers;
  TextEditorHighlighter* m_highlighter;
  QString m_filename;
  QString m_encoding;
  bool m_dirty;

  void onModified(bool b);
  void dirty(bool b);

  QWidget* wrapEditWidget();

  void paintLineNumbers(QPaintEvent* e);
};

#endif // MO_TEXTEDITOR_H
