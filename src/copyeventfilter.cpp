#include "copyeventfilter.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QKeyEvent>

CopyEventFilter::CopyEventFilter(QAbstractItemView* view, int column, int role)
    : CopyEventFilter(view, [=](auto& index) {
        return index.sibling(index.row(), column).data(role).toString();
      })
{}

CopyEventFilter::CopyEventFilter(QAbstractItemView* view,
                                 std::function<QString(const QModelIndex&)> format)
    : QObject(view), m_view(view), m_format(format)
{}

void CopyEventFilter::copySelection() const
{
  if (!m_view->selectionModel()->hasSelection()) {
    return;
  }

  // sort to reflect the visual order
  QModelIndexList selectedRows = m_view->selectionModel()->selectedRows();
  std::sort(selectedRows.begin(), selectedRows.end(),
            [=](const auto& lidx, const auto& ridx) {
              return m_view->visualRect(lidx).top() < m_view->visualRect(ridx).top();
            });

  QStringList rows;
  for (auto& idx : selectedRows) {
    rows.append(m_format(idx));
  }

  QGuiApplication::clipboard()->setText(rows.join("\n"));
}

bool CopyEventFilter::eventFilter(QObject* sender, QEvent* event)
{
  if (sender == m_view && event->type() == QEvent::KeyPress) {
    QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
    if (keyEvent->modifiers() == Qt::ControlModifier && keyEvent->key() == Qt::Key_C) {
      copySelection();
      return true;
    }
  }
  return QObject::eventFilter(sender, event);
}
