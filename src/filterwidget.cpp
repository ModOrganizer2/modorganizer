#include "filterwidget.h"
#include "eventfilter.h"

FilterWidgetProxyModel::FilterWidgetProxyModel(FilterWidget& fw, QWidget* parent)
  : QSortFilterProxyModel(parent), m_filter(fw)
{
  connect(&fw, &FilterWidget::changed, [&]{ invalidateFilter(); });
}

bool FilterWidgetProxyModel::filterAcceptsRow(
  int sourceRow, const QModelIndex& sourceParent) const
{
  const auto cols = sourceModel()->columnCount();

  const auto m = m_filter.matches([&](auto&& what) {
    for (int c=0; c<cols; ++c) {
      QModelIndex index = sourceModel()->index(sourceRow, c, sourceParent);
      const auto text = sourceModel()->data(index, Qt::DisplayRole).toString();

      if (text.contains(what, Qt::CaseInsensitive)) {
        return true;
      }
    }

    return false;
    });

  return m;
}


FilterWidget::FilterWidget() :
  m_edit(nullptr), m_list(nullptr), m_proxy(nullptr),
  m_eventFilter(nullptr), m_clear(nullptr)
{
}

void FilterWidget::setEdit(QLineEdit* edit)
{
  unhook();

  m_edit = edit;

  if (!m_edit) {
    return;
  }

  m_edit->setPlaceholderText(QObject::tr("Filter"));

  createClear();
  hookEvents();
  clear();
}

void FilterWidget::setList(QAbstractItemView* list)
{
  m_list = list;

  m_proxy = new FilterWidgetProxyModel(*this);
  m_proxy->setSourceModel(m_list->model());
  m_list->setModel(m_proxy);
}

void FilterWidget::clear()
{
  if (!m_edit) {
    return;
  }

  m_edit->clear();
}

QModelIndex FilterWidget::map(const QModelIndex& index)
{
  if (m_proxy) {
    return m_proxy->mapToSource(index);
  } else {
    qCritical() << "FilterWidget::map() called, but proxy isn't set up";
    return index;
  }
}

void FilterWidget::compile()
{
  m_compiled.clear();

  const QStringList ORList = [&] {
    QString filterCopy = QString(m_text);
    filterCopy.replace("||", ";").replace("OR", ";").replace("|", ";");
    return filterCopy.split(";", QString::SkipEmptyParts);
  }();

  // split in ORSegments that internally use AND logic
  for (auto& ORSegment : ORList) {
    m_compiled.push_back(ORSegment.split(" ", QString::SkipEmptyParts));
  }
}

bool FilterWidget::matches(std::function<bool (const QString& what)> pred) const
{
  if (m_compiled.isEmpty() || !pred) {
    return true;
  }

  for (auto& ANDKeywords : m_compiled) {
    bool segmentGood = true;

    // check each word in the segment for match, each word needs to be matched
    // but it doesn't matter where.
    for (auto& currentKeyword : ANDKeywords) {
      if (!pred(currentKeyword)) {
          segmentGood = false;
      }
    }

    if (segmentGood) {
      // the last AND loop didn't break so the ORSegments is true so mod
      // matches filter
      return true;
    }
  }

  return false;
}

void FilterWidget::unhook()
{
  if (m_clear) {
    delete m_clear;
    m_clear = nullptr;
  }

  if (m_edit) {
    m_edit->removeEventFilter(m_eventFilter);
  }

  if (m_proxy && m_list) {
    auto* model = m_proxy->sourceModel();
    m_proxy->setSourceModel(nullptr);
    delete m_proxy;

    m_list->setModel(model);
  }
}

void FilterWidget::createClear()
{
  m_clear = new QToolButton(m_edit);

  QPixmap pixmap(":/MO/gui/edit_clear");
  m_clear->setIcon(QIcon(pixmap));
  m_clear->setIconSize(pixmap.size());
  m_clear->setCursor(Qt::ArrowCursor);
  m_clear->setStyleSheet("QToolButton { border: none; padding: 0px; }");
  m_clear->hide();

  QObject::connect(m_clear, &QToolButton::clicked, [&]{ clear(); });
  QObject::connect(m_edit, &QLineEdit::textChanged, [&]{ onTextChanged(); });

  repositionClearButton();
}

void FilterWidget::hookEvents()
{
  m_eventFilter = new EventFilter(m_edit, [&](auto* w, auto* e) {
    if (e->type() == QEvent::Resize) {
      onResized();
    }

    return false;
  });

  m_edit->installEventFilter(m_eventFilter);
}

void FilterWidget::onTextChanged()
{
  m_clear->setVisible(!m_edit->text().isEmpty());

  const auto text = m_edit->text();

  if (text != m_text) {
    m_text = text;
    compile();

    if (m_proxy) {
      m_proxy->invalidateFilter();
    }

    emit changed();
  }
}

void FilterWidget::onResized()
{
  repositionClearButton();
}

void FilterWidget::repositionClearButton()
{
  if (!m_clear) {
    return;
  }

  const QSize sz = m_clear->sizeHint();
  const int frame = m_edit->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
  const auto r = m_edit->rect();

  const auto x = r.right() - frame - sz.width();
  const auto y = (r.bottom() + 1 - sz.height()) / 2;

  m_clear->move(x, y);
}
