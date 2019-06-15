#include "texteditor.h"
#include "utility.h"

TextEditor::TextEditor(QPlainTextEdit* edit)
  : m_edit(edit), m_dirty(false)
{
  m_edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  wordWrap(true);

  QObject::connect(
    m_edit->document(), &QTextDocument::modificationChanged,
    [&](bool b){ onChanged(b); });
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

void TextEditor::onChanged(bool b)
{
  dirty(b);
  emit changed(b);
}
