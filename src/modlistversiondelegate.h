#ifndef MODLISTVERSIONDELEGATE_H
#define MODLISTVERSIONDELEGATE_H

#include <QStyledItemDelegate>

class ModListView;
class Settings;

class ModListVersionDelegate : public QItemDelegate
{
public:
  ModListVersionDelegate(ModListView* view, Settings& settings);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;

private:
  ModListView* m_view;
  Settings& m_settings;
};

#endif
