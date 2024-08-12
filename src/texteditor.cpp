#include "texteditor.h"
#include "utility.h"
#include <QSplitter>
#include <log.h>

using namespace MOBase;

TextEditor::TextEditor(QWidget* parent)
    : QPlainTextEdit(parent), m_toolbar(nullptr), m_lineNumbers(nullptr),
      m_highlighter(nullptr), m_dirty(false), m_loading(false)
{
  m_toolbar     = new TextEditorToolbar(*this);
  m_lineNumbers = new TextEditorLineNumbers(*this);
  m_highlighter = new TextEditorHighlighter(document());

  setDefaultStyle();
  wordWrap(true);

  emit modified(false);

  connect(document(), &QTextDocument::modificationChanged, [&](bool b) {
    onModified(b);
  });

  connect(this, &QPlainTextEdit::cursorPositionChanged, [&] {
    highlightCurrentLine();
  });
}

void TextEditor::setDefaultStyle()
{
  const auto font = QFontDatabase::systemFont(QFontDatabase::FixedFont);

  setFont(font);
  m_lineNumbers->setFont(font);

  QColor textColor(Qt::black);
  QColor altTextColor(Qt::darkGray);
  QColor backgroundColor(Qt::white);

  {
    auto w = std::make_unique<QWidget>();

    if (auto* s = style()) {
      s->polish(w.get());
    }

    textColor       = w->palette().color(QPalette::WindowText);
    altTextColor    = w->palette().color(QPalette::Disabled, QPalette::WindowText);
    backgroundColor = w->palette().color(QPalette::Window);
  }

  setTextColor(textColor);
  m_lineNumbers->setTextColor(altTextColor);

  setBackgroundColor(backgroundColor);
  m_lineNumbers->setBackgroundColor(backgroundColor);

  setHighlightBackgroundColor(backgroundColor);
}

void TextEditor::clear()
{
  QScopedValueRollback loading(m_loading, true);

  m_filename.clear();
  m_encoding.clear();
  setPlainText("");
  dirty(false);
  document()->setModified(false);

  emit loaded("");
}

bool TextEditor::load(const QString& filename)
{
  clear();

  QScopedValueRollback loading(m_loading, true);

  m_filename = filename;

  const QString s = MOBase::readFileText(filename, &m_encoding);

  setPlainText(s);
  document()->setModified(false);

  if (s.isEmpty()) {
    // the modificationChanged even is not fired by the setModified() call
    // above when the text being set is empty
    onModified(false);
  }

  emit loaded(m_filename);

  return true;
}

bool TextEditor::save()
{
  if (m_filename.isEmpty() || m_encoding.isEmpty()) {
    return false;
  }

  QFile file(m_filename);
  file.open(QIODevice::WriteOnly);
  file.resize(0);

  auto codec = QStringConverter::encodingForName(m_encoding.toLocal8Bit());
  if (!codec.has_value())
    return false;
  QStringEncoder encoder(codec.value());

  QString data = toPlainText().replace("\n", "\r\n");

  file.write(encoder.encode(data));
  document()->setModified(false);

  return true;
}

const QString& TextEditor::filename() const
{
  return m_filename;
}

void TextEditor::wordWrap(bool b)
{
  if (b) {
    setLineWrapMode(QPlainTextEdit::WidgetWidth);
  } else {
    setLineWrapMode(QPlainTextEdit::NoWrap);
  }

  emit wordWrapChanged(b);
}

void TextEditor::toggleWordWrap()
{
  wordWrap(!wordWrap());
}

bool TextEditor::wordWrap() const
{
  return (lineWrapMode() == QPlainTextEdit::WidgetWidth);
}

void TextEditor::dirty(bool b)
{
  m_dirty = b;
}

bool TextEditor::dirty() const
{
  return m_dirty;
}

QColor TextEditor::backgroundColor() const
{
  return m_highlighter->backgroundColor();
}

void TextEditor::setBackgroundColor(const QColor& c)
{
  if (m_highlighter->backgroundColor() == c) {
    return;
  }

  m_highlighter->setBackgroundColor(c);

  setStyleSheet(QString("QPlainTextEdit{ background-color: rgba(%1, %2, %3, %4); }")
                    .arg(c.redF() * 255)
                    .arg(c.greenF() * 255)
                    .arg(c.blueF() * 255)
                    .arg(c.alphaF()));
}

QColor TextEditor::textColor() const
{
  return m_highlighter->textColor();
}

void TextEditor::setTextColor(const QColor& c)
{
  m_highlighter->setTextColor(c);
}

QColor TextEditor::highlightBackgroundColor() const
{
  return m_highlightBackground;
}

