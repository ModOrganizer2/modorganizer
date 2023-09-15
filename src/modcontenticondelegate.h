#ifndef MODCONTENTICONDELEGATE_H
#define MODCONTENTICONDELEGATE_H

#include "icondelegate.h"

class ModListView;

class ModContentIconDelegate : public IconDelegate
{
  Q_OBJECT
public:
  explicit ModContentIconDelegate(ModListView* view, int column = -1,
                                  int compactSize = 150);

  bool helpEvent(QHelpEvent* event, QAbstractItemView* view,
                 const QStyleOptionViewItem& option, const QModelIndex& index) override;

protected:
  QList<QString> getIcons(const QModelIndex& index) const override;
  size_t getNumIcons(const QModelIndex& index) const override;

  // constructor for color table
  //
  ModContentIconDelegate() : ModContentIconDelegate(nullptr) {}

private:
  ModListView* m_view;
};

#endif  // GENERICICONDELEGATE_H
