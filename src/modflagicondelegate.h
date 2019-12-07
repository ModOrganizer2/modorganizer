#ifndef MODFLAGICONDELEGATE_H
#define MODFLAGICONDELEGATE_H

#include "icondelegate.h"

class ModFlagIconDelegate : public IconDelegate
{
  Q_OBJECT;

public:
  explicit ModFlagIconDelegate(QObject *parent = 0, int logicalIndex = -1, int compactSize = 120);
  virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;

  static QList<QString> getIconsForFlags(
    std::vector<ModInfo::EFlag> flags, bool compact);

  static QString getFlagIcon(ModInfo::EFlag flag);

public slots:
  void columnResized(int logicalIndex, int oldSize, int newSize);

protected:
  virtual QList<QString> getIcons(const QModelIndex &index) const;
  virtual size_t getNumIcons(const QModelIndex &index) const;

private:
  int m_LogicalIndex;
  int m_CompactSize;
  bool m_Compact;
};

#endif // MODFLAGICONDELEGATE_H
