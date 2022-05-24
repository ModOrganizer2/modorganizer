#include "game_features.h"

#include <algorithm>

#include <QTimer>

#include "iplugingame.h"
#include "log.h"

#include "bsainvalidation.h"
#include "dataarchives.h"
#include "gameplugins.h"
#include "localsavegames.h"
#include "organizercore.h"
#include "pluginmanager.h"
#include "savegameinfo.h"
#include "scriptextender.h"
#include "unmanagedmods.h"

using namespace MOBase;

// avoid updating features more than once every 100ms
constexpr std::chrono::milliseconds UPDATE_QUEUE_TIMER{100};

// specific type index for checker and content that behaves differently
const std::type_index ModDataCheckerIndex{typeid(ModDataChecker)};
const std::type_index ModDataContentIndex{typeid(ModDataContent)};

class GameFeatures::CombinedModDataChecker : public ModDataChecker
{
  std::vector<std::shared_ptr<ModDataChecker>> m_modDataCheckers;
  mutable std::shared_ptr<ModDataChecker> m_fixer = nullptr;

public:
  void setCheckers(std::vector<std::shared_ptr<ModDataChecker>> checkers)
  {
    m_modDataCheckers = std::move(checkers);
    m_fixer           = nullptr;
  }

  bool isValid() const { return !m_modDataCheckers.empty(); }

  CheckReturn
  dataLooksValid(std::shared_ptr<const MOBase::IFileTree> fileTree) const override
  {
    m_fixer = nullptr;

    // go through the available mod-data checker, if any returns valid, we also
    // return valid, otherwise, return the first one that is fixable
    for (const auto& modDataChecker : m_modDataCheckers) {
      auto check = modDataChecker->dataLooksValid(fileTree);

      switch (check) {
      case CheckReturn::FIXABLE:
        // only update fixer if there is not one with higher priority
        if (!m_fixer) {
          m_fixer = modDataChecker;
        }
        break;
      case CheckReturn::VALID:
        // clear fixer if one were found before and return VALID, not mandatory
        // but cleaner
        m_fixer = nullptr;
        return CheckReturn::VALID;
      case CheckReturn::INVALID:
        break;
      }
    }

    return m_fixer ? CheckReturn::FIXABLE : CheckReturn::INVALID;
  }

  std::shared_ptr<MOBase::IFileTree>
  fix(std::shared_ptr<MOBase::IFileTree> fileTree) const override
  {
    if (m_fixer) {
      return m_fixer->fix(fileTree);
    }

    return nullptr;
  }
};
class GameFeatures::CombinedModDataContent : public ModDataContent
{
  // store the ModDataContent and the offset to add to the content
  std::vector<std::pair<std::shared_ptr<ModDataContent>, int>> m_modDataContents;
  std::vector<Content> m_allContents;

public:
  bool isValid() const { return !m_modDataContents.empty(); }

  void setContents(std::vector<std::shared_ptr<ModDataContent>> modDataContents)
  {
    m_modDataContents.clear();
    m_modDataContents.reserve(modDataContents.size());

    m_allContents.clear();

    // update all contents and offsets
    std::size_t offset = 0;
    for (auto& modDataContent : modDataContents) {

      m_modDataContents.emplace_back(modDataContent, static_cast<int>(offset));

      // add to the list of contents
      auto contents = modDataContent->getAllContents();
      m_allContents.insert(m_allContents.end(),
                           std::make_move_iterator(contents.begin()),
                           std::make_move_iterator(contents.end()));

      // increase offset for next mod data content
      offset += contents.size();
    }
  }

  std::vector<Content> getAllContents() const override { return m_allContents; }

  std::vector<int>
  getContentsFor(std::shared_ptr<const MOBase::IFileTree> fileTree) const
  {
    std::vector<int> contentsFor;
    for (auto& modDataContent : m_modDataContents) {
      auto contentsForFrom = modDataContent.first->getContentsFor(fileTree);
      std::transform(contentsForFrom.begin(), contentsForFrom.end(),
                     std::back_inserter(contentsFor),
                     [offset = modDataContent.second](auto content) {
                       return content + offset;
                     });
    }

    return contentsFor;
  }
};

GameFeatures::GameFeatures(OrganizerCore* core, PluginManager* plugins)
    : m_plugins(*plugins), m_modDataChecker(std::make_unique<CombinedModDataChecker>()),
      m_modDataContent(std::make_unique<CombinedModDataContent>())
{
  // can be nullptr since the plugin container can be initialized with a Core (e.g.,
  // on first MO2 start)
  if (core) {
    connect(core, &OrganizerCore::managedGameChanged, [this] {
      updateCurrentFeatures();
    });
  }

  auto updateFeatures = [this] {
    QTimer::singleShot(UPDATE_QUEUE_TIMER, [this] {
      updateCurrentFeatures();
    });
  };

  connect(plugins, &PluginManager::pluginEnabled, updateFeatures);
  connect(plugins, &PluginManager::pluginDisabled, updateFeatures);
  connect(plugins, &PluginManager::pluginRegistered, updateFeatures);
  connect(plugins, &PluginManager::pluginUnregistered,
          [this, updateFeatures](MOBase::IPlugin* plugin) {
            // remove features from the current plugin
            for (auto& [_, features] : m_allFeatures) {
              features.erase(std::remove_if(features.begin(), features.end(),
                                            [plugin](const auto& feature) {
                                              return feature.plugin() == plugin;
                                            }),
                             features.end());
            }

            // update current features
            updateFeatures();
          });
}

