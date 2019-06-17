#include "texteditor.h"
#include "utility.h"
#include <QSplitter>

TextEditor::TextEditor(QWidget* parent) :
  QPlainTextEdit(parent),
  m_toolbar(*this), m_lineNumbers(nullptr), m_highlighter(nullptr),
  m_dirty(false)
{
  setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  wordWrap(true);

  m_lineNumbers = new TextEditorLineNumbers(*this);
  m_highlighter = new TextEditorHighlighter(document());

  emit modified(false);

  QObject::connect(
    document(), &QTextDocument::modificationChanged,
    [&](bool b){ onModified(b); });
}

bool TextEditor::load(const QString& filename)
{
  m_filename = filename;
  setPlainText(MOBase::readFileText(filename, &m_encoding));
  document()->setModified(false);

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

  QTextCodec* codec = QTextCodec::codecForName(m_encoding.toUtf8());
  QString data = toPlainText().replace("\n", "\r\n");

  file.write(codec->fromUnicode(data));
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

QString TextEditor::textColor() const
{
  return m_highlighter->textColor().name(QColor::HexArgb);
}

void TextEditor::setTextColor(const QString& s)
{
  const QColor c(s);
  if (!c.isValid()) {
    qWarning() << "TextEditor: invalid text color '" << s << "'";
    return;
  }

  m_highlighter->setTextColor(c);
}

QString TextEditor::backgroundColor() const
{
  return m_highlighter->backgroundColor().name(QColor::HexArgb);
}

void TextEditor::setBackgroundColor(const QString& s)
{
  const QColor c(s);
  if (!c.isValid()) {
    qWarning() << "TextEditor: invalid backgorund color '" << s << "'";
    return;
  }

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

void TextEditor::onModified(bool b)
{
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
  layout->addWidget(m_toolbar.widget());
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

  if (auto* parentLayout=parentWidget()->layout()) {
    // the edit's parent has a regular layout, replace the edit by the new
    // widget and delete the QLayoutItem that's returned as it's not needed
    delete parentLayout->replaceWidget(this, widget.get());

  } else if (auto* splitter=qobject_cast<QSplitter*>(parentWidget())) {
    // the edit's parent is a QSplitter, which doesn't have a layout; replace
    // the edit by using its index in the splitter
    auto index = splitter->indexOf(this);

    if (index == -1) {
      qCritical(
        "TextEditor: cannot wrap edit widget to display a toolbar, "
        "parent is a splitter, but widget isn't in it");

      return nullptr;
    }

    splitter->replaceWidget(index, widget.get());

  } else {
    // unknown parent
    qCritical(
      "TextEditor: cannot wrap edit widget to display a toolbar, "
      "no parent or parent has no layout");

    return nullptr;
  }

  return widget.release();
}

void TextEditor::resizeEvent(QResizeEvent* e)
{
  QPlainTextEdit::resizeEvent(e);

  QRect cr = contentsRect();
  m_lineNumbers->setGeometry(QRect(cr.left(), cr.top(), m_lineNumbers->areaWidth(), cr.height()));
}

void TextEditor::paintLineNumbers(QPaintEvent* e)
{
  QPainter painter(m_lineNumbers);
  painter.fillRect(e->rect(), Qt::lightGray);

  QTextBlock block = firstVisibleBlock();
  int blockNumber = block.blockNumber();
  int top = (int) blockBoundingGeometry(block).translated(contentOffset()).top();
  int bottom = top + (int) blockBoundingRect(block).height();

  while (block.isValid() && top <= e->rect().bottom()) {
    if (block.isVisible() && bottom >= e->rect().top()) {
      QString number = QString::number(blockNumber + 1);
      painter.setPen(Qt::black);

      painter.drawText(
        0, top, m_lineNumbers->width(), fontMetrics().height(),
        Qt::AlignRight, number);
    }

    block = block.next();
    top = bottom;
    bottom = top + (int) blockBoundingRect(block).height();
    ++blockNumber;
  }
}


TextEditorHighlighter::TextEditorHighlighter(QTextDocument* doc) :
  QSyntaxHighlighter(doc),
  m_background(QColor("transparent")),
  m_text(QColor("black"))
{
}

QColor TextEditorHighlighter::backgroundColor()
{
  return m_background;
}

void TextEditorHighlighter::setBackgroundColor(const QColor& c)
{
  m_background = c;
  changed();
}

QColor TextEditorHighlighter::textColor()
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
  : QWidget(&editor), m_editor(editor)
{
  setFont(editor.font());

  connect(&m_editor, &QPlainTextEdit::blockCountChanged, [&]{ updateAreaWidth(); });
  connect(&m_editor, &QPlainTextEdit::updateRequest, [&](auto&& rect, int dy){ updateArea(rect, dy); });
  connect(&m_editor, &QPlainTextEdit::cursorPositionChanged, [&]{ highlightCurrentLine(); });

  updateAreaWidth();
  highlightCurrentLine();
}

QSize TextEditorLineNumbers::sizeHint() const
{
  return QSize(areaWidth(), 0);
}

void TextEditorLineNumbers::paintEvent(QPaintEvent* e)
{
  m_editor.paintLineNumbers(e);
}

int TextEditorLineNumbers::areaWidth() const
{
  int digits = 1;
  int max = std::max(1, m_editor.blockCount());

  while (max >= 10) {
    max /= 10;
    ++digits;
  }

  digits = std::max(3, digits);

  int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;

  return space;
}

void TextEditorLineNumbers::updateAreaWidth()
{
  m_editor.setViewportMargins(areaWidth(), 0, 0, 0);
}

void TextEditorLineNumbers::updateArea(const QRect &rect, int dy)
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

void TextEditorLineNumbers::highlightCurrentLine()
{
  QList<QTextEdit::ExtraSelection> extraSelections;

  if (!m_editor.isReadOnly()) {
    QTextEdit::ExtraSelection selection;

    QColor lineColor = QColor(Qt::yellow).lighter(160);

    selection.format.setBackground(lineColor);
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = m_editor.textCursor();
    selection.cursor.clearSelection();
    extraSelections.append(selection);
  }

  m_editor.setExtraSelections(extraSelections);
}


TextEditorToolbar::TextEditorToolbar(TextEditor& editor) :
  m_editor(editor),
  m_widget(new QWidget),
  m_save(new QAction(QObject::tr("&Save"))),
  m_wordWrap(new QAction(QObject::tr("&Word wrap")))
{
  QObject::connect(m_save, &QAction::triggered, [&]{ m_editor.save(); });

  m_wordWrap->setCheckable(true);
  QObject::connect(m_wordWrap, &QAction::triggered, [&]{ m_editor.toggleWordWrap(); });

  auto* layout = new QHBoxLayout(m_widget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setAlignment(Qt::AlignLeft);

  auto* b = new QToolButton;
  b->setDefaultAction(m_save);
  layout->addWidget(b);

  b = new QToolButton;
  b->setDefaultAction(m_wordWrap);
  layout->addWidget(b);

  QObject::connect(&m_editor, &TextEditor::modified, [&](bool b){ onTextModified(b); });
  QObject::connect(&m_editor, &TextEditor::wordWrapChanged, [&](bool b){ onWordWrap(b); });
}

QWidget* TextEditorToolbar::widget()
{
  return m_widget;
}

void TextEditorToolbar::onTextModified(bool b)
{
  m_save->setEnabled(b);
}

void TextEditorToolbar::onWordWrap(bool b)
{
  m_wordWrap->setChecked(b);
}
