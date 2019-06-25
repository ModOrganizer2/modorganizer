#ifndef FILTERWIDGET_H
#define FILTERWIDGET_H

#include <QObject>
#include <QLineEdit>
#include <QToolButton>
#include <QList>
#include <QAbstractItemView>

class EventFilter;
class FilterWidget;

class FilterWidgetProxyModel : public QSortFilterProxyModel
{
  Q_OBJECT;

public:
  FilterWidgetProxyModel(FilterWidget& fw, QWidget* parent=nullptr);
  using QSortFilterProxyModel::invalidateFilter;

protected:
  bool filterAcceptsRow(int row, const QModelIndex& parent) const override;

private:
  FilterWidget& m_filter;
};


class FilterWidget : public QObject
{
  Q_OBJECT;

public:
  using predFun = std::function<bool (const QString& what)>;

  FilterWidget();

  void setEdit(QLineEdit* edit);
  void setList(QAbstractItemView* list);
  void clear();
  bool empty() const;

  QModelIndex map(const QModelIndex& index);

  bool matches(predFun pred) const;

signals:
  void changed();

private:
  QLineEdit* m_edit;
  QAbstractItemView* m_list;
  FilterWidgetProxyModel* m_proxy;
  EventFilter* m_eventFilter;
  QToolButton* m_clear;
  QString m_text;
  QList<QList<QString>> m_compiled;

  void unhook();
  void createClear();
  void hookEvents();
  void repositionClearButton();

  void onTextChanged();
  void onResized();

  void compile();
};

#endif // FILTERWIDGET_H
