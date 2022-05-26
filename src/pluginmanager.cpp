#include "pluginmanager.h"

#include <QLibrary>
#include <QPluginLoader>

#include <boost/fusion/algorithm/iteration/for_each.hpp>
#include <boost/fusion/include/at_key.hpp>
#include <boost/fusion/include/for_each.hpp>
#include <boost/fusion/sequence/intrinsic/at_key.hpp>

#include "extensionmanager.h"
#include "iuserinterface.h"
#include "organizercore.h"
#include "organizerproxy.h"
#include "previewgenerator.h"
#include "proxyqt.h"

using namespace MOBase;
namespace bf = boost::fusion;

// localized names

template <class T>
struct PluginTypeName;

template <>
struct PluginTypeName<IPlugin>
{
  static QString value() { return PluginManager::tr("Plugin"); }
};
template <>
struct PluginTypeName<IPluginDiagnose>
{
  static QString value() { return PluginManager::tr("Diagnose"); }
};
template <>
struct PluginTypeName<IPluginGame>
{
  static QString value() { return PluginManager::tr("Game"); }
};
template <>
struct PluginTypeName<IPluginInstaller>
{
  static QString value() { return PluginManager::tr("Installer"); }
};
template <>
struct PluginTypeName<IPluginModPage>
{
  static QString value() { return PluginManager::tr("Mod Page"); }
};
template <>
struct PluginTypeName<IPluginPreview>
{
  static QString value() { return PluginManager::tr("Preview"); }
};
template <>
struct PluginTypeName<IPluginTool>
{
  static QString value() { return PluginManager::tr("Tool"); }
};
template <>
struct PluginTypeName<IPluginFileMapper>
{
  static QString value() { return PluginManager::tr("File Mapper"); }
};

// PluginDetails

PluginDetails::PluginDetails(PluginManager* manager, PluginExtension const& extension,
                             IPlugin* plugin, OrganizerProxy* proxy)
    : m_manager(manager), m_extension(&extension), m_plugin(plugin), m_organizer(proxy)
{}

void PluginDetails::fetchRequirements()
{
  m_requirements = m_plugin->requirements();
}

std::vector<IPluginRequirement::Problem> PluginDetails::problems() const
{
  std::vector<IPluginRequirement::Problem> result;
  for (auto& requirement : m_requirements) {
    if (auto p = requirement->check(m_organizer)) {
      result.push_back(*p);
    }
  }
  return result;
}

bool PluginDetails::canEnable() const
{
  return problems().empty();
}

bool PluginDetails::hasRequirements() const
{
  return !m_requirements.empty();
}

QStringList PluginDetails::requiredGames() const
{
  // We look for a "GameDependencyRequirement" - There can be only one since otherwise
  // it'd mean that the plugin requires two games at once.
  for (auto& requirement : m_requirements) {
    if (auto* gdep =
            dynamic_cast<const GameDependencyRequirement*>(requirement.get())) {
      return gdep->gameNames();
    }
  }

  return {};
}

// PluginManager

QStringList PluginManager::pluginInterfaces()
{
  // Find all the names:
  QStringList names;
  boost::mp11::mp_for_each<PluginTypeOrder>([&names](const auto* p) {
    using plugin_type = std::decay_t<decltype(*p)>;
    auto name         = PluginTypeName<plugin_type>::value();
    if (!name.isEmpty()) {
      names.append(name);
    }
  });

  return names;
}

PluginManager::PluginManager(ExtensionManager const& manager, OrganizerCore* core)
    : m_extensions{manager}, m_core{core}
{
  m_loaders = makeLoaders();
}

QStringList PluginManager::implementedInterfaces(IPlugin* plugin) const
{
  // we need a QObject to be able to qobject_cast<> to the plugin types
  QObject* oPlugin = as_qobject(plugin);

  if (!oPlugin) {
    return {};
  }

  return implementedInterfaces(oPlugin);
}

QStringList PluginManager::implementedInterfaces(QObject* oPlugin) const
{
  // Find all the names:
  QStringList names;
  boost::mp11::mp_for_each<PluginTypeOrder>([oPlugin, &names](const auto* p) {
    using plugin_type = std::decay_t<decltype(*p)>;
    if (qobject_cast<plugin_type*>(oPlugin)) {
      auto name = PluginTypeName<plugin_type>::value();
      if (!name.isEmpty()) {
        names.append(name);
      }
    }
  });

  // If the plugin implements at least one interface other than IPlugin, remove IPlugin:
  if (names.size() > 1) {
    names.removeAll(PluginTypeName<IPlugin>::value());
  }

  return names;
}

