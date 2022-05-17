#include "modconflicticondelegate.h"
#include "modlist.h"
#include "modlistview.h"
#include <QList>
#include <log.h>

using namespace MOBase;

ModConflictIconDelegate::ModConflictIconDelegate(ModListView* view, int logicalIndex,
                                                 int compactSize)
    : IconDelegate(view, logicalIndex, compactSize), m_view(view)
{}

QList<QString>
ModConflictIconDelegate::getIconsForFlags(std::vector<ModInfo::EConflictFlag> flags,
                                          bool compact)
{
  QList<QString> result;

  // Don't do flags for overwrite
  if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE_CONFLICT) !=
      flags.end())
    return result;

  // insert conflict icons to provide nicer alignment
  {  // insert loose file conflicts first
    auto iter = std::find_first_of(flags.begin(), flags.end(), s_ConflictFlags.begin(),
                                   s_ConflictFlags.end());
    if (iter != flags.end()) {
      result.append(getFlagIcon(*iter));
      flags.erase(iter);
    } else if (!compact) {
      result.append(QString());
    }
  }

  {  // insert loose vs archive overwrite second
    auto iter = std::find(flags.begin(), flags.end(),
                          ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITE);
    if (iter != flags.end()) {
      result.append(getFlagIcon(*iter));
      flags.erase(iter);
    } else if (!compact) {
      result.append(QString());
    }
  }

  {  // insert loose vs archive overwritten third
    auto iter = std::find(flags.begin(), flags.end(),
                          ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITTEN);
    if (iter != flags.end()) {
      result.append(getFlagIcon(*iter));
      flags.erase(iter);
    } else if (!compact) {
      result.append(QString());
    }
  }

  {  // insert archive conflicts last
    auto iter =
        std::find_first_of(flags.begin(), flags.end(), s_ArchiveConflictFlags.begin(),
                           s_ArchiveConflictFlags.end());
    if (iter != flags.end()) {
      result.append(getFlagIcon(*iter));
      flags.erase(iter);
    } else if (!compact) {
      result.append(QString());
    }
  }

  for (auto iter = flags.begin(); iter != flags.end(); ++iter) {
    auto iconPath = getFlagIcon(*iter);
    if (!iconPath.isEmpty())
      result.append(iconPath);
  }

  return result;
}

QList<QString> ModConflictIconDelegate::getIcons(const QModelIndex& index) const
{
  QVariant modIndex = index.data(ModList::IndexRole);

  if (!modIndex.isValid()) {
    return {};
  }

  bool compact;
  auto flags = m_view->conflictFlags(index, &compact);
  return getIconsForFlags(flags, compact || this->compact());
}

QString ModConflictIconDelegate::getFlagIcon(ModInfo::EConflictFlag flag)
{
  switch (flag) {
  case ModInfo::FLAG_CONFLICT_MIXED:
    return QStringLiteral(":/MO/gui/emblem_conflict_mixed");
  case ModInfo::FLAG_CONFLICT_OVERWRITE:
    return QStringLiteral(":/MO/gui/emblem_conflict_overwrite");
  case ModInfo::FLAG_CONFLICT_OVERWRITTEN:
    return QStringLiteral(":/MO/gui/emblem_conflict_overwritten");
  case ModInfo::FLAG_CONFLICT_REDUNDANT:
    return QStringLiteral(":/MO/gui/emblem_conflict_redundant");
  case ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITE:
    return QStringLiteral(":/MO/gui/archive_loose_conflict_overwrite");
  case ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITTEN:
    return QStringLiteral(":/MO/gui/archive_loose_conflict_overwritten");
  case ModInfo::FLAG_ARCHIVE_CONFLICT_MIXED:
    return QStringLiteral(":/MO/gui/archive_conflict_mixed");
  case ModInfo::FLAG_ARCHIVE_CONFLICT_OVERWRITE:
    return QStringLiteral(":/MO/gui/archive_conflict_winner");
  case ModInfo::FLAG_ARCHIVE_CONFLICT_OVERWRITTEN:
    return QStringLiteral(":/MO/gui/archive_conflict_loser");
  case ModInfo::FLAG_OVERWRITE_CONFLICT:
    return QString();
  default:
    log::warn("ModInfo flag {} has no defined icon", flag);
    return QString();
  }
}

size_t ModConflictIconDelegate::getNumIcons(const QModelIndex& index) const
{
  QVariant modIndex = index.data(ModList::IndexRole);

  if (!modIndex.isValid()) {
    return 0;
  }

  return m_view->conflictFlags(index, nullptr).size();
}

QSize ModConflictIconDelegate::sizeHint(const QStyleOptionViewItem& option,
                                        const QModelIndex& modelIndex) const
{
  size_t count       = getNumIcons(modelIndex);
  unsigned int index = modelIndex.data(ModList::IndexRole).toInt();
  QSize result;
  if (index < ModInfo::getNumMods()) {
    result = QSize(static_cast<int>(count) * 40, 20);
  } else {
    result = QSize(1, 20);
  }
  if (option.rect.width() > 0) {
    result.setWidth(std::min(option.rect.width(), result.width()));
  }
  return result;
}
