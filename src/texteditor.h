#ifndef MO_TEXTEDITOR_H
#define MO_TEXTEDITOR_H

#include <QPlainTextEdit>

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
  void changed(bool b);

private:
  QPlainTextEdit* m_edit;
  QString m_filename;
  QString m_encoding;
  bool m_dirty;

  void onChanged(bool b);
  void dirty(bool b);
};

#endif // MO_TEXTEDITOR_H
