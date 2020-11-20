#include "modconflicticondelegate.h"
#include <log.h>

using namespace MOBase;

ModInfo::EConflictFlag ModConflictIconDelegate::m_ConflictFlags[4] = { ModInfo::FLAG_CONFLICT_MIXED
                                                         , ModInfo::FLAG_CONFLICT_OVERWRITE
                                                         , ModInfo::FLAG_CONFLICT_OVERWRITTEN
                                                         , ModInfo::FLAG_CONFLICT_REDUNDANT };

ModInfo::EConflictFlag ModConflictIconDelegate::m_ArchiveLooseConflictFlags[2] = { ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITE
                                                                     , ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITTEN };

ModInfo::EConflictFlag ModConflictIconDelegate::m_ArchiveConflictFlags[3] = { ModInfo::FLAG_ARCHIVE_CONFLICT_MIXED
                                                                , ModInfo::FLAG_ARCHIVE_CONFLICT_OVERWRITE
                                                                , ModInfo::FLAG_ARCHIVE_CONFLICT_OVERWRITTEN };

ModConflictIconDelegate::ModConflictIconDelegate(QObject *parent, int logicalIndex, int compactSize)
  : IconDelegate(parent)
  , m_LogicalIndex(logicalIndex)
  , m_CompactSize(compactSize)
  , m_Compact(false)
{
}

void ModConflictIconDelegate::columnResized(int logicalIndex, int, int newSize)
{
  if (logicalIndex == m_LogicalIndex) {
    m_Compact = newSize < m_CompactSize;
  }
}

QList<QString> ModConflictIconDelegate::getIconsForFlags(
  std::vector<ModInfo::EConflictFlag> flags, bool compact)
{
  QList<QString> result;

  // Don't do flags for overwrite
  if (std::find(flags.begin(), flags.end(),ModInfo::FLAG_OVERWRITE_CONFLICT) != flags.end())
    return result;

  // insert conflict icons to provide nicer alignment
  { // insert loose file conflicts first
    auto iter = std::find_first_of(flags.begin(), flags.end(),
                                    m_ConflictFlags, m_ConflictFlags + 4);
    if (iter != flags.end()) {
      result.append(getFlagIcon(*iter));
      flags.erase(iter);
    } else if (!compact) {
      result.append(QString());
    }
  }

  { // insert loose vs archive overwrite second
    auto iter = std::find(flags.begin(), flags.end(),
      ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITE);
    if (iter != flags.end()) {
      result.append(getFlagIcon(*iter));
      flags.erase(iter);
    } else if (!compact) {
      result.append(QString());
    }
  }

  { // insert loose vs archive overwritten third
    auto iter = std::find_first_of(flags.begin(), flags.end(),
      m_ArchiveLooseConflictFlags + 1, m_ArchiveLooseConflictFlags + 2);
    if (iter != flags.end()) {
      result.append(getFlagIcon(*iter));
      flags.erase(iter);
    } else if (!compact) {
      result.append(QString());
    }
  }

  { // insert archive conflicts last
    auto iter = std::find_first_of(flags.begin(), flags.end(),
      m_ArchiveConflictFlags, m_ArchiveConflictFlags + 3);
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

QList<QString> ModConflictIconDelegate::getIcons(const QModelIndex &index) const
{
  QVariant modid = index.data(Qt::UserRole + 1);

  if (modid.isValid()) {
    ModInfo::Ptr info = ModInfo::getByIndex(modid.toInt());
    return getIconsForFlags(info->getConflictFlags(), m_Compact);
  }

  return {};
}

QString ModConflictIconDelegate::getFlagIcon(ModInfo::EConflictFlag flag)
{
  switch (flag) {
    case ModInfo::FLAG_CONFLICT_MIXED: return QStringLiteral(":/MO/gui/emblem_conflict_mixed");
    case ModInfo::FLAG_CONFLICT_OVERWRITE: return QStringLiteral(":/MO/gui/emblem_conflict_overwrite");
    case ModInfo::FLAG_CONFLICT_OVERWRITTEN: return QStringLiteral(":/MO/gui/emblem_conflict_overwritten");
    case ModInfo::FLAG_CONFLICT_REDUNDANT: return QStringLiteral(":/MO/gui/emblem_conflict_redundant");
    case ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITE: return QStringLiteral(":/MO/gui/archive_loose_conflict_overwrite");
    case ModInfo::FLAG_ARCHIVE_LOOSE_CONFLICT_OVERWRITTEN: return QStringLiteral(":/MO/gui/archive_loose_conflict_overwritten");
    case ModInfo::FLAG_ARCHIVE_CONFLICT_MIXED: return QStringLiteral(":/MO/gui/archive_conflict_mixed");
    case ModInfo::FLAG_ARCHIVE_CONFLICT_OVERWRITE: return QStringLiteral(":/MO/gui/archive_conflict_winner");
    case ModInfo::FLAG_ARCHIVE_CONFLICT_OVERWRITTEN: return QStringLiteral(":/MO/gui/archive_conflict_loser");
    case ModInfo::FLAG_OVERWRITE_CONFLICT: return QString();
    default:
      log::warn("ModInfo flag {} has no defined icon", flag);
      return QString();
  }
}

size_t ModConflictIconDelegate::getNumIcons(const QModelIndex &index) const
{
  unsigned int modIdx = index.data(Qt::UserRole + 1).toInt();
  if (modIdx < ModInfo::getNumMods()) {
    ModInfo::Ptr info = ModInfo::getByIndex(modIdx);
    std::vector<ModInfo::EConflictFlag> flags = info->getConflictFlags();
    size_t count = flags.size();
    if (std::find_first_of(flags.begin(), flags.end(), m_ConflictFlags, m_ConflictFlags + 4) == flags.end()) {
      ++count;
    }
    return count;
  } else {
    return 0;
  }
}


QSize ModConflictIconDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &modelIndex) const
{
  size_t count = getNumIcons(modelIndex);
  unsigned int index = modelIndex.data(Qt::UserRole + 1).toInt();
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

