#include "modflagicondelegate.h"
#include "modlist.h"
#include "modlistview.h"
#include <QList>
#include <log.h>

using namespace MOBase;

ModFlagIconDelegate::ModFlagIconDelegate(ModListView* view, int column, int compactSize)
    : IconDelegate(view, column, compactSize), m_view(view)
{}

QList<QString> ModFlagIconDelegate::getIconsForFlags(std::vector<ModInfo::EFlag> flags,
                                                     bool compact)
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

QList<QString> ModFlagIconDelegate::getIcons(const QModelIndex& index) const
{
  QVariant modid = index.data(ModList::IndexRole);

  if (modid.isValid()) {
    bool compact;
    auto flags = m_view->modFlags(index, &compact);
    return getIconsForFlags(flags, compact || this->compact());
  }

  return {};
}

QString ModFlagIconDelegate::getFlagIcon(ModInfo::EFlag flag)
{
  switch (flag) {
  case ModInfo::FLAG_BACKUP:
    return QStringLiteral(":/MO/gui/emblem_backup");
  case ModInfo::FLAG_INVALID:
    return QStringLiteral(":/MO/gui/problem");
  case ModInfo::FLAG_NOTENDORSED:
    return QStringLiteral(":/MO/gui/emblem_notendorsed");
  case ModInfo::FLAG_NOTES:
    return QStringLiteral(":/MO/gui/emblem_notes");
  case ModInfo::FLAG_HIDDEN_FILES:
    return QStringLiteral(":/MO/gui/emblem_hidden_files");
  case ModInfo::FLAG_ALTERNATE_GAME:
    return QStringLiteral(":/MO/gui/alternate_game");
  case ModInfo::FLAG_FOREIGN:
    return QString();
  case ModInfo::FLAG_SEPARATOR:
    return QString();
  case ModInfo::FLAG_OVERWRITE:
    return QString();
  case ModInfo::FLAG_PLUGIN_SELECTED:
    return QString();
  case ModInfo::FLAG_TRACKED:
    return QStringLiteral(":/MO/gui/tracked");
  default:
    log::warn("ModInfo flag {} has no defined icon", flag);
    return QString();
  }
}

size_t ModFlagIconDelegate::getNumIcons(const QModelIndex& index) const
{
  unsigned int modIdx = index.data(ModList::IndexRole).toInt();
  if (modIdx < ModInfo::getNumMods()) {
    ModInfo::Ptr info                 = ModInfo::getByIndex(modIdx);
    std::vector<ModInfo::EFlag> flags = info->getFlags();
    return flags.size();
  } else {
    return 0;
  }
}

QSize ModFlagIconDelegate::sizeHint(const QStyleOptionViewItem& option,
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
