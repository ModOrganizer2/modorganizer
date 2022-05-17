#include "modidlineedit.h"
#include <QWhatsThis>

#include <QEvent>
#include <QWhatsThisClickedEvent>

ModIDLineEdit::ModIDLineEdit(QWidget* parent) : QLineEdit(parent) {}
ModIDLineEdit::ModIDLineEdit(const QString& text, QWidget* parent)
    : QLineEdit(text, parent)
{}

bool ModIDLineEdit::event(QEvent* event)
{
  if (event->type() == QEvent::WhatsThisClicked) {
    QWhatsThisClickedEvent* wtcEvent = static_cast<QWhatsThisClickedEvent*>(event);
    emit linkClicked(wtcEvent->href().trimmed());
  }

  return QLineEdit::event(event);
}
