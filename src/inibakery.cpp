#include "inibakery.h"

#include <bsainvalidation.h>
#include <localsavegames.h>

#include "organizercore.h"

using namespace MOBase;

IniBakery::IniBakery(OrganizerCore& core) : m_core{core}
{
  m_core.onAboutToRun([this](auto&&...) {
    return prepareIni();
  });
}

bool IniBakery::prepareIni() const
{
  const auto& features = m_core.pluginManager().gameFeatures();

  if (auto savegames = features.gameFeature<LocalSavegames>()) {
    savegames->prepareProfile(m_core.currentProfile());
  }

  if (auto invalidation = features.gameFeature<BSAInvalidation>()) {
    invalidation->prepareProfile(m_core.currentProfile());
  }

  return true;
}

MappingType IniBakery::mappings() const
{
  MappingType result;

  const auto iniFileNames = m_core.managedGame()->iniFiles();
  const IPluginGame* game = m_core.managedGame();

  IProfile* profile = m_core.currentProfile();

  if (profile->localSettingsEnabled()) {
    for (const QString& iniFile : iniFileNames) {
      result.push_back({m_core.profilePath() + "/" + QFileInfo(iniFile).fileName(),
                        game->documentsDirectory().absoluteFilePath(iniFile), false,
                        false});
    }
  }

  return result;
}
