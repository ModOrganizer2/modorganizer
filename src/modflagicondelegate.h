#ifndef MODFLAGICONDELEGATE_H
#define MODFLAGICONDELEGATE_H

#include "icondelegate.h"

class ModFlagIconDelegate : public IconDelegate
{
  Q_OBJECT;

public:
  explicit ModFlagIconDelegate(QObject *parent = 0, int logicalIndex = -1, int compactSize = 120);
  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

  static QList<QString> getIconsForFlags(
    std::vector<ModInfo::EFlag> flags, bool compact);

  static QString getFlagIcon(ModInfo::EFlag flag);

public slots:
  void columnResized(int logicalIndex, int oldSize, int newSize);

protected:
  QList<QString> getIcons(const QModelIndex &index) const override;
  size_t getNumIcons(const QModelIndex &index) const override;

private:
  int m_LogicalIndex;
  int m_CompactSize;
  bool m_Compact;
};

#endif // MODFLAGICONDELEGATE_H
