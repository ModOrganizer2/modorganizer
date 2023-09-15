#ifndef MODCONFLICTICONDELEGATE_H
#define MODCONFLICTICONDELEGATE_H

#include <array>

#include "icondelegate.h"
#include "modinfo.h"

class ModListView;

class ModConflictIconDelegate : public IconDelegate
{
  Q_OBJECT;

public:
  explicit ModConflictIconDelegate(ModListView* view, int logicalIndex = -1,
                                   int compactSize = 80);
  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override;

protected:
  static QList<QString> getIconsForFlags(std::vector<ModInfo::EConflictFlag> flags,
                                         bool compact);
  static QString getFlagIcon(ModInfo::EConflictFlag flag);

  QList<QString> getIcons(const QModelIndex& index) const override;
  size_t getNumIcons(const QModelIndex& index) const override;

  // constructor for color table
  //
  ModConflictIconDelegate() : ModConflictIconDelegate(nullptr) {}

private:
  static constexpr std::array s_ConflictFlags{
      ModInfo::FLAG_CONFLICT_MIXED, ModInfo::FLAG_CONFLICT_OVERWRITE,
      ModInfo::FLAG_CONFLICT_OVERWRITTEN, ModInfo::FLAG_CONFLICT_REDUNDANT};
  static constexpr std::array s_ArchiveLooseConflictFlags{
      ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITE,
      ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITTEN};
  static constexpr std::array s_ArchiveConflictFlags{
      ModInfo::FLAG_ARCHIVE_CONFLICT_MIXED, ModInfo::FLAG_ARCHIVE_CONFLICT_OVERWRITE,
      ModInfo::FLAG_ARCHIVE_CONFLICT_OVERWRITTEN};

  ModListView* m_view;
};

#endif  // MODCONFLICTICONDELEGATE_H