QString PluginManager::topImplementedInterface(IPlugin* plugin) const
{
  auto interfaces = implementedInterfaces(plugin);
  return interfaces.isEmpty() ? "" : interfaces[0];
}

bool PluginManager::isBetterInterface(QObject* lhs, QObject* rhs) const
{
  int count = 0, lhsIdx = -1, rhsIdx = -1;
  boost::mp11::mp_for_each<PluginTypeOrder>([&](const auto* p) {
    using plugin_type = std::decay_t<decltype(*p)>;
    if (lhsIdx < 0 && qobject_cast<plugin_type*>(lhs)) {
      lhsIdx = count;
    }
    if (rhsIdx < 0 && qobject_cast<plugin_type*>(rhs)) {
      rhsIdx = count;
    }
    ++count;
  });
  return lhsIdx < rhsIdx;
}

MOBase::IPluginGame* PluginManager::game(const QString& name) const
{
  auto iter = m_supportedGames.find(name);
  if (iter != m_supportedGames.end()) {
    return iter->second;
  } else {
    return nullptr;
  }
}

MOBase::IPluginGame* PluginManager::managedGame() const
{
  // TODO: this const_cast is safe but ugly
  //
  // most methods require a IPlugin*, so returning a const-version if painful, this
  // should be fixed by making methods accept a const IPlugin* instead, but there are a
  // few tricks with qobject_cast and const
  //
  return const_cast<IPluginGame*>(m_core->managedGame());
}

MOBase::IPlugin* PluginManager::plugin(QString const& pluginName) const
{
  auto& map = bf::at_key<QString>(m_accessPlugins);
  auto it   = map.find(pluginName);
  if (it == std::end(map)) {
    return nullptr;
  }
  return it->second;
}

MOBase::IPlugin* PluginManager::plugin(MOBase::IPluginDiagnose* diagnose) const
{
  auto& map = bf::at_key<IPluginDiagnose>(m_accessPlugins);
  auto it   = map.find(diagnose);
  if (it == std::end(map)) {
    return nullptr;
  }
  return it->second;
}

MOBase::IPlugin* PluginManager::plugin(MOBase::IPluginFileMapper* mapper) const
{
  auto& map = bf::at_key<IPluginFileMapper>(m_accessPlugins);
  auto it   = map.find(mapper);
  if (it == std::end(map)) {
    return nullptr;
  }
  return it->second;
}

bool PluginManager::isEnabled(MOBase::IPlugin* plugin) const
{
  // check if it is a game plugin
  if (implementInterface<IPluginGame>(plugin)) {
    return plugin == m_core->managedGame();
  }

  // check the master of the group
  const auto& d = details(plugin);
  if (d.master() && d.master() != plugin) {
    return isEnabled(d.master());
  }

  return m_extensions.isEnabled(details(plugin).extension());
}

bool PluginManager::isEnabled(QString const& pluginName) const
{
  IPlugin* p = plugin(pluginName);
  return p ? isEnabled(p) : false;
}

bool PluginManager::isEnabled(MOBase::IPluginDiagnose* diagnose) const
{
  IPlugin* p = plugin(diagnose);
  return p ? isEnabled(p) : false;
}

bool PluginManager::isEnabled(MOBase::IPluginFileMapper* mapper) const
{
  IPlugin* p = plugin(mapper);
  return p ? isEnabled(p) : false;
}

QObject* PluginManager::as_qobject(MOBase::IPlugin* plugin) const
{
  // Find the correspond QObject - Can this be done safely with a cast?
  auto& objects = bf::at_key<QObject>(m_plugins);
  auto it =
      std::find_if(std::begin(objects), std::end(objects), [plugin](QObject* obj) {
        return qobject_cast<IPlugin*>(obj) == plugin;
      });

  if (it == std::end(objects)) {
    return nullptr;
  }

  return *it;
}

bool PluginManager::initPlugin(PluginExtension const& extension, IPlugin* plugin,
                               bool skipInit)
{
  // when MO has no instance loaded, init() is not called on plugins, except
  // for proxy plugins, where init() is called with a null IOrganizer
  //
  // after proxies are initialized, instantiate() is called for all the plugins
  // they've discovered, but as for regular plugins, init() won't be
  // called on them if m_OrganizerCore is null
  //

  if (plugin == nullptr) {
    return false;
  }

  OrganizerProxy* proxy = nullptr;
  if (m_core) {
    proxy = new OrganizerProxy(m_core, this, plugin);
    proxy->setParent(as_qobject(plugin));
  }

  auto [it, bl] =
      m_details.emplace(plugin, PluginDetails(this, extension, plugin, proxy));

  if (!m_core || skipInit) {
    return true;
  }

  if (!plugin->init(proxy)) {
    log::warn("plugin failed to initialize");
    return false;
  }

  // Update requirements:
  it->second.fetchRequirements();

  return true;
}

