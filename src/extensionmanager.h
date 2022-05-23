#ifndef EXTENSIONMANAGER_H
#define EXTENSIONMANAGER_H

#include <memory>

#include <boost/fusion/algorithm.hpp>
#include <boost/fusion/container.hpp>
#include <boost/fusion/include/at_key.hpp>

#include <extension.h>

#include "extensionwatcher.h"

class ExtensionManager
{
public:
  // load all extensions from the given directory
  //
  // trigger all currently registered watchers
  //
  void loadExtensions(std::filesystem::path const& directory);

  // retrieve the list of currently loaded extensions
  //
  const auto& extensions() const { return m_extensions; }

  // register an object implementing one or many watcher classes
  //
  template <class Watcher>
  void registerWatcher(Watcher& watcher)
  {
    using WatcherType = std::decay_t<Watcher>;
    boost::fusion::for_each(m_watchers, [&watcher](auto& watchers) {
      using KeyType =
          ExtensionWatcher<typename std::decay_t<decltype(watchers)>::first_type>;
      if constexpr (std::is_base_of_v<KeyType, WatcherType>) {
        watchers.second.push_back(&watcher);
      }
    });
  }

private:
  // trigger appropriate watchers for the given extension
  //
  void triggerWatchers(const MOBase::IExtension& extension) const;

private:
  std::vector<std::unique_ptr<const MOBase::IExtension>> m_extensions;

  using WatcherMap = boost::fusion::map<
      boost::fusion::pair<MOBase::ThemeExtension,
                          std::vector<ExtensionWatcher<MOBase::ThemeExtension>*>>,
      boost::fusion::pair<MOBase::TranslationExtension,
                          std::vector<ExtensionWatcher<MOBase::TranslationExtension>*>>,
      boost::fusion::pair<MOBase::PluginExtension,
                          std::vector<ExtensionWatcher<MOBase::PluginExtension>*>>,
      boost::fusion::pair<MOBase::GameExtension,
                          std::vector<ExtensionWatcher<MOBase::GameExtension>*>>>;

  WatcherMap m_watchers;
};

#endif