void TextEditor::setHighlightBackgroundColor(const QColor& c)
{
  m_highlightBackground = c;
  update();
}

void TextEditor::explore()
{
  if (m_filename.isEmpty()) {
    return;
  }

  shell::Explore(m_filename);
}

void TextEditor::onModified(bool b)
{
  if (m_loading) {
    return;
  }

  dirty(b);
  emit modified(b);
}

void TextEditor::setupToolbar()
{
  auto* widget = wrapEditWidget();
  if (!widget) {
    return;
  }

  auto* layout = new QVBoxLayout(widget);

  // adding toolbar and edit
  layout->addWidget(m_toolbar);
  layout->addWidget(this);

  // make the edit stretch
  layout->setStretch(0, 0);
  layout->setStretch(1, 1);

  // visuals
  layout->setContentsMargins(0, 0, 0, 0);
  widget->show();
}

QWidget* TextEditor::wrapEditWidget()
{
  auto widget = std::make_unique<QWidget>();

  // wrapping the QPlainTextEdit into a new widget so the toolbar can be
  // displayed above it

  if (auto* parentLayout = parentWidget()->layout()) {
    // the edit's parent has a regular layout, replace the edit by the new
    // widget and delete the QLayoutItem that's returned as it's not needed
    delete parentLayout->replaceWidget(this, widget.get());

  } else if (auto* splitter = qobject_cast<QSplitter*>(parentWidget())) {
    // the edit's parent is a QSplitter, which doesn't have a layout; replace
    // the edit by using its index in the splitter
    auto index = splitter->indexOf(this);

    if (index == -1) {
      log::error("TextEditor: cannot wrap edit widget to display a toolbar, "
                 "parent is a splitter, but widget isn't in it");

      return nullptr;
    }

    splitter->replaceWidget(index, widget.get());

  } else {
    // unknown parent
    log::error("TextEditor: cannot wrap edit widget to display a toolbar, "
               "no parent or parent has no layout");

    return nullptr;
  }

  return widget.release();
}

void TextEditor::resizeEvent(QResizeEvent* e)
{
  QPlainTextEdit::resizeEvent(e);

  QRect cr = contentsRect();
  m_lineNumbers->setGeometry(
      QRect(cr.left(), cr.top(), m_lineNumbers->areaWidth(), cr.height()));
}

void TextEditor::paintLineNumbers(QPaintEvent* e, const QColor& textColor)
{
  QStyleOption opt;
  opt.initFrom(m_lineNumbers);

  QPainter painter(m_lineNumbers);

  QTextBlock block = firstVisibleBlock();
  int blockNumber  = block.blockNumber();
  int top    = (int)blockBoundingGeometry(block).translated(contentOffset()).top();
  int bottom = top + (int)blockBoundingRect(block).height();

  while (block.isValid() && top <= e->rect().bottom()) {
    if (block.isVisible() && bottom >= e->rect().top()) {
      QString number = QString::number(blockNumber + 1);
      painter.setPen(textColor);

      painter.drawText(0, top, m_lineNumbers->width() - 3, fontMetrics().height(),
                       Qt::AlignRight, number);
    }

    block  = block.next();
    top    = bottom;
    bottom = top + (int)blockBoundingRect(block).height();
    ++blockNumber;
  }
}

void TextEditor::highlightCurrentLine()
{
  QList<QTextEdit::ExtraSelection> extraSelections;

  if (!isReadOnly()) {
    QTextEdit::ExtraSelection selection;

    QColor lineColor = QColor(Qt::yellow).lighter(160);

    selection.format.setBackground(m_highlightBackground);
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor();
    selection.cursor.clearSelection();
    extraSelections.append(selection);
  }

  setExtraSelections(extraSelections);
}

TextEditorHighlighter::TextEditorHighlighter(QTextDocument* doc)
    : QSyntaxHighlighter(doc), m_background(QColor("transparent")),
      m_text(QColor("black"))
{}

QColor TextEditorHighlighter::backgroundColor() const
{
  return m_background;
}

void TextEditorHighlighter::setBackgroundColor(const QColor& c)
{
  m_background = c;
  changed();
}

QColor TextEditorHighlighter::textColor() const
{
  return m_text;
}

void TextEditorHighlighter::setTextColor(const QColor& c)
{
  m_text = c;
  changed();
}

void TextEditorHighlighter::highlightBlock(const QString& s)
{
  QTextCharFormat f;
  f.setBackground(m_background);
  f.setForeground(m_text);

  setFormat(0, s.size(), f);
}

void TextEditorHighlighter::changed()
{
  rehighlight();
}

