#include "plugincontainer.h"
#include "iuserinterface.h"
#include "organizercore.h"
#include "organizerproxy.h"
#include "report.h"
#include "shared/appconfig.h"
#include <QAction>
#include <QCoreApplication>
#include <QDirIterator>
#include <QMessageBox>
#include <QToolButton>
#include <boost/fusion/algorithm/iteration/for_each.hpp>
#include <boost/fusion/include/at_key.hpp>
#include <boost/fusion/include/for_each.hpp>
#include <boost/fusion/sequence/intrinsic/at_key.hpp>
#include <idownloadmanager.h>
#include <ipluginproxy.h>

using namespace MOBase;
using namespace MOShared;

namespace bf = boost::fusion;

// Welcome to the wonderful world of MO2 plugin management!
//
// We'll start by the C++ side.
//
// There are 9 types of MO2 plugins, two of which cannot be standalone: IPluginDiagnose
// and IPluginFileMapper. This means that you can have a class implementing IPluginGame,
// IPluginDiagnose and IPluginFileMapper. It is not possible for a class to implement
// two full plugin types (e.g. IPluginPreview and IPluginTool).
//
// Plugins are fetch as QObject initially and must be "qobject-casted" to the right
// type.
//
// Plugins are stored in the PluginContainer class in various C++ containers: there is a
// vector that stores all the plugin as QObject, multiple vectors that stores the plugin
// of each types, a map to find IPlugin object from their names or from IPluginDiagnose
// or IFileMapper (since these do not inherit IPlugin, they cannot be downcasted).
//
// Requirements for plugins are stored in m_Requirements:
// - IPluginGame cannot be enabled by user. A game plugin is considered enable only if
// it is
//   the one corresponding to the currently managed games.
// - If a plugin has a master plugin (IPlugin::master()), it cannot be enabled/disabled
// by users,
//   and will follow the enabled/disabled state of its parent.
// - Each plugin has an "enabled" setting stored in persistence.  If the setting does
// not exist,
//   the plugin's enabledByDefault is used instead.
// - A plugin is considered disabled if the setting is false.
// - If the setting is true, a plugin is considered disabled if one of its
//   requirements is not met.
// - Users cannot enable a plugin if one of its requirements is not met.
//
// Now let's move to the Proxy side... Or the as of now, the Python side.
//
// Proxied plugins are much more annoying because they can implement all interfaces, and
// are given to MO2 as separate plugins... A Python class implementing IPluginGame and
// IPluginDiagnose will be seen by MO2 as two separate QObject, and they will all have
// the same name.
//
// When a proxied plugin is registered, a few things must be taken care of:
// - There can only be one plugin mapped to a name in the PluginContainer class, so we
// keep the
//   plugin corresponding to the most relevant class (see PluginTypeOrder), e.g. if the
//   class inherits both IPluginGame and IPluginFileMapper, we map the name to the C++
//   QObject corresponding to the IPluginGame.
// - When a proxied plugin implements multiple interfaces, the IPlugin corresponding to
// the most
//   important interface is set as the parent (hidden) of the other IPlugin through
//   PluginRequirements. This way, the plugin are managed together (enabled/disabled
//   state). The "fake" children plugins will not be returned by
//   PluginRequirements::children().
// - Since each interface corresponds to a different QObject, we need to take care not
// to call
//   IPlugin::init() on each QObject, but only on the first one.
//
// All the proxied plugins are linked to the proxy plugin by PluginRequirements. If the
// proxy plugin is disabled, the proxied plugins are not even loaded so not visible in
// the plugin management tab.

template <class T>
struct PluginTypeName;

template <>
struct PluginTypeName<MOBase::IPlugin>
{
  static QString value() { return QT_TR_NOOP("Plugin"); }
};
template <>
struct PluginTypeName<MOBase::IPluginDiagnose>
{
  static QString value() { return QT_TR_NOOP("Diagnose"); }
};
template <>
struct PluginTypeName<MOBase::IPluginGame>
{
  static QString value() { return QT_TR_NOOP("Game"); }
};
template <>
struct PluginTypeName<MOBase::IPluginInstaller>
{
  static QString value() { return QT_TR_NOOP("Installer"); }
};
template <>
struct PluginTypeName<MOBase::IPluginModPage>
{
  static QString value() { return QT_TR_NOOP("Mod Page"); }
};
template <>
struct PluginTypeName<MOBase::IPluginPreview>
{
  static QString value() { return QT_TR_NOOP("Preview"); }
};
template <>
struct PluginTypeName<MOBase::IPluginTool>
{
  static QString value() { return QT_TR_NOOP("Tool"); }
};
template <>
struct PluginTypeName<MOBase::IPluginProxy>
{
  static QString value() { return QT_TR_NOOP("Proxy"); }
};
template <>
struct PluginTypeName<MOBase::IPluginFileMapper>
{
  static QString value() { return QT_TR_NOOP("File Mapper"); }
};

