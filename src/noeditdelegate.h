#ifndef NOEDITDELEGATE_H
#define NOEDITDELEGATE_H

#include <QStyledItemDelegate>

class NoEditDelegate: public QStyledItemDelegate {
public:
  NoEditDelegate(QObject *parent = NULL);
  virtual QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

#endif // NOEDITDELEGATE_H
