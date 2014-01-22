#include "modflagicondelegate.h"
#include <QList>


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
    return info->getFlags().size();
  } else {
    return 0;
  }
}

