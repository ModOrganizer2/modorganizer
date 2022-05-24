#ifndef MO2_GAME_FEATURES_H
#define MO2_GAME_FEATURES_H

#include <any>
#include <memory>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include <QObject>

#include "game_feature.h"
#include "moddatachecker.h"
#include "moddatacontent.h"

namespace MOBase
{
class IPlugin;
class IPluginGame;
}  // namespace MOBase

class OrganizerCore;
class PluginManager;

/**
 * Class managing game features, either registered or from the game plugin.
 */
class GameFeatures : public QObject
{
  Q_OBJECT

public:
  /**
   *
   */
  GameFeatures(OrganizerCore* core, PluginManager* plugins);

  ~GameFeatures();

  // register game features
  //
  bool registerGameFeature(MOBase::IPlugin* plugin, QStringList const& games,
                           std::shared_ptr<MOBase::GameFeature> feature, int priority);

  // unregister game features
  //
  bool unregisterGameFeature(std::shared_ptr<MOBase::GameFeature> feature);
  int unregisterGameFeatures(MOBase::IPlugin* plugin, std::type_info const& info);

  // retrieve a game feature
  //
  template <class T>
  std::shared_ptr<T> gameFeature() const
  {
    return std::dynamic_pointer_cast<T>(gameFeature(typeid(T)));
  }

signals:
  void modDataCheckerUpdated(const MOBase::ModDataChecker* check);
  void modDataContentUpdated(const MOBase::ModDataContent* check);

private:
  friend class GameFeaturesProxy;

  class GameFeatureWithData
  {
    // feature
    std::shared_ptr<MOBase::GameFeature> m_feature;

    // plugin that registered the feature
    MOBase::IPlugin* m_plugin;

    // games this plugin applies to - empty list indicates all games
    QStringList m_games;

    // priority of the plugin
    int m_priority;

  public:
    GameFeatureWithData(std::shared_ptr<MOBase::GameFeature> feature,
                        MOBase::IPlugin* plugin, QStringList games, int priority)
        : m_feature(feature), m_plugin(plugin), m_games(games), m_priority(priority)
    {}

    const auto& feature() const { return m_feature; }
    auto* const plugin() const { return m_plugin; }
    const auto& games() const { return m_games; }
    auto priority() const { return m_priority; }
  };

  // retrieve a game feature from info
  //
  std::shared_ptr<MOBase::GameFeature> gameFeature(std::type_info const& index) const;

  // update current features by filtering
  //
  void updateCurrentFeatures(std::type_index const& index);
  void updateCurrentFeatures();

  // implementation details
  class CombinedModDataChecker;
  class CombinedModDataContent;

  CombinedModDataChecker& modDataChecker() const;
  CombinedModDataContent& modDataContent() const;

  PluginManager& m_plugins;

  std::unordered_map<std::type_index, std::vector<GameFeatureWithData>> m_allFeatures;
  std::unordered_map<std::type_index, std::vector<std::shared_ptr<MOBase::GameFeature>>>
      m_currentFeatures;

  std::shared_ptr<CombinedModDataChecker> m_modDataChecker;
  std::shared_ptr<CombinedModDataContent> m_modDataContent;
};

#endif
