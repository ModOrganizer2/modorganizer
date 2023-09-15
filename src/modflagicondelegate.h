#ifndef MODFLAGICONDELEGATE_H
#define MODFLAGICONDELEGATE_H

#include <QTreeView>

#include "icondelegate.h"
#include "modinfo.h"

class ModListView;

class ModFlagIconDelegate : public IconDelegate
{
  Q_OBJECT;

public:
  explicit ModFlagIconDelegate(ModListView* view, int column = -1,
                               int compactSize = 120);
  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override;

protected:
  static QList<QString> getIconsForFlags(std::vector<ModInfo::EFlag> flags,
                                         bool compact);
  static QString getFlagIcon(ModInfo::EFlag flag);

  QList<QString> getIcons(const QModelIndex& index) const override;
  size_t getNumIcons(const QModelIndex& index) const override;

  // constructor for color table
  //
  ModFlagIconDelegate() : ModFlagIconDelegate(nullptr) {}

private:
  ModListView* m_view;
};

#endif  // MODFLAGICONDELEGATE_H