IPlugin* PluginManager::registerPlugin(const PluginExtension& extension,
                                       QObject* plugin,
                                       QList<QObject*> const& pluginGroup)
{
  // generic treatment for all plugins
  IPlugin* pluginObj = qobject_cast<IPlugin*>(plugin);
  if (pluginObj == nullptr) {
    log::debug("PluginContainer::registerPlugin() called with a non IPlugin QObject.");
    return nullptr;
  }

  // we check if there is already a plugin with this name, if there is one, it must be
  // from the same group
  bool skipInit  = false;
  auto& mapNames = bf::at_key<QString>(m_accessPlugins);
  if (mapNames.contains(pluginObj->name())) {

    IPlugin* other = mapNames[pluginObj->name()];

    // if both plugins are from the same group that's ok, we just need to skip
    // initialization
    if (pluginGroup.contains(as_qobject(other))) {

      // plugin has already been initialized
      skipInit = true;

      if (isBetterInterface(plugin, as_qobject(other))) {
        log::debug(
            "replacing plugin '{}' with interfaces [{}] by one with interfaces [{}]",
            pluginObj->name(), implementedInterfaces(other).join(", "),
            implementedInterfaces(plugin).join(", "));
        bf::at_key<QString>(m_accessPlugins)[pluginObj->name()] = pluginObj;
      }
    } else {
      log::warn("trying to register two plugins with the name '{}' (from {} and {}), "
                "the second one will not be registered",
                pluginObj->name(), details(other).extension().metadata().name(),
                extension.metadata().name());
      return nullptr;
    }
  } else {
    bf::at_key<QString>(m_accessPlugins)[pluginObj->name()] = pluginObj;
  }

  // storing the original QObject* is a bit of a hack as I couldn't figure out any
  // way to cast directly between IPlugin* and IPluginDiagnose*
  bf::at_key<QObject>(m_plugins).push_back(plugin);
  m_allPlugins.push_back(pluginObj);

  plugin->setParent(this);

  if (m_core) {
    m_core->settings().plugins().registerPlugin(pluginObj);
  }

  {  // diagnosis plugin
    IPluginDiagnose* diagnose = qobject_cast<IPluginDiagnose*>(plugin);
    if (diagnose != nullptr) {
      bf::at_key<IPluginDiagnose>(m_plugins).push_back(diagnose);
      bf::at_key<IPluginDiagnose>(m_accessPlugins)[diagnose] = pluginObj;
      diagnose->onInvalidated([&, diagnose]() {
        emit diagnosePluginInvalidated(diagnose);
      });
    }
  }

  {  // file mapper plugin
    IPluginFileMapper* mapper = qobject_cast<IPluginFileMapper*>(plugin);
    if (mapper != nullptr) {
      bf::at_key<IPluginFileMapper>(m_plugins).push_back(mapper);
      bf::at_key<IPluginFileMapper>(m_accessPlugins)[mapper] = pluginObj;
    }
  }

  {  // mod page plugin
    IPluginModPage* modPage = qobject_cast<IPluginModPage*>(plugin);
    if (initPlugin(extension, modPage, skipInit)) {
      bf::at_key<IPluginModPage>(m_plugins).push_back(modPage);
      emit pluginRegistered(modPage);
      return modPage;
    }
  }

  {  // game plugin
    IPluginGame* game = qobject_cast<IPluginGame*>(plugin);
    if (game) {
      game->detectGame();
      if (initPlugin(extension, game, skipInit)) {
        bf::at_key<IPluginGame>(m_plugins).push_back(game);
        registerGame(game);
        emit pluginRegistered(game);
        return game;
      }
    }
  }

  {  // tool plugins
    IPluginTool* tool = qobject_cast<IPluginTool*>(plugin);
    if (initPlugin(extension, tool, skipInit)) {
      bf::at_key<IPluginTool>(m_plugins).push_back(tool);
      emit pluginRegistered(tool);
      return tool;
    }
  }

  {  // installer plugins
    IPluginInstaller* installer = qobject_cast<IPluginInstaller*>(plugin);
    if (initPlugin(extension, installer, skipInit)) {
      bf::at_key<IPluginInstaller>(m_plugins).push_back(installer);
      if (m_core) {
        installer->setInstallationManager(m_core->installationManager());
      }
      emit pluginRegistered(installer);
      return installer;
    }
  }

  {  // preview plugins
    IPluginPreview* preview = qobject_cast<IPluginPreview*>(plugin);
    if (initPlugin(extension, preview, skipInit)) {
      bf::at_key<IPluginPreview>(m_plugins).push_back(preview);
      return preview;
    }
  }

  {  // dummy plugins
    // only initialize these, no processing otherwise
    IPlugin* dummy = qobject_cast<IPlugin*>(plugin);
    if (initPlugin(extension, dummy, skipInit)) {
      bf::at_key<IPlugin>(m_plugins).push_back(dummy);
      emit pluginRegistered(dummy);
      return dummy;
    }
  }

  return nullptr;
}

