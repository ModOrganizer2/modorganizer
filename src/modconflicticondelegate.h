#ifndef MODCONFLICTICONDELEGATE_H
#define MODCONFLICTICONDELEGATE_H

#include <array>

#include "icondelegate.h"

class ModConflictIconDelegate : public IconDelegate
{
  Q_OBJECT;

public:
  explicit ModConflictIconDelegate(QObject *parent = 0, int logicalIndex = -1, int compactSize = 80);
  QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

  static QList<QString> getIconsForFlags(
    std::vector<ModInfo::EConflictFlag> flags, bool compact);

  static QString getFlagIcon(ModInfo::EConflictFlag flag);

public slots:
  void columnResized(int logicalIndex, int oldSize, int newSize);

protected:
  QList<QString> getIcons(const QModelIndex &index) const override;
  size_t getNumIcons(const QModelIndex &index) const override;

private:
  static constexpr std::array s_ConflictFlags{
    ModInfo::FLAG_CONFLICT_MIXED,
    ModInfo::FLAG_CONFLICT_OVERWRITE,
    ModInfo::FLAG_CONFLICT_OVERWRITTEN,
    ModInfo::FLAG_CONFLICT_REDUNDANT
  };
  static constexpr std::array s_ArchiveLooseConflictFlags{
    ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITE,
    ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITTEN
  };
  static constexpr std::array s_ArchiveConflictFlags{
    ModInfo::FLAG_ARCHIVE_CONFLICT_MIXED,
    ModInfo::FLAG_ARCHIVE_CONFLICT_OVERWRITE,
    ModInfo::FLAG_ARCHIVE_CONFLICT_OVERWRITTEN
  };

  int m_LogicalIndex;
  int m_CompactSize;
  bool m_Compact;
};

#endif // MODCONFLICTICONDELEGATE_H