QStringList PluginContainer::pluginInterfaces()
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

// PluginRequirementProxy

const std::set<QString> PluginRequirements::s_CorePlugins{"INI Bakery"};

PluginRequirements::PluginRequirements(PluginContainer* pluginContainer,
                                       MOBase::IPlugin* plugin, OrganizerProxy* proxy,
                                       MOBase::IPluginProxy* pluginProxy)
    : m_PluginContainer(pluginContainer), m_Plugin(plugin), m_PluginProxy(pluginProxy),
      m_Master(nullptr), m_Organizer(proxy)
{
  // There are a lots of things we cannot set here (e.g. m_Master) because we do not
  // know the order plugins are loaded.
}

void PluginRequirements::fetchRequirements()
{
  m_Requirements = m_Plugin->requirements();
}

IPluginProxy* PluginRequirements::proxy() const
{
  return m_PluginProxy;
}

std::vector<IPlugin*> PluginRequirements::proxied() const
{
  std::vector<IPlugin*> children;
  if (dynamic_cast<IPluginProxy*>(m_Plugin)) {
    for (auto* obj : m_PluginContainer->plugins<QObject>()) {
      auto* plugin = qobject_cast<IPlugin*>(obj);
      if (plugin && m_PluginContainer->requirements(plugin).proxy() == m_Plugin) {
        children.push_back(plugin);
      }
    }
  }
  return children;
}

IPlugin* PluginRequirements::master() const
{
  // If we have a m_Master, it was forced and thus override the default master().
  if (m_Master) {
    return m_Master;
  }

  if (m_Plugin->master().isEmpty()) {
    return nullptr;
  }

  return m_PluginContainer->plugin(m_Plugin->master());
}

void PluginRequirements::setMaster(IPlugin* master)
{
  m_Master = master;
}

std::vector<IPlugin*> PluginRequirements::children() const
{
  std::vector<IPlugin*> children;
  for (auto* obj : m_PluginContainer->plugins<QObject>()) {
    auto* plugin = qobject_cast<IPlugin*>(obj);

    // Not checking master() but requirements().master() due to "hidden"
    // masters.
    // If the master has the same name as the plugin, this is a "hidden"
    // master, we do not add it here.
    if (plugin && m_PluginContainer->requirements(plugin).master() == m_Plugin &&
        plugin->name() != m_Plugin->name()) {
      children.push_back(plugin);
    }
  }
  return children;
}

std::vector<IPluginRequirement::Problem> PluginRequirements::problems() const
{
  std::vector<IPluginRequirement::Problem> result;
  for (auto& requirement : m_Requirements) {
    if (auto p = requirement->check(m_Organizer)) {
      result.push_back(*p);
    }
  }
  return result;
}

bool PluginRequirements::canEnable() const
{
  return problems().empty();
}

bool PluginRequirements::isCorePlugin() const
{
  // Let's consider game plugins as "core":
  if (m_PluginContainer->implementInterface<IPluginGame>(m_Plugin)) {
    return true;
  }

  return s_CorePlugins.contains(m_Plugin->name());
}

bool PluginRequirements::hasRequirements() const
{
  return !m_Requirements.empty();
}

QStringList PluginRequirements::requiredGames() const
{
  // We look for a "GameDependencyRequirement" - There can be only one since otherwise
  // it'd mean that the plugin requires two games at once.
  for (auto& requirement : m_Requirements) {
    if (auto* gdep =
            dynamic_cast<const GameDependencyRequirement*>(requirement.get())) {
      return gdep->gameNames();
    }
  }

  return {};
}

std::vector<MOBase::IPlugin*> PluginRequirements::requiredFor() const
{
  std::vector<MOBase::IPlugin*> required;
  std::set<MOBase::IPlugin*> visited;
  requiredFor(required, visited);
  return required;
}

void PluginRequirements::requiredFor(std::vector<MOBase::IPlugin*>& required,
                                     std::set<MOBase::IPlugin*>& visited) const
{
  // Handle cyclic dependencies.
  if (visited.contains(m_Plugin)) {
    return;
  }
  visited.insert(m_Plugin);

  for (auto& [plugin, requirements] : m_PluginContainer->m_Requirements) {

    // If the plugin is not enabled, discard:
    if (!m_PluginContainer->isEnabled(plugin)) {
      continue;
    }

    // Check the requirements:
    for (auto& requirement : requirements.m_Requirements) {

      // We check for plugin dependency. Game dependency are not checked this way.
      if (auto* pdep =
              dynamic_cast<const PluginDependencyRequirement*>(requirement.get())) {

        // Check if at least one of the plugin in the requirements is enabled (except
        // this one):
        bool oneEnabled = false;
        for (auto& pluginName : pdep->pluginNames()) {
          if (pluginName != m_Plugin->name() &&
              m_PluginContainer->isEnabled(pluginName)) {
            oneEnabled = true;
            break;
          }
        }

        // No plugin enabled found, so the plugin requires this plugin:
        if (!oneEnabled) {
          required.push_back(plugin);
          requirements.requiredFor(required, visited);
          break;
        }
      }
    }
  }
}

