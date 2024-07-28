#include "gamefeaturesproxy.h"

#include "game_features.h"
#include "organizerproxy.h"

GameFeaturesProxy::GameFeaturesProxy(OrganizerProxy* coreProxy,
                                     GameFeatures& gameFeatures)
    : m_CoreProxy(*coreProxy), m_Features(gameFeatures)
{}

bool GameFeaturesProxy::registerFeature(QStringList const& games,
                                        std::shared_ptr<MOBase::GameFeature> feature,
                                        int priority, bool replace)
{
  if (replace) {
    m_Features.unregisterGameFeatures(m_CoreProxy.plugin(), feature->typeInfo());
  }
  return m_Features.registerGameFeature(m_CoreProxy.plugin(), games, feature, priority);
}

bool GameFeaturesProxy::registerFeature(MOBase::IPluginGame* game,
                                        std::shared_ptr<MOBase::GameFeature> feature,
                                        int priority, bool replace)
{
  return registerFeature({game->gameName()}, feature, priority, replace);
}

bool GameFeaturesProxy::registerFeature(std::shared_ptr<MOBase::GameFeature> feature,
                                        int priority, bool replace)
{
  return registerFeature(QStringList(), feature, priority, replace);
}

bool GameFeaturesProxy::unregisterFeature(std::shared_ptr<MOBase::GameFeature> feature)
{
  return m_Features.unregisterGameFeature(feature);
}

std::shared_ptr<MOBase::GameFeature>
GameFeaturesProxy::gameFeatureImpl(std::type_info const& info) const
{
  return m_Features.gameFeature(info);
}

int GameFeaturesProxy::unregisterFeaturesImpl(std::type_info const& info)
{
  return m_Features.unregisterGameFeatures(m_CoreProxy.plugin(), info);
}
