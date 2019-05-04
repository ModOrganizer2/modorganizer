#ifndef MODFLAGICONDELEGATE_H
#define MODFLAGICONDELEGATE_H

#include "icondelegate.h"

class ModFlagIconDelegate : public IconDelegate
{
Q_OBJECT

public:
  explicit ModFlagIconDelegate(QObject *parent = 0, int logicalIndex = -1, int compactSize = 120);
  virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;

public slots:
  void columnResized(int logicalIndex, int oldSize, int newSize);

private:
  virtual QList<QString> getIcons(const QModelIndex &index) const;
  virtual size_t getNumIcons(const QModelIndex &index) const;

  QString getFlagIcon(ModInfo::EFlag flag) const;

private:
  static ModInfo::EFlag m_ConflictFlags[4];
  static ModInfo::EFlag m_ArchiveLooseConflictFlags[2];
  static ModInfo::EFlag m_ArchiveConflictFlags[3];

  int m_LogicalIndex;
  int m_CompactSize;
  bool m_Compact;
};

#endif // MODFLAGICONDELEGATE_H
