#include "modflagicondelegate.h"
#include <QList>


ModInfo::EFlag ModFlagIconDelegate::m_ConflictFlags[4] = { ModInfo::FLAG_CONFLICT_MIXED
                                                       , ModInfo::FLAG_CONFLICT_OVERWRITE
                                                       , ModInfo::FLAG_CONFLICT_OVERWRITTEN
                                                       , ModInfo::FLAG_CONFLICT_REDUNDANT };

ModFlagIconDelegate::ModFlagIconDelegate(QObject *parent)
  : IconDelegate(parent)
{
}

QList<QIcon> ModFlagIconDelegate::getIcons(const QModelIndex &index) const
{
  QList<QIcon> result;
  QVariant modid = index.data(Qt::UserRole + 1);
  if (modid.isValid()) {
    ModInfo::Ptr info = ModInfo::getByIndex(modid.toInt());
    std::vector<ModInfo::EFlag> flags = info->getFlags();

    { // insert conflict icon first to provide nicer alignment
      auto iter = std::find_first_of(flags.begin(), flags.end(), m_ConflictFlags, m_ConflictFlags + 4);
      if (iter != flags.end()) {
        result.append(getFlagIcon(*iter));
        flags.erase(iter);
      } else {
        result.append(QIcon());
      }
    }

    for (auto iter = flags.begin(); iter != flags.end(); ++iter) {
      result.append(getFlagIcon(*iter));
    }
  }
  return result;
}

QIcon ModFlagIconDelegate::getFlagIcon(ModInfo::EFlag flag) const
{
  switch (flag) {
    case ModInfo::FLAG_BACKUP: return QIcon(":/MO/gui/emblem_backup");
    case ModInfo::FLAG_INVALID: return QIcon(":/MO/gui/emblem_problem");
    case ModInfo::FLAG_NOTENDORSED: return QIcon(":/MO/gui/emblem_notendorsed");
    case ModInfo::FLAG_NOTES: return QIcon(":/MO/gui/emblem_notes");
    case ModInfo::FLAG_CONFLICT_OVERWRITE: return QIcon(":/MO/gui/emblem_conflict_overwrite");
    case ModInfo::FLAG_CONFLICT_OVERWRITTEN: return QIcon(":/MO/gui/emblem_conflict_overwritten");
    case ModInfo::FLAG_CONFLICT_MIXED: return QIcon(":/MO/gui/emblem_conflict_mixed");
    case ModInfo::FLAG_CONFLICT_REDUNDANT: return QIcon(":MO/gui/emblem_conflict_redundant");
    default: return QIcon();
  }
}

size_t ModFlagIconDelegate::getNumIcons(const QModelIndex &index) const
{
  unsigned int modIdx = index.data(Qt::UserRole + 1).toInt();
  if (modIdx < ModInfo::getNumMods()) {
    ModInfo::Ptr info = ModInfo::getByIndex(modIdx);
    std::vector<ModInfo::EFlag> flags = info->getFlags();
    int count = flags.size();
    if (std::find_first_of(flags.begin(), flags.end(), m_ConflictFlags, m_ConflictFlags + 4) == flags.end()) {
      ++count;
    }
    return count;
  } else {
    return 0;
  }
}


QSize ModFlagIconDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &modelIndex) const
{
  int count = getNumIcons(modelIndex);
  unsigned int index = modelIndex.data(Qt::UserRole + 1).toInt();
  QSize result;
  if (index < ModInfo::getNumMods()) {
    result = QSize(count * 40, 20);
  } else {
    result = QSize(1, 20);
  }
  if (option.rect.width() > 0) {
    result.setWidth(std::min(option.rect.width(), result.width()));
  }
  return result;
}