TextEditorLineNumbers::TextEditorLineNumbers(TextEditor& editor)
    : QFrame(&editor), m_editor(editor)
{
  setFont(editor.font());

  connect(&m_editor, &QPlainTextEdit::blockCountChanged, [&] {
    updateAreaWidth();
  });
  connect(&m_editor, &QPlainTextEdit::updateRequest, [&](auto&& rect, int dy) {
    updateArea(rect, dy);
  });

  updateAreaWidth();
}

QSize TextEditorLineNumbers::sizeHint() const
{
  return QSize(areaWidth(), 0);
}

int TextEditorLineNumbers::areaWidth() const
{
  int digits = 1;
  int max    = std::max(1, m_editor.blockCount());

  while (max >= 10) {
    max /= 10;
    ++digits;
  }

  digits = std::max(3, digits);

  int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits + 3;

  return space;
}

QColor TextEditorLineNumbers::textColor() const
{
  return m_text;
}

void TextEditorLineNumbers::setTextColor(const QColor& c)
{
  m_text = c;
  m_editor.update();
}

QColor TextEditorLineNumbers::backgroundColor() const
{
  return m_background;
}

void TextEditorLineNumbers::setBackgroundColor(const QColor& c)
{
  m_background = c;
  m_editor.update();
}

void TextEditorLineNumbers::paintEvent(QPaintEvent* e)
{
  QPainter painter(this);
  painter.fillRect(e->rect(), m_background);

  QFrame::paintEvent(e);
  m_editor.paintLineNumbers(e, m_text);
}

void TextEditorLineNumbers::updateAreaWidth()
{
  m_editor.setViewportMargins(areaWidth(), 0, 0, 0);
}

void TextEditorLineNumbers::updateArea(const QRect& rect, int dy)
{
  if (dy) {
    scroll(0, dy);
  } else {
    update(0, rect.y(), width(), rect.height());
  }

  if (rect.contains(m_editor.viewport()->rect())) {
    updateAreaWidth();
  }
}

TextEditorToolbar::TextEditorToolbar(TextEditor& editor)
    : m_editor(editor), m_save(nullptr), m_wordWrap(nullptr), m_explore(nullptr),
      m_path(nullptr)
{
  m_save = new QAction(QIcon(":/MO/gui/save"), QObject::tr("&Save"), &editor);

  m_save->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  m_save->setShortcut(Qt::CTRL | Qt::Key_S);
  m_editor.addAction(m_save);

  m_wordWrap =
      new QAction(QIcon(":/MO/gui/word-wrap"), QObject::tr("&Word wrap"), &editor);

  m_wordWrap->setCheckable(true);

  m_explore = new QAction(QObject::tr("&Open in Explorer"), &editor);

  m_path = new QLineEdit;
  m_path->setReadOnly(true);

  QObject::connect(m_save, &QAction::triggered, [&] {
    m_editor.save();
  });
  QObject::connect(m_wordWrap, &QAction::triggered, [&] {
    m_editor.toggleWordWrap();
  });
  QObject::connect(m_explore, &QAction::triggered, [&] {
    m_editor.explore();
  });

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setAlignment(Qt::AlignLeft);

  auto* b = new QToolButton;
  b->setDefaultAction(m_save);
  layout->addWidget(b);

  b = new QToolButton;
  b->setDefaultAction(m_wordWrap);
  layout->addWidget(b);

  b = new QToolButton;
  b->setDefaultAction(m_explore);
  layout->addWidget(b);

  layout->addWidget(m_path);

  QObject::connect(&m_editor, &TextEditor::modified, [&](bool b) {
    onTextModified(b);
  });
  QObject::connect(&m_editor, &TextEditor::wordWrapChanged, [&](bool b) {
    onWordWrap(b);
  });
  QObject::connect(&m_editor, &TextEditor::loaded, [&](QString f) {
    onLoaded(f);
  });
}

void TextEditorToolbar::onTextModified(bool b)
{
  m_save->setEnabled(b);
}

void TextEditorToolbar::onWordWrap(bool b)
{
  m_wordWrap->setChecked(b);
}

void TextEditorToolbar::onLoaded(const QString& path)
{
  const auto hasDoc = !path.isEmpty();

  m_explore->setEnabled(hasDoc);
  m_wordWrap->setEnabled(hasDoc);
  m_path->setEnabled(hasDoc);
  m_path->setText(path);
}

void HTMLEditor::focusOutEvent(QFocusEvent* e)
{
  if (document() && document()->isModified()) {
    emit editingFinished();
  }

  QTextEdit::focusInEvent(e);
}