// PluginContainer

PluginContainer::PluginContainer(OrganizerCore* organizer)
    : m_Organizer(organizer), m_UserInterface(nullptr), m_PreviewGenerator(*this)
{}

PluginContainer::~PluginContainer()
{
  m_Organizer = nullptr;
  unloadPlugins();
}

void PluginContainer::startPlugins(IUserInterface* userInterface)
{
  m_UserInterface = userInterface;
  startPluginsImpl(plugins<QObject>());
}

QStringList PluginContainer::implementedInterfaces(IPlugin* plugin) const
{
  // We need a QObject to be able to qobject_cast<> to the plugin types:
  QObject* oPlugin = as_qobject(plugin);

  if (!oPlugin) {
    return {};
  }

  return implementedInterfaces(oPlugin);
}

QStringList PluginContainer::implementedInterfaces(QObject* oPlugin) const
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

QString PluginContainer::topImplementedInterface(IPlugin* plugin) const
{
  auto interfaces = implementedInterfaces(plugin);
  return interfaces.isEmpty() ? "" : interfaces[0];
}

bool PluginContainer::isBetterInterface(QObject* lhs, QObject* rhs) const
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

QStringList PluginContainer::pluginFileNames() const
{
  QStringList result;
  for (QPluginLoader* loader : m_PluginLoaders) {
    result.append(loader->fileName());
  }
  std::vector<IPluginProxy*> proxyList = bf::at_key<IPluginProxy>(m_Plugins);
  for (IPluginProxy* proxy : proxyList) {
    QStringList proxiedPlugins =
        proxy->pluginList(QCoreApplication::applicationDirPath() + "/" +
                          ToQString(AppConfig::pluginPath()));
    result.append(proxiedPlugins);
  }
  return result;
}

QObject* PluginContainer::as_qobject(MOBase::IPlugin* plugin) const
{
  // Find the correspond QObject - Can this be done safely with a cast?
  auto& objects = bf::at_key<QObject>(m_Plugins);
  auto it =
      std::find_if(std::begin(objects), std::end(objects), [plugin](QObject* obj) {
        return qobject_cast<IPlugin*>(obj) == plugin;
      });

  if (it == std::end(objects)) {
    return nullptr;
  }

  return *it;
}

