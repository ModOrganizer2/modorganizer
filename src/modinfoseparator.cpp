#include "modinfoseparator.h"

bool ModInfoSeparator::setName(const QString& name)
{
  return ModInfoRegular::setName(name);
}

std::vector<ModInfo::EFlag> ModInfoSeparator::getFlags() const
{
  {
    auto result = ModInfoRegular::getFlags();
    result.insert(result.begin(), FLAG_SEPARATOR);
    return result;
  }
}

int ModInfoSeparator::getHighlight() const
{
  return HIGHLIGHT_CENTER;
}

QString ModInfoSeparator::getDescription() const
{
  return tr("This is a Separator");
}

QString ModInfoSeparator::name() const
{
  return ModInfoRegular::name();
}


ModInfoSeparator::ModInfoSeparator(PluginContainer *pluginContainer, const MOBase::IPluginGame *game, const QDir &path, MOShared::DirectoryEntry **directoryStructure)
  : ModInfoRegular(pluginContainer, game, path, directoryStructure)
{
}
