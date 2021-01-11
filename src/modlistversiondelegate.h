#ifndef MODLISTVERSIONDELEGATE_H
#define MODLISTVERSIONDELEGATE_H

#include <QStyledItemDelegate>

class ModListView;

class ModListVersionDelegate : public QItemDelegate
{
public:
  ModListVersionDelegate(ModListView* view);

  void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;


private:

  ModListView* m_view;
};

#endif
