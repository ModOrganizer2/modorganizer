#ifndef NOEDITDELEGATE_H
#define NOEDITDELEGATE_H

class NoEditDelegate: public QStyledItemDelegate {
public:
  NoEditDelegate(QObject *parent = nullptr);
  virtual QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

#endif // NOEDITDELEGATE_H
