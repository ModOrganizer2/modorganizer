#include "texteditor.h"
#include "utility.h"
#include <QSplitter>

TextEditor::TextEditor(QPlainTextEdit* edit)
  : m_edit(edit), m_toolbar(*this), m_dirty(false)
{
  setupToolbar();

  m_edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  wordWrap(true);

  emit modified(false);

  QObject::connect(
    m_edit->document(), &QTextDocument::modificationChanged,
    [&](bool b){ onModified(b); });
}

bool TextEditor::load(const QString& filename)
{
  m_filename = filename;
  m_edit->setPlainText(MOBase::readFileText(filename, &m_encoding));
  m_edit->document()->setModified(false);

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
  QString data = m_edit->toPlainText().replace("\n", "\r\n");

  file.write(codec->fromUnicode(data));
  m_edit->document()->setModified(false);

  return true;
}

const QString& TextEditor::filename() const
{
  return m_filename;
}

void TextEditor::wordWrap(bool b)
{
  if (b) {
    m_edit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
  } else {
    m_edit->setLineWrapMode(QPlainTextEdit::NoWrap);
  }

  emit wordWrapChanged(b);
}

void TextEditor::toggleWordWrap()
{
  wordWrap(!wordWrap());
}

bool TextEditor::wordWrap() const
{
  return (m_edit->lineWrapMode() == QPlainTextEdit::WidgetWidth);
}

void TextEditor::dirty(bool b)
{
  m_dirty = b;
}

bool TextEditor::dirty() const
{
  return m_dirty;
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
  layout->addWidget(m_edit);

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

  if (auto* parentLayout=m_edit->parentWidget()->layout()) {
    // the edit's parent has a regular layout, replace the edit by the new
    // widget and delete the QLayoutItem that's returned as it's not needed
    delete parentLayout->replaceWidget(m_edit, widget.get());
  } else if (auto* splitter=qobject_cast<QSplitter*>(m_edit->parentWidget())) {
    // the edit's parent is a QSplitter, which doesn't have a layout; replace
    // the edit by using its index in the splitter
    splitter->replaceWidget(splitter->indexOf(m_edit), widget.get());
  } else {
    // unknown parent
    qCritical("TextEditor: cannot wrap edit widget to display a toolbar");
    return nullptr;
  }

  return widget.release();
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