bool PluginContainer::initPlugin(IPlugin* plugin, IPluginProxy* pluginProxy,
                                 bool skipInit)
{
  // when MO has no instance loaded, init() is not called on plugins, except
  // for proxy plugins, where init() is called with a null IOrganizer
  //
  // after proxies are initialized, instantiate() is called for all the plugins
  // they've discovered, but as for regular plugins, init() won't be
  // called on them if m_OrganizerCore is null

  if (plugin == nullptr) {
    return false;
  }

  OrganizerProxy* proxy = nullptr;
  if (m_Organizer) {
    proxy = new OrganizerProxy(m_Organizer, this, plugin);
    proxy->setParent(as_qobject(plugin));
  }

  // Check if it is a proxy plugin:
  bool isProxy = dynamic_cast<IPluginProxy*>(plugin);

  auto [it, bl] = m_Requirements.emplace(
      plugin, PluginRequirements(this, plugin, proxy, pluginProxy));

  if (!m_Organizer && !isProxy) {
    return true;
  }

  if (skipInit) {
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

void PluginContainer::registerGame(IPluginGame* game)
{
  m_SupportedGames.insert({game->gameName(), game});
}

void PluginContainer::unregisterGame(MOBase::IPluginGame* game)
{
  m_SupportedGames.erase(game->gameName());
}

IPlugin* PluginContainer::registerPlugin(QObject* plugin, const QString& filepath,
                                         MOBase::IPluginProxy* pluginProxy)
{

  // generic treatment for all plugins
  IPlugin* pluginObj = qobject_cast<IPlugin*>(plugin);
  if (pluginObj == nullptr) {
    log::debug("PluginContainer::registerPlugin() called with a non IPlugin QObject.");
    return nullptr;
  }

  // If we already a plugin with this name:
  bool skipInit  = false;
  auto& mapNames = bf::at_key<QString>(m_AccessPlugins);
  if (mapNames.contains(pluginObj->name())) {

    IPlugin* other = mapNames[pluginObj->name()];

    // If both plugins are from the same proxy and the same file, this is usually
    // ok (in theory some one could write two different classes from the same Python
    // file/module):
    if (pluginProxy && m_Requirements.at(other).proxy() == pluginProxy &&
        this->filepath(other) == QDir::cleanPath(filepath)) {

      // Plugin has already been initialized:
      skipInit = true;

      if (isBetterInterface(plugin, as_qobject(other))) {
        log::debug(
            "replacing plugin '{}' with interfaces [{}] by one with interfaces [{}]",
            pluginObj->name(), implementedInterfaces(other).join(", "),
            implementedInterfaces(plugin).join(", "));
        bf::at_key<QString>(m_AccessPlugins)[pluginObj->name()] = pluginObj;
      }
    } else {
      log::warn("Trying to register two plugins with the name '{}' (from {} and {}), "
                "the second one will not be registered.",
                pluginObj->name(), this->filepath(other), QDir::cleanPath(filepath));
      return nullptr;
    }
  } else {
    bf::at_key<QString>(m_AccessPlugins)[pluginObj->name()] = pluginObj;
  }

  // Storing the original QObject* is a bit of a hack as I couldn't figure out any
  // way to cast directly between IPlugin* and IPluginDiagnose*
  bf::at_key<QObject>(m_Plugins).push_back(plugin);

  plugin->setProperty("filepath", QDir::cleanPath(filepath));
  plugin->setParent(this);

  if (m_Organizer) {
    m_Organizer->settings().plugins().registerPlugin(pluginObj);
  }

  {  // diagnosis plugin
    IPluginDiagnose* diagnose = qobject_cast<IPluginDiagnose*>(plugin);
    if (diagnose != nullptr) {
      bf::at_key<IPluginDiagnose>(m_Plugins).push_back(diagnose);
      bf::at_key<IPluginDiagnose>(m_AccessPlugins)[diagnose] = pluginObj;
      diagnose->onInvalidated([&]() {
        emit diagnosisUpdate();
      });
    }
  }
  {  // file mapper plugin
    IPluginFileMapper* mapper = qobject_cast<IPluginFileMapper*>(plugin);
    if (mapper != nullptr) {
      bf::at_key<IPluginFileMapper>(m_Plugins).push_back(mapper);
      bf::at_key<IPluginFileMapper>(m_AccessPlugins)[mapper] = pluginObj;
    }
  }
  {  // mod page plugin
    IPluginModPage* modPage = qobject_cast<IPluginModPage*>(plugin);
    if (initPlugin(modPage, pluginProxy, skipInit)) {
      bf::at_key<IPluginModPage>(m_Plugins).push_back(modPage);
      emit pluginRegistered(modPage);
      return modPage;
    }
  }
  {  // game plugin
    IPluginGame* game = qobject_cast<IPluginGame*>(plugin);
    if (game) {
      game->detectGame();
      if (initPlugin(game, pluginProxy, skipInit)) {
        bf::at_key<IPluginGame>(m_Plugins).push_back(game);
        registerGame(game);
        emit pluginRegistered(game);
        return game;
      }
    }
  }
  {  // tool plugins
    IPluginTool* tool = qobject_cast<IPluginTool*>(plugin);
    if (initPlugin(tool, pluginProxy, skipInit)) {
      bf::at_key<IPluginTool>(m_Plugins).push_back(tool);
      emit pluginRegistered(tool);
      return tool;
    }
  }
  {  // installer plugins
    IPluginInstaller* installer = qobject_cast<IPluginInstaller*>(plugin);
    if (initPlugin(installer, pluginProxy, skipInit)) {
      bf::at_key<IPluginInstaller>(m_Plugins).push_back(installer);
      if (m_Organizer) {
        installer->setInstallationManager(m_Organizer->installationManager());
      }
      emit pluginRegistered(installer);
      return installer;
    }
  }
  {  // preview plugins
    IPluginPreview* preview = qobject_cast<IPluginPreview*>(plugin);
    if (initPlugin(preview, pluginProxy, skipInit)) {
      bf::at_key<IPluginPreview>(m_Plugins).push_back(preview);
      return preview;
    }
  }
  {  // proxy plugins
    IPluginProxy* proxy = qobject_cast<IPluginProxy*>(plugin);
    if (initPlugin(proxy, pluginProxy, skipInit)) {
      bf::at_key<IPluginProxy>(m_Plugins).push_back(proxy);
      emit pluginRegistered(proxy);

      QStringList filepaths =
          proxy->pluginList(QCoreApplication::applicationDirPath() + "/" +
                            ToQString(AppConfig::pluginPath()));
      for (const QString& filepath : filepaths) {
        loadProxied(filepath, proxy);
      }
      return proxy;
    }
  }

  {  // dummy plugins
    // only initialize these, no processing otherwise
    IPlugin* dummy = qobject_cast<IPlugin*>(plugin);
    if (initPlugin(dummy, pluginProxy, skipInit)) {
      bf::at_key<IPlugin>(m_Plugins).push_back(dummy);
      emit pluginRegistered(dummy);
      return dummy;
    }
  }

  return nullptr;
}

IPlugin* PluginContainer::managedGame() const
{
  // TODO: This const_cast is safe but ugly. Most methods require a IPlugin*, so
  // returning a const-version if painful. This should be fixed by making methods accept
  // a const IPlugin* instead, but there are a few tricks with qobject_cast and const.
  return const_cast<IPluginGame*>(m_Organizer->managedGame());
}

bool PluginContainer::isEnabled(IPlugin* plugin) const
{
  // Check if it's a game plugin:
  if (implementInterface<IPluginGame>(plugin)) {
    return plugin == m_Organizer->managedGame();
  }

  // Check the master, if any:
  auto& requirements = m_Requirements.at(plugin);

  if (requirements.master()) {
    return isEnabled(requirements.master());
  }

  // Check if the plugin is enabled:
  if (!m_Organizer->persistent(plugin->name(), "enabled", plugin->enabledByDefault())
           .toBool()) {
    return false;
  }

  // Check the requirements:
  return m_Requirements.at(plugin).canEnable();
}

void PluginContainer::setEnabled(MOBase::IPlugin* plugin, bool enable,
                                 bool dependencies)
{
  // If required, disable dependencies:
  if (!enable && dependencies) {
    for (auto* p : requirements(plugin).requiredFor()) {
      setEnabled(
          p, false,
          false);  // No need to "recurse" here since requiredFor already does it.
    }
  }

  // Always disable/enable child plugins:
  for (auto* p : requirements(plugin).children()) {
    // "Child" plugin should have no dependencies.
    setEnabled(p, enable, false);
  }

  m_Organizer->setPersistent(plugin->name(), "enabled", enable, true);

  if (enable) {
    emit pluginEnabled(plugin);
  } else {
    emit pluginDisabled(plugin);
  }
}

MOBase::IPlugin* PluginContainer::plugin(QString const& pluginName) const
{
  auto& map = bf::at_key<QString>(m_AccessPlugins);
  auto it   = map.find(pluginName);
  if (it == std::end(map)) {
    return nullptr;
  }
  return it->second;
}

MOBase::IPlugin* PluginContainer::plugin(MOBase::IPluginDiagnose* diagnose) const
{
  auto& map = bf::at_key<IPluginDiagnose>(m_AccessPlugins);
  auto it   = map.find(diagnose);
  if (it == std::end(map)) {
    return nullptr;
  }
  return it->second;
}

MOBase::IPlugin* PluginContainer::plugin(MOBase::IPluginFileMapper* mapper) const
{
  auto& map = bf::at_key<IPluginFileMapper>(m_AccessPlugins);
  auto it   = map.find(mapper);
  if (it == std::end(map)) {
    return nullptr;
  }
  return it->second;
}

bool PluginContainer::isEnabled(QString const& pluginName) const
{
  IPlugin* p = plugin(pluginName);
  return p ? isEnabled(p) : false;
}
bool PluginContainer::isEnabled(MOBase::IPluginDiagnose* diagnose) const
{
  IPlugin* p = plugin(diagnose);
  return p ? isEnabled(p) : false;
}
bool PluginContainer::isEnabled(MOBase::IPluginFileMapper* mapper) const
{
  IPlugin* p = plugin(mapper);
  return p ? isEnabled(p) : false;
}

const PluginRequirements& PluginContainer::requirements(IPlugin* plugin) const
{
  return m_Requirements.at(plugin);
}

OrganizerProxy* PluginContainer::organizerProxy(MOBase::IPlugin* plugin) const
{
  return requirements(plugin).m_Organizer;
}

MOBase::IPluginProxy* PluginContainer::pluginProxy(MOBase::IPlugin* plugin) const
{
  return requirements(plugin).proxy();
}

QString PluginContainer::filepath(MOBase::IPlugin* plugin) const
{
  return as_qobject(plugin)->property("filepath").toString();
}

IPluginGame* PluginContainer::game(const QString& name) const
{
  auto iter = m_SupportedGames.find(name);
  if (iter != m_SupportedGames.end()) {
    return iter->second;
  } else {
    return nullptr;
  }
}

const PreviewGenerator& PluginContainer::previewGenerator() const
{
  return m_PreviewGenerator;
}

void PluginContainer::startPluginsImpl(const std::vector<QObject*>& plugins) const
{
  // setUserInterface()
  if (m_UserInterface) {
    for (auto* plugin : plugins) {
      if (auto* proxy = qobject_cast<IPluginProxy*>(plugin)) {
        proxy->setParentWidget(m_UserInterface->mainWindow());
      }
      if (auto* modPage = qobject_cast<IPluginModPage*>(plugin)) {
        modPage->setParentWidget(m_UserInterface->mainWindow());
      }
      if (auto* tool = qobject_cast<IPluginTool*>(plugin)) {
        tool->setParentWidget(m_UserInterface->mainWindow());
      }
      if (auto* installer = qobject_cast<IPluginInstaller*>(plugin)) {
        installer->setParentWidget(m_UserInterface->mainWindow());
      }
    }
  }

  // Trigger initial callbacks, e.g. onUserInterfaceInitialized and onProfileChanged.
  if (m_Organizer) {
    for (auto* object : plugins) {
      auto* plugin = qobject_cast<IPlugin*>(object);
      auto* oproxy = organizerProxy(plugin);
      oproxy->connectSignals();
      oproxy->m_ProfileChanged(nullptr, m_Organizer->currentProfile());

      if (m_UserInterface) {
        oproxy->m_UserInterfaceInitialized(m_UserInterface->mainWindow());
      }
    }
  }
}

std::vector<QObject*> PluginContainer::loadProxied(const QString& filepath,
                                                   IPluginProxy* proxy)
{
  std::vector<QObject*> proxiedPlugins;

  try {
    // We get a list of matching plugins as proxies can return multiple plugins
    // per file and do not  have a good way of supporting multiple inheritance.
    QList<QObject*> matchingPlugins = proxy->load(filepath);

    // We are going to group plugin by names and "fix" them later:
    std::map<QString, std::vector<IPlugin*>> proxiedByNames;

    for (QObject* proxiedPlugin : matchingPlugins) {
      if (proxiedPlugin != nullptr) {

        if (IPlugin* proxied = registerPlugin(proxiedPlugin, filepath, proxy);
            proxied) {
          log::debug("loaded plugin '{}' from '{}' - [{}]", proxied->name(),
                     QFileInfo(filepath).fileName(),
                     implementedInterfaces(proxied).join(", "));

          // Store the plugin for later:
          proxiedPlugins.push_back(proxiedPlugin);
          proxiedByNames[proxied->name()].push_back(proxied);
        } else {
          log::warn("plugin \"{}\" failed to load. If this plugin is for an older "
                    "version of MO "
                    "you have to update it or delete it if no update exists.",
                    filepath);
        }
      }
    }

    // Fake masters:
    for (auto& [name, proxiedPlugins] : proxiedByNames) {
      if (proxiedPlugins.size() > 1) {
        auto it = std::min_element(std::begin(proxiedPlugins), std::end(proxiedPlugins),
                                   [&](auto const& lhs, auto const& rhs) {
                                     return isBetterInterface(as_qobject(lhs),
                                                              as_qobject(rhs));
                                   });

        for (auto& proxiedPlugin : proxiedPlugins) {
          if (proxiedPlugin != *it) {
            m_Requirements.at(proxiedPlugin).setMaster(*it);
          }
        }
      }
    }
  } catch (const std::exception& e) {
    reportError(
        QObject::tr("failed to initialize plugin %1: %2").arg(filepath).arg(e.what()));
  }

  return proxiedPlugins;
}

QObject* PluginContainer::loadQtPlugin(const QString& filepath)
{
  std::unique_ptr<QPluginLoader> pluginLoader(new QPluginLoader(filepath, this));
  if (pluginLoader->instance() == nullptr) {
    m_FailedPlugins.push_back(filepath);
    log::error("failed to load plugin {}: {}", filepath, pluginLoader->errorString());
  } else {
    QObject* object = pluginLoader->instance();
    if (IPlugin* plugin = registerPlugin(object, filepath, nullptr); plugin) {
      log::debug("loaded plugin '{}' from '{}' - [{}]", plugin->name(),
                 QFileInfo(filepath).fileName(),
                 implementedInterfaces(plugin).join(", "));
      m_PluginLoaders.push_back(pluginLoader.release());
      return object;
    } else {
      m_FailedPlugins.push_back(filepath);
      log::warn("plugin '{}' failed to load (may be outdated)", filepath);
    }
  }
  return nullptr;
}

std::optional<QString> PluginContainer::isQtPluginFolder(const QString& filepath) const
{

  if (!QFileInfo(filepath).isDir()) {
    return {};
  }

  QDirIterator iter(filepath, QDir::Files | QDir::NoDotAndDotDot);
  while (iter.hasNext()) {
    iter.next();
    const auto filePath = iter.filePath();

    // not a library, skip
    if (!QLibrary::isLibrary(filePath)) {
      continue;
    }

    // check if we have proper metadata - this does not load the plugin (metaData()
    // should be very lightweight)
    const QPluginLoader loader(filePath);
    if (!loader.metaData().isEmpty()) {
      return filePath;
    }
  }

  return {};
}

void PluginContainer::loadPlugin(QString const& filepath)
{
  std::vector<QObject*> plugins;
  if (QFileInfo(filepath).isFile() && QLibrary::isLibrary(filepath)) {
    QObject* plugin = loadQtPlugin(filepath);
    if (plugin) {
      plugins.push_back(plugin);
    }
  } else if (auto p = isQtPluginFolder(filepath)) {
    QObject* plugin = loadQtPlugin(*p);
    if (plugin) {
      plugins.push_back(plugin);
    }
  } else {
    // We need to check if this can be handled by a proxy.
    for (auto* proxy : this->plugins<IPluginProxy>()) {
      auto filepaths = proxy->pluginList(QCoreApplication::applicationDirPath() + "/" +
                                         ToQString(AppConfig::pluginPath()));
      if (filepaths.contains(filepath)) {
        plugins = loadProxied(filepath, proxy);
        break;
      }
    }
  }

  for (auto* plugin : plugins) {
    emit pluginRegistered(qobject_cast<IPlugin*>(plugin));
  }

  startPluginsImpl(plugins);
}

void PluginContainer::unloadPlugin(MOBase::IPlugin* plugin, QObject* object)
{
  if (auto* game = qobject_cast<IPluginGame*>(object)) {

    if (game == managedGame()) {
      throw Exception("cannot unload the plugin for the currently managed game");
    }

    unregisterGame(game);
  }

  // We need to remove from the m_Plugins maps BEFORE unloading from the proxy
  // otherwise the qobject_cast to check the plugin type will not work.
  bf::for_each(m_Plugins, [object](auto& t) {
    using type = typename std::decay_t<decltype(t.second)>::value_type;

    // We do not want to remove from QObject since we are iterating over them.
    if constexpr (!std::is_same<type, QObject*>{}) {
      auto itp =
          std::find(t.second.begin(), t.second.end(), qobject_cast<type>(object));
      if (itp != t.second.end()) {
        t.second.erase(itp);
      }
    }
  });

  emit pluginUnregistered(plugin);

  // Remove from the members.
  if (auto* diagnose = qobject_cast<IPluginDiagnose*>(object)) {
    bf::at_key<IPluginDiagnose>(m_AccessPlugins).erase(diagnose);
  }
  if (auto* mapper = qobject_cast<IPluginFileMapper*>(object)) {
    bf::at_key<IPluginFileMapper>(m_AccessPlugins).erase(mapper);
  }

  auto& mapNames = bf::at_key<QString>(m_AccessPlugins);
  if (mapNames.contains(plugin->name())) {
    mapNames.erase(plugin->name());
  }

  m_Organizer->settings().plugins().unregisterPlugin(plugin);

  // Force disconnection of the signals from the proxies. This is a safety
  // operations since those signals should be disconnected when the proxies
  // are destroyed anyway.
  organizerProxy(plugin)->disconnectSignals();

  // Is this a proxied plugin?
  auto* proxy = pluginProxy(plugin);

  if (proxy) {
    proxy->unload(filepath(plugin));
  } else {
    // We need to find the loader.
    auto it = std::find_if(m_PluginLoaders.begin(), m_PluginLoaders.end(),
                           [object](auto* loader) {
                             return loader->instance() == object;
                           });

    if (it != m_PluginLoaders.end()) {
      if (!(*it)->unload()) {
        log::error("failed to unload {}: {}", (*it)->fileName(), (*it)->errorString());
      }
      delete *it;
      m_PluginLoaders.erase(it);
    } else {
      log::error("loader for plugin {} does not exist, cannot unload", plugin->name());
    }
  }

  object->deleteLater();

  // Do this at the end.
  m_Requirements.erase(plugin);
}

void PluginContainer::unloadPlugin(QString const& filepath)
{
  // We need to find all the plugins from the given path and
  // unload them:
  QString cleanPath = QDir::cleanPath(filepath);
  auto& objects     = bf::at_key<QObject>(m_Plugins);
  for (auto it = objects.begin(); it != objects.end();) {
    auto* plugin = qobject_cast<IPlugin*>(*it);
    if (this->filepath(plugin) == filepath) {
      unloadPlugin(plugin, *it);
      it = objects.erase(it);
    } else {
      ++it;
    }
  }
}

void PluginContainer::reloadPlugin(QString const& filepath)
{
  unloadPlugin(filepath);
  loadPlugin(filepath);
}

void PluginContainer::unloadPlugins()
{
  if (m_Organizer) {
    // this will clear several structures that can hold on to pointers to
    // plugins, as well as read the plugin blacklist from the ini file, which
    // is used in loadPlugins() below to skip plugins
    //
    // note that the first thing loadPlugins() does is call unloadPlugins(),
    // so this makes sure the blacklist is always available
    m_Organizer->settings().plugins().clearPlugins();
  }

  bf::for_each(m_Plugins, [](auto& t) {
    t.second.clear();
  });
  bf::for_each(m_AccessPlugins, [](auto& t) {
    t.second.clear();
  });
  m_Requirements.clear();

  while (!m_PluginLoaders.empty()) {
    QPluginLoader* loader = m_PluginLoaders.back();
    m_PluginLoaders.pop_back();
    if ((loader != nullptr) && !loader->unload()) {
      log::debug("failed to unload {}: {}", loader->fileName(), loader->errorString());
    }
    delete loader;
  }
}

void PluginContainer::loadPlugins()
{
  TimeThis tt("PluginContainer::loadPlugins()");

  unloadPlugins();

  for (QObject* plugin : QPluginLoader::staticInstances()) {
    registerPlugin(plugin, "", nullptr);
  }

  QFile loadCheck;
  QString skipPlugin;

  if (m_Organizer) {
    loadCheck.setFileName(qApp->property("dataPath").toString() +
                          "/plugin_loadcheck.tmp");

    if (loadCheck.exists() && loadCheck.open(QIODevice::ReadOnly)) {
      // oh, there was a failed plugin load last time. Find out which plugin was loaded
      // last
      QString fileName;
      while (!loadCheck.atEnd()) {
        fileName = QString::fromUtf8(loadCheck.readLine().constData()).trimmed();
      }

      log::warn("loadcheck file found for plugin '{}'", fileName);

      MOBase::TaskDialog dlg;

      const auto Skip      = QMessageBox::Ignore;
      const auto Blacklist = QMessageBox::Cancel;
      const auto Load      = QMessageBox::Ok;

      const auto r =
          dlg.title(tr("Plugin error"))
              .main(tr("Mod Organizer failed to load the plugin '%1' last time it was "
                       "started.")
                        .arg(fileName))
              .content(tr(
                  "The plugin can be skipped for this session, blacklisted, "
                  "or loaded normally, in which case it might fail again. Blacklisted "
                  "plugins can be re-enabled later in the settings."))
              .icon(QMessageBox::Warning)
              .button({tr("Skip this plugin"), Skip})
              .button({tr("Blacklist this plugin"), Blacklist})
              .button({tr("Load this plugin"), Load})
              .exec();

      switch (r) {
      case Skip:
        log::warn("user wants to skip plugin '{}'", fileName);
        skipPlugin = fileName;
        break;

      case Blacklist:
        log::warn("user wants to blacklist plugin '{}'", fileName);
        m_Organizer->settings().plugins().addBlacklist(fileName);
        break;

      case Load:
        log::warn("user wants to load plugin '{}' anyway", fileName);
        break;
      }

      loadCheck.close();
    }

    loadCheck.open(QIODevice::WriteOnly);
  }

  QString pluginPath =
      qApp->applicationDirPath() + "/" + ToQString(AppConfig::pluginPath());
  log::debug("looking for plugins in {}", QDir::toNativeSeparators(pluginPath));
  QDirIterator iter(pluginPath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

  while (iter.hasNext()) {
    iter.next();

    if (skipPlugin == iter.fileName()) {
      log::debug("plugin \"{}\" skipped for this session", iter.fileName());
      continue;
    }

    if (m_Organizer) {
      if (m_Organizer->settings().plugins().blacklisted(iter.fileName())) {
        log::debug("plugin \"{}\" blacklisted", iter.fileName());
        continue;
      }
    }

    if (loadCheck.isOpen()) {
      loadCheck.write(iter.fileName().toUtf8());
      loadCheck.write("\n");
      loadCheck.flush();
    }

    QString filepath = iter.filePath();
    if (QLibrary::isLibrary(filepath)) {
      loadQtPlugin(filepath);
    } else if (auto p = isQtPluginFolder(filepath)) {
      loadQtPlugin(*p);
    }
  }

  if (skipPlugin.isEmpty()) {
    // remove the load check file on success
    if (loadCheck.isOpen()) {
      loadCheck.remove();
    }
  } else {
    // remember the plugin for next time
    if (loadCheck.isOpen()) {
      loadCheck.close();
    }

    log::warn("user skipped plugin '{}', remembering in loadcheck", skipPlugin);
    loadCheck.open(QIODevice::WriteOnly);
    loadCheck.write(skipPlugin.toUtf8());
    loadCheck.write("\n");
    loadCheck.flush();
  }

  bf::at_key<IPluginDiagnose>(m_Plugins).push_back(this);

  if (m_Organizer) {
    bf::at_key<IPluginDiagnose>(m_Plugins).push_back(m_Organizer);
    m_Organizer->connectPlugins(this);
  }
}

std::vector<unsigned int> PluginContainer::activeProblems() const
{
  std::vector<unsigned int> problems;
  if (m_FailedPlugins.size()) {
    problems.push_back(PROBLEM_PLUGINSNOTLOADED);
  }
  return problems;
}

QString PluginContainer::shortDescription(unsigned int key) const
{
  switch (key) {
  case PROBLEM_PLUGINSNOTLOADED: {
    return tr("Some plugins could not be loaded");
  } break;
  default: {
    return tr("Description missing");
  } break;
  }
}

QString PluginContainer::fullDescription(unsigned int key) const
{
  switch (key) {
  case PROBLEM_PLUGINSNOTLOADED: {
    QString result =
        tr("The following plugins could not be loaded. The reason may be missing "
           "dependencies (i.e. python) or an outdated version:") +
        "<ul>";
    for (const QString& plugin : m_FailedPlugins) {
      result += "<li>" + plugin + "</li>";
    }
    result += "<ul>";
    return result;
  } break;
  default: {
    return tr("Description missing");
  } break;
  }
}

bool PluginContainer::hasGuidedFix(unsigned int) const
{
  return false;
}

void PluginContainer::startGuidedFix(unsigned int) const {}
