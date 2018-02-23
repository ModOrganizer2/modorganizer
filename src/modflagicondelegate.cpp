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

QList<QString> ModFlagIconDelegate::getIcons(const QModelIndex &index) const {
  QList<QString> result;
  QVariant modid = index.data(Qt::UserRole + 1);
  if (modid.isValid()) {
    ModInfo::Ptr info = ModInfo::getByIndex(modid.toInt());
    std::vector<ModInfo::EFlag> flags = info->getFlags();

    { // insert conflict icon first to provide nicer alignment
      auto iter = std::find_first_of(flags.begin(), flags.end(),
                                     m_ConflictFlags, m_ConflictFlags + 4);
      if (iter != flags.end()) {
        result.append(getFlagIcon(*iter));
        flags.erase(iter);
      } else {
        result.append(QString());
      }
    }

    for (auto iter = flags.begin(); iter != flags.end(); ++iter) {
      result.append(getFlagIcon(*iter));
    }
  }
  return result;
}

QString ModFlagIconDelegate::getFlagIcon(ModInfo::EFlag flag) const
{
  switch (flag) {
    case ModInfo::FLAG_BACKUP: return ":/MO/gui/emblem_backup";
    case ModInfo::FLAG_INVALID: return ":/MO/gui/problem";
    case ModInfo::FLAG_NOTENDORSED: return ":/MO/gui/emblem_notendorsed";
    case ModInfo::FLAG_NOTES: return ":/MO/gui/emblem_notes";
    case ModInfo::FLAG_CONFLICT_OVERWRITE: return ":/MO/gui/emblem_conflict_overwrite";
    case ModInfo::FLAG_CONFLICT_OVERWRITTEN: return ":/MO/gui/emblem_conflict_overwritten";
    case ModInfo::FLAG_CONFLICT_MIXED: return ":/MO/gui/emblem_conflict_mixed";
    case ModInfo::FLAG_CONFLICT_REDUNDANT: return ":MO/gui/emblem_conflict_redundant";
    default: return QString();
  }
}

size_t ModFlagIconDelegate::getNumIcons(const QModelIndex &index) const
{
  unsigned int modIdx = index.data(Qt::UserRole + 1).toInt();
  if (modIdx < ModInfo::getNumMods()) {
    ModInfo::Ptr info = ModInfo::getByIndex(modIdx);
    std::vector<ModInfo::EFlag> flags = info->getFlags();
    size_t count = flags.size();
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

