#include "modflagicondelegate.h"
#include <log.h>
#include <QList>

using namespace MOBase;

ModFlagIconDelegate::ModFlagIconDelegate(QObject *parent, int logicalIndex, int compactSize)
  : IconDelegate(parent)
  , m_LogicalIndex(logicalIndex)
  , m_CompactSize(compactSize)
  , m_Compact(false)
{
}

void ModFlagIconDelegate::columnResized(int logicalIndex, int, int newSize)
{
  if (logicalIndex == m_LogicalIndex) {
    m_Compact = newSize < m_CompactSize;
  }
}

QList<QString> ModFlagIconDelegate::getIconsForFlags(
  std::vector<ModInfo::EFlag> flags, bool compact)
{
  QList<QString> result;

  // Don't do flags for overwrite
  if (std::find(flags.begin(), flags.end(), ModInfo::FLAG_OVERWRITE) != flags.end())
    return result;

  for (auto iter = flags.begin(); iter != flags.end(); ++iter) {
    auto iconPath = getFlagIcon(*iter);
    if (!iconPath.isEmpty())
      result.append(iconPath);
  }

  return result;
}

QList<QString> ModFlagIconDelegate::getIcons(const QModelIndex &index) const
{
  QVariant modid = index.data(Qt::UserRole + 1);

  if (modid.isValid()) {
    ModInfo::Ptr info = ModInfo::getByIndex(modid.toInt());
    return getIconsForFlags(info->getFlags(), m_Compact);
  }

  return {};
}

QString ModFlagIconDelegate::getFlagIcon(ModInfo::EFlag flag)
{
  switch (flag) {
    case ModInfo::FLAG_BACKUP: return QStringLiteral(":/MO/gui/emblem_backup");
    case ModInfo::FLAG_INVALID: return QStringLiteral(":/MO/gui/problem");
    case ModInfo::FLAG_NOTENDORSED: return QStringLiteral(":/MO/gui/emblem_notendorsed");
    case ModInfo::FLAG_NOTES: return QStringLiteral(":/MO/gui/emblem_notes");
    case ModInfo::FLAG_ALTERNATE_GAME: return QStringLiteral(":/MO/gui/alternate_game");
    case ModInfo::FLAG_FOREIGN: return QString();
    case ModInfo::FLAG_SEPARATOR: return QString();
    case ModInfo::FLAG_OVERWRITE: return QString();
    case ModInfo::FLAG_PLUGIN_SELECTED: return QString();
    case ModInfo::FLAG_TRACKED: return QStringLiteral(":/MO/gui/tracked");
    default:
      log::warn("ModInfo flag {} has no defined icon", flag);
      return QString();
  }
}

size_t ModFlagIconDelegate::getNumIcons(const QModelIndex &index) const
{
  unsigned int modIdx = index.data(Qt::UserRole + 1).toInt();
  if (modIdx < ModInfo::getNumMods()) {
    ModInfo::Ptr info = ModInfo::getByIndex(modIdx);
    std::vector<ModInfo::EFlag> flags = info->getFlags();
    return flags.size();
  } else {
    return 0;
  }
}


QSize ModFlagIconDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &modelIndex) const
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