void PluginManager::loadPlugins()
{
  unloadPlugins();

  // TODO: order based on dependencies
  for (auto& extension : m_extensions.extensions()) {
    if (auto* pluginExtension = dynamic_cast<const PluginExtension*>(extension.get())) {
      loadPlugins(*pluginExtension);
    }
  }
}

bool PluginManager::loadPlugins(const MOBase::PluginExtension& extension)
{
  unloadPlugins(extension);

  // load plugins
  QList<QList<QObject*>> objects;
  for (auto& loader : m_loaders) {
    objects.append(loader->load(extension));
  }

  for (auto& objectGroup : objects) {

    // safety for min_element
    if (objectGroup.isEmpty()) {
      continue;
    }

    // find the best interface
    auto it         = std::min_element(std::begin(objectGroup), std::end(objectGroup),
                                       [&](auto const& lhs, auto const& rhs) {
                                 return isBetterInterface(lhs, rhs);
                               });
    IPlugin* master = qobject_cast<IPlugin*>(*it);

    // register plugins in the group
    for (auto* object : objectGroup) {
      IPlugin* plugin = registerPlugin(extension, object, objectGroup);

      if (plugin) {
        m_details.at(plugin).m_master = master;
      }
    }
  }

  // TODO: move this elsewhere, e.g., in core
  if (m_core) {
    bf::at_key<IPluginDiagnose>(m_plugins).push_back(m_core);
    m_core->connectPlugins(this);
  }
  return true;
}

void PluginManager::unloadPlugin(MOBase::IPlugin* plugin, QObject* object)
{
  if (auto* game = qobject_cast<IPluginGame*>(object)) {

    if (game == managedGame()) {
      throw Exception("cannot unload the plugin for the currently managed game");
    }

    unregisterGame(game);
  }

  // we need to remove from the m_plugins maps BEFORE unloading from the proxy
  // otherwise the qobject_cast to check the plugin type will not work
  bf::for_each(m_plugins, [object](auto& t) {
    using type = typename std::decay_t<decltype(t.second)>::value_type;

    // we do not want to remove from QObject since we are iterating over them
    if constexpr (!std::is_same<type, QObject*>{}) {
      auto itp =
          std::find(t.second.begin(), t.second.end(), qobject_cast<type>(object));
      if (itp != t.second.end()) {
        t.second.erase(itp);
      }
    }
  });

  emit pluginUnregistered(plugin);

  // remove from the members
  if (auto* diagnose = qobject_cast<IPluginDiagnose*>(object)) {
    bf::at_key<IPluginDiagnose>(m_accessPlugins).erase(diagnose);
  }
  if (auto* mapper = qobject_cast<IPluginFileMapper*>(object)) {
    bf::at_key<IPluginFileMapper>(m_accessPlugins).erase(mapper);
  }

  auto& mapNames = bf::at_key<QString>(m_accessPlugins);
  if (mapNames.contains(plugin->name())) {
    mapNames.erase(plugin->name());
  }

  m_core->settings().plugins().unregisterPlugin(plugin);

  // force disconnection of the signals from the proxies
  //
  // this is a safety operations since those signals should be disconnected when the
  // proxies are destroyed anyway
  //
  details(plugin).m_organizer->disconnectSignals();

  // do this at the end
  m_details.erase(plugin);
}

bool PluginManager::unloadPlugins(const MOBase::PluginExtension& extension)
{
  std::vector<QObject*> objectsToDelete;

  // first we clear the internal structures, disconnect signales, etc.
  {
    auto& objects = bf::at_key<QObject>(m_plugins);
    for (auto it = objects.begin(); it != objects.end();) {
      auto* plugin = qobject_cast<IPlugin*>(*it);
      if (&details(plugin).extension() == &extension) {
        unloadPlugin(plugin, *it);
        objectsToDelete.push_back(*it);
        it = objects.erase(it);
      } else {
        ++it;
      }
    }
  }

  // then we let the loader unload the plugin
  for (auto& loader : m_loaders) {
    loader->unload(extension);
  }

  // manual delete (for safety)
  for (auto* object : objectsToDelete) {
    object->deleteLater();
  }

  return true;
}

