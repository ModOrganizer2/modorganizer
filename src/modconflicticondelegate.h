#ifndef MODCONFLICTICONDELEGATE_H
#define MODCONFLICTICONDELEGATE_H

#include "icondelegate.h"

class ModConflictIconDelegate : public IconDelegate
{
  Q_OBJECT;

public:
  explicit ModConflictIconDelegate(QObject *parent = 0, int logicalIndex = -1, int compactSize = 80);
  virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;

  static QList<QString> getIconsForFlags(
    std::vector<ModInfo::EConflictFlag> flags, bool compact);

  static QString getFlagIcon(ModInfo::EConflictFlag flag);

public slots:
  void columnResized(int logicalIndex, int oldSize, int newSize);

protected:
  virtual QList<QString> getIcons(const QModelIndex &index) const;
  virtual size_t getNumIcons(const QModelIndex &index) const;

private:
  static ModInfo::EConflictFlag m_ConflictFlags[4];
  static ModInfo::EConflictFlag m_ArchiveLooseConflictFlags[2];
  static ModInfo::EConflictFlag m_ArchiveConflictFlags[3];

  int m_LogicalIndex;
  int m_CompactSize;
  bool m_Compact;
};

#endif // MODCONFLICTICONDELEGATE_H