GameFeatures::~GameFeatures() {}

GameFeatures::CombinedModDataChecker& GameFeatures::modDataChecker() const
{
  return dynamic_cast<CombinedModDataChecker&>(*m_modDataChecker);
}
GameFeatures::CombinedModDataContent& GameFeatures::modDataContent() const
{
  return dynamic_cast<CombinedModDataContent&>(*m_modDataContent);
}

void GameFeatures::updateCurrentFeatures(std::type_index const& index)
{
  auto& features = m_allFeatures[index];

  m_currentFeatures[index].clear();

  // this can occur when starting MO2, just wait for the next update
  if (!m_plugins.managedGame()) {
    return;
  }

  for (const auto& dataFeature : features) {

    // registering plugin is disabled
    if (!m_plugins.isEnabled(dataFeature.plugin())) {
      continue;
    }

    // games does not match
    if (!dataFeature.games().isEmpty() &&
        !dataFeature.games().contains(m_plugins.managedGame()->gameName())) {
      continue;
    }

    m_currentFeatures[index].push_back(dataFeature.feature());
  }

  // update mod data checker
  if (index == ModDataCheckerIndex) {
    auto& currentCheckers = m_currentFeatures[ModDataCheckerIndex];
    std::vector<std::shared_ptr<ModDataChecker>> checkers;
    checkers.reserve(currentCheckers.size());
    std::transform(currentCheckers.begin(), currentCheckers.end(),
                   std::back_inserter(checkers), [](auto const& checker) {
                     return std::dynamic_pointer_cast<ModDataChecker>(checker);
                   });
    modDataChecker().setCheckers(std::move(checkers));
    emit modDataCheckerUpdated(gameFeature<ModDataChecker>().get());
  }

  // update mod data content
  if (index == ModDataContentIndex) {
    auto& currentContents = m_currentFeatures[ModDataContentIndex];
    std::vector<std::shared_ptr<ModDataContent>> contents;
    contents.reserve(currentContents.size());
    std::transform(currentContents.begin(), currentContents.end(),
                   std::back_inserter(contents), [](auto const& checker) {
                     return std::dynamic_pointer_cast<ModDataContent>(checker);
                   });
    modDataContent().setContents(std::move(contents));
    emit modDataContentUpdated(gameFeature<ModDataContent>().get());
  }
}

void GameFeatures::updateCurrentFeatures()
{
  // TODO: this completely refilters everything currently, which should be ok since
  // this should only be triggered by function that are uncommon (enabling/disabling
  // plugin or changing the game), it should be possible to filter more finely by
  // maintaining more information (e.g., only removing features from current when a
  // plugin is disabled or unregistered)
  //

  for (auto& [info, _] : m_allFeatures) {
    updateCurrentFeatures(info);
  }
}

bool GameFeatures::registerGameFeature(MOBase::IPlugin* plugin,
                                       QStringList const& games,
                                       std::shared_ptr<MOBase::GameFeature> feature,
                                       int priority)
{
  auto& features = m_allFeatures[feature->typeInfo()];

  if (std::find_if(features.begin(), features.end(), [&feature](const auto& data) {
        return data.feature() == feature;
      }) != features.end()) {
    log::error("cannot register feature multiple time");
    return false;
  }

  std::decay_t<decltype(features)>::iterator it;
  if (dynamic_cast<IPluginGame*>(plugin)) {
    it = features.end();
  } else {
    it = std::lower_bound(features.begin(), features.end(), priority,
                          [](const auto& feature, int priority) {
                            return feature.priority() > priority;
                          });
  }

  features.emplace(it, feature, plugin, games, std::numeric_limits<int>::min());

  // TODO: only update if relevant
  updateCurrentFeatures(feature->typeInfo());

  return true;
}

// unregister game features
//
bool GameFeatures::unregisterGameFeature(std::shared_ptr<MOBase::GameFeature> feature)
{
  bool removed = false;
  for (auto& [_, features] : m_allFeatures) {
    auto it =
        std::find_if(features.begin(), features.end(), [&feature](const auto& data) {
          return data.feature() == feature;
        });

    // the feature can only exist for one kind of features and cannot be duplicated
    if (it != features.end()) {
      features.erase(it);
      removed = true;
      break;
    }
  }

  if (removed) {
    updateCurrentFeatures();
  }

  return removed;
}

int GameFeatures::unregisterGameFeatures(MOBase::IPlugin* plugin,
                                         std::type_info const& info)
{
  auto& features = m_allFeatures[info];

  const auto initialSize = features.size();

  features.erase(std::remove_if(features.begin(), features.end(),
                                [plugin](const auto& feature) {
                                  return feature.plugin() == plugin;
                                }),
                 features.end());

  const int removed = features.size() - initialSize;

  if (removed) {
    updateCurrentFeatures();
  }

  return removed;
}

std::shared_ptr<GameFeature> GameFeatures::gameFeature(std::type_info const& info) const
{
  if (info == ModDataCheckerIndex) {
    return modDataChecker().isValid() ? m_modDataChecker : nullptr;
  }

  if (info == ModDataContentIndex) {
    return modDataContent().isValid() ? m_modDataContent : nullptr;
  }

  auto it = m_currentFeatures.find(info);
  if (it == m_currentFeatures.end() || it->second.empty()) {
    return nullptr;
  }

  return it->second.front();
}
