#include "modlistversiondelegate.h"

#include "log.h"
#include "modlistview.h"
#include "settings.h"

ModListVersionDelegate::ModListVersionDelegate(ModListView* view, Settings& settings)
    : QItemDelegate(view), m_view(view), m_settings(settings)
{}

void ModListVersionDelegate::paint(QPainter* painter,
                                   const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const
{
  m_view->itemDelegate()->paint(painter, option, index);

  if (m_view->hasCollapsibleSeparators() && m_view->model()->hasChildren(index) &&
      m_settings.interface().collapsibleSeparatorsIcons(ModList::COL_VERSION) &&
      !m_view->isExpanded(index.sibling(index.row(), 0))) {
    auto* model = m_view->model();

    bool downgrade = false, upgrade = false;

    for (int i = 0; i < model->rowCount(index); ++i) {
      const auto mIndex =
          model->index(i, index.column(), index).data(ModList::IndexRole);
      if (mIndex.isValid()) {
        auto info = ModInfo::getByIndex(mIndex.toInt());
        downgrade = downgrade || info->downgradeAvailable();
        upgrade   = upgrade || info->updateAvailable();
      }
    }

    const int margin =
        m_view->style()->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, m_view) + 1;

    QStyleOptionViewItem opt(option);
    const int sz =
        m_view->style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, m_view);
    opt.decorationSize      = QSize(sz, sz);
    opt.decorationAlignment = Qt::AlignCenter;

    if (upgrade) {
      QIcon icon(":/MO/gui/update_available");
      QPixmap pixmap = decoration(opt, icon);

      QSize pm = icon.actualSize(opt.decorationSize);
      pm.rwidth() += 2 * margin;
      opt.rect.setRect(opt.rect.x(), opt.rect.y(), pm.width(), opt.rect.height());
      drawDecoration(painter, opt, opt.rect, pixmap);

      // add margin for next icon (if any)
      opt.rect.adjust(opt.decorationSize.width() + margin, 0, 0, 0);
    }

    if (downgrade) {
      QIcon icon(":/MO/gui/warning");
      QPixmap pixmap = decoration(opt, icon);

      QSize pm = icon.actualSize(opt.decorationSize);
      pm.rwidth() += 2 * margin;
      opt.rect.setRect(opt.rect.x(), opt.rect.y(), pm.width(), opt.rect.height());

      drawDecoration(painter, opt, opt.rect, pixmap);
    }
  }
}
