#include "extensionmanager.h"

#include <log.h>

using namespace MOBase;
namespace fs = std::filesystem;

void ExtensionManager::loadExtensions(fs::path const& directory)
{
  for (const auto& entry : fs::directory_iterator{directory}) {
    if (entry.is_directory()) {
      auto extension = ExtensionFactory::loadExtension(entry.path());

      if (extension) {
        // check if we have a duplicate identifier
        const auto it = std::find_if(
            m_extensions.begin(), m_extensions.end(), [&extension](const auto& value) {
              return value->metadata().identifier().compare(
                         extension->metadata().identifier(), Qt::CaseInsensitive) == 0;
            });
        if (it != m_extensions.end()) {
          log::error("an extension '{}' already exists",
                     extension->metadata().identifier());
          continue;
        }

        log::debug("extension correctly loaded from '{}': {}, {}",
                   entry.path().native(), extension->metadata().identifier(),
                   extension->metadata().type());

        triggerWatchers(*extension);
        m_extensions.push_back(std::move(extension));
      }
    }
  }
}

void ExtensionManager::triggerWatchers(const MOBase::IExtension& extension) const
{
  boost::fusion::for_each(m_watchers, [&extension](auto& watchers) {
    using KeyType = typename std::decay_t<decltype(watchers)>::first_type;
    if (auto* p = dynamic_cast<const KeyType*>(&extension)) {
      for (auto& watcher : watchers.second) {
        watcher->extensionLoaded(*p);
      }
    }
  });
}

const IExtension* ExtensionManager::extension(QString const& identifier) const
{
  // TODO: use a map for faster lookup
  auto it = std::find_if(m_extensions.begin(), m_extensions.end(),
                         [&identifier](const auto& ext) {
                           return identifier.compare(ext->metadata().identifier(),
                                                     Qt::CaseInsensitive) == 0;
                         });

  return it == m_extensions.end() ? nullptr : it->get();
}

bool ExtensionManager::isEnabled(MOBase::IExtension const& extension) const
{
  // TODO
  return true;
}

bool ExtensionManager::isEnabled(QString const& identifier) const
{
  const auto* e = extension(identifier);
  return e ? isEnabled(*e) : false;
}
