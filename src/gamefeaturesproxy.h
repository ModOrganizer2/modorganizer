#ifndef GAMEFEATURESPROXY_H
#define GAMEFEATURESPROXY_H

#include "igamefeatures.h"

class GameFeatures;
class OrganizerProxy;

class GameFeaturesProxy : public MOBase::IGameFeatures
{
public:
  GameFeaturesProxy(OrganizerProxy* coreProxy, GameFeatures& gameFeatures);

  bool registerFeature(QStringList const& games,
                       std::shared_ptr<MOBase::GameFeature> feature, int priority,
                       bool replace) override;
  bool registerFeature(MOBase::IPluginGame* game,
                       std::shared_ptr<MOBase::GameFeature> feature, int priority,
                       bool replace) override;
  bool registerFeature(std::shared_ptr<MOBase::GameFeature> feature, int priority,
                       bool replace) override;
  bool unregisterFeature(std::shared_ptr<MOBase::GameFeature> feature) override;

protected:
  std::shared_ptr<MOBase::GameFeature>
  gameFeatureImpl(std::type_info const& info) const override;
  int unregisterFeaturesImpl(std::type_info const& info) override;

private:
  GameFeatures& m_Features;
  OrganizerProxy& m_CoreProxy;
};

#endif