void PluginManager::unloadPlugins()
{
  if (m_core) {
    // this will clear several structures that can hold on to pointers to
    // plugins, as well as read the plugin blacklist from the ini file, which
    // is used in loadPlugins() below to skip plugins
    //
    // note that the first thing loadPlugins() does is call unloadPlugins(),
    // so this makes sure the blacklist is always available
    m_core->settings().plugins().clearPlugins();
  }

  bf::for_each(m_plugins, [](auto& t) {
    t.second.clear();
  });
  bf::for_each(m_accessPlugins, [](auto& t) {
    t.second.clear();
  });

  m_details.clear();
  m_supportedGames.clear();

  for (auto& loader : m_loaders) {
    loader->unloadAll();
  }
}

bool PluginManager::reloadPlugins(const MOBase::PluginExtension& extension)
{
  // load plugin already unload(), so no need to manually do it here
  return loadPlugins(extension);
}

void PluginManager::registerGame(MOBase::IPluginGame* game)
{
  m_supportedGames.insert({game->gameName(), game});
}

void PluginManager::unregisterGame(MOBase::IPluginGame* game)
{
  m_supportedGames.erase(game->gameName());
}

void PluginManager::startPlugins(IUserInterface* userInterface)
{
  m_userInterface = userInterface;
  startPluginsImpl(plugins<QObject>());
}

void PluginManager::startPluginsImpl(const std::vector<QObject*>& plugins) const
{
  if (m_userInterface) {
    for (auto* plugin : plugins) {
      if (auto* modPage = qobject_cast<IPluginModPage*>(plugin)) {
        modPage->setParentWidget(m_userInterface->mainWindow());
      }
      if (auto* tool = qobject_cast<IPluginTool*>(plugin)) {
        tool->setParentWidget(m_userInterface->mainWindow());
      }
      if (auto* installer = qobject_cast<IPluginInstaller*>(plugin)) {
        installer->setParentWidget(m_userInterface->mainWindow());
      }
    }
  }

  // Trigger initial callbacks, e.g. onUserInterfaceInitialized and onProfileChanged.
  if (m_core) {
    for (auto* object : plugins) {
      auto* plugin = qobject_cast<IPlugin*>(object);
      auto* oproxy = details(plugin).m_organizer;
      oproxy->connectSignals();
      oproxy->m_ProfileChanged(nullptr, m_core->currentProfile());

      if (m_userInterface) {
        oproxy->m_UserInterfaceInitialized(m_userInterface->mainWindow());
      }
    }
  }
}

const PreviewGenerator& PluginManager::previewGenerator() const
{
  return *m_previews;
}

PluginManager::PluginLoaderDeleter::PluginLoaderDeleter(QPluginLoader* qPluginLoader)
    : m_qPluginLoader(qPluginLoader)
{}

void PluginManager::PluginLoaderDeleter::operator()(MOBase::IPluginLoader* loader) const
{
  // if there is a QPluginLoader, the loader is responsible for unloading the plugin
  if (m_qPluginLoader) {
    m_qPluginLoader->unload();
    delete m_qPluginLoader;
  } else {
    delete loader;
  }
}

std::vector<PluginManager::PluginLoaderPtr> PluginManager::makeLoaders()
{
  std::vector<PluginLoaderPtr> loaders;

  // create the Qt loader
  loaders.push_back(PluginLoaderPtr(new ProxyQtLoader(), PluginLoaderDeleter{}));

  // load the python proxy
  {
    const QString proxyPath =
        QCoreApplication::applicationDirPath() + "/proxies/python";
    auto pluginLoader =
        std::make_unique<QPluginLoader>(proxyPath + "/python_proxy.dll", this);

    if (auto* object = pluginLoader->instance(); object) {
      auto loader = qobject_cast<MOBase::IPluginLoader*>(object);
      QString errorMessage;

      if (loader->initialize(errorMessage)) {
        loaders.push_back(
            PluginLoaderPtr(loader, PluginLoaderDeleter{pluginLoader.release()}));
      } else {
        log::error("failed to initialize proxy from '{}': {}", proxyPath, errorMessage);
      }
    }
  }

  return loaders;
}
