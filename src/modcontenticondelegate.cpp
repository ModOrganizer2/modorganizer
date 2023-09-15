#include "modcontenticondelegate.h"

#include "modlistview.h"

ModContentIconDelegate::ModContentIconDelegate(ModListView* view, int column,
                                               int compactSize)
    : IconDelegate(view, column, compactSize), m_view(view)
{}

QList<QString> ModContentIconDelegate::getIcons(const QModelIndex& index) const
{
  QVariant modIndex = index.data(ModList::IndexRole);

  if (!modIndex.isValid()) {
    return {};
  }

  bool compact;
  auto icons = m_view->contentsIcons(index, &compact);

  if (compact || this->compact()) {
    icons.removeAll(QString());
  }

  return icons;
}

size_t ModContentIconDelegate::getNumIcons(const QModelIndex& index) const
{
  return getIcons(index).size();
}

bool ModContentIconDelegate::helpEvent(QHelpEvent* event, QAbstractItemView* view,
                                       const QStyleOptionViewItem& option,
                                       const QModelIndex& index)
{
  if (!event || !view) {
    return false;
  }
  if (event->type() == QEvent::ToolTip) {
    // this code is from QAbstractItemDelegate::helpEvent, only the the way
    // text is retrieved has been changed
    QHelpEvent* he      = static_cast<QHelpEvent*>(event);
    const int precision = inherits("QItemDelegate")
                              ? 10
                              : 6;  // keep in sync with DBL_DIG in qitemdelegate.cpp
    const QString tooltip =
        index.isValid() ? m_view->contentsTooltip(index) : QString();
    QRect rect;
    if (index.isValid()) {
      const QRect r = view->visualRect(index);
      rect          = QRect(view->mapToGlobal(r.topLeft()), r.size());
    }
    QToolTip::showText(he->globalPos(), tooltip, view, rect);
    event->setAccepted(!tooltip.isEmpty());
    return true;
  } else {
    return IconDelegate::helpEvent(event, view, option, index);
  }
}
