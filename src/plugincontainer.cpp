#include "plugincontainer.h"
#include "organizercore.h"
#include "organizerproxy.h"
#include "report.h"
#include <ipluginproxy.h>
#include "iuserinterface.h"
#include <idownloadmanager.h>
#include "shared/appconfig.h"
#include <QAction>
#include <QToolButton>
#include <QCoreApplication>
#include <QMessageBox>
#include <QDirIterator>
#include <boost/fusion/sequence/intrinsic/at_key.hpp>
#include <boost/fusion/include/at_key.hpp>
#include <boost/fusion/algorithm/iteration/for_each.hpp>
#include <boost/fusion/include/for_each.hpp>

using namespace MOBase;
using namespace MOShared;

namespace bf = boost::fusion;

template <class T>
struct PluginTypeName;

template <> struct PluginTypeName<MOBase::IPlugin> { static QString value() { return QT_TR_NOOP("Plugin"); } };
template <> struct PluginTypeName<MOBase::IPluginDiagnose> { static QString value() { return QT_TR_NOOP("Diagnose"); } };
template <> struct PluginTypeName<MOBase::IPluginGame> { static QString value() { return QT_TR_NOOP("Game"); } };
template <> struct PluginTypeName<MOBase::IPluginInstaller> { static QString value() { return QT_TR_NOOP("Installer"); } };
template <> struct PluginTypeName<MOBase::IPluginModPage> { static QString value() { return QT_TR_NOOP("Mod Page"); } };
template <> struct PluginTypeName<MOBase::IPluginPreview> { static QString value() { return QT_TR_NOOP("Preview"); } };
template <> struct PluginTypeName<MOBase::IPluginTool> { static QString value() { return QT_TR_NOOP("Tool"); } };
template <> struct PluginTypeName<MOBase::IPluginProxy> { static QString value() { return QT_TR_NOOP("Proxy"); } };
template <> struct PluginTypeName<MOBase::IPluginFileMapper> { static QString value() { return QT_TR_NOOP("File Mapper"); } };


QStringList PluginContainer::pluginInterfaces()
{
  // Find all the names:
  QStringList names;
  boost::mp11::mp_for_each<PluginTypeOrder>([&names](const auto* p) {
    using plugin_type = std::decay_t<decltype(*p)>;
    auto name = PluginTypeName<plugin_type>::value();
    if (!name.isEmpty()) {
      names.append(name);
    }
  });

  return names;
}


// PluginRequirementProxy

PluginRequirements::PluginRequirements(PluginContainer* pluginContainer, MOBase::IPlugin* plugin, IOrganizer* proxy, MOBase::IPluginProxy* pluginProxy)
  : m_PluginContainer(pluginContainer)
  , m_Plugin(plugin)
  , m_PluginProxy(pluginProxy)
  , m_Organizer(proxy)
{
  for (auto* requirement : plugin->requirements()) {
    m_Requirements.emplace_back(requirement);
  }

  // TODO:
  if (pluginProxy) {
    m_Requirements.emplace_back(PluginRequirementFactory::pluginDependency(pluginProxy->name()));
  }
}

MOBase::IPluginProxy* PluginRequirements::proxy() const
{
  return m_PluginProxy;
}

std::vector<PluginRequirements::Problem> PluginRequirements::problems() const
{
  std::vector<Problem> result;
  for (auto& requirement : m_Requirements) {
    for (auto p : requirement->problems(m_Organizer)) {
      result.push_back(Problem(requirement.get(), p));
    }
  }
  return result;
}

bool PluginRequirements::canEnable() const
{
  return problems().empty();
}

QStringList PluginRequirements::requiredGames() const
{
  // We look for a "GameDependencyRequirement" - There can be only one since otherwise
  // it'd mean that the plugin requires two games at once.
  for (auto& requirement : m_Requirements) {
    if (auto* gdep = dynamic_cast<const GameDependencyRequirement*>(requirement.get())) {
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


void PluginRequirements::requiredFor(std::vector<MOBase::IPlugin*> &required, std::set<MOBase::IPlugin*>& visited) const
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
      if (auto* pdep = dynamic_cast<const PluginDependencyRequirement*>(requirement.get())) {

        // Check if at least one of the plugin in the requirements is enabled (except this
        // one):
        bool oneEnabled = false;
        for (auto& pluginName : pdep->pluginNames()) {
          if (pluginName != m_Plugin->name() && m_PluginContainer->isEnabled(pluginName)) {
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

PluginContainer::PluginContainer(OrganizerCore *organizer)
  : m_Organizer(organizer)
  , m_UserInterface(nullptr)
  , m_PreviewGenerator(this)
{
}

PluginContainer::~PluginContainer() {
  m_Organizer = nullptr;
  unloadPlugins();
}


void PluginContainer::setUserInterface(IUserInterface *userInterface, QWidget *widget)
{
  for (IPluginProxy *proxy : bf::at_key<IPluginProxy>(m_Plugins)) {
    proxy->setParentWidget(widget);
  }

  if (userInterface != nullptr) {
    for (IPluginModPage *modPage : bf::at_key<IPluginModPage>(m_Plugins)) {
      userInterface->registerModPage(modPage);
    }
  }

  m_UserInterface = userInterface;
}


QStringList PluginContainer::implementedInterfaces(IPlugin* plugin) const
{
  // We need a QObject to be able to qobject_cast<> to the plugin types:
  QObject* oPlugin = as_qobject(plugin);

  if (!oPlugin) {
    return {};
  }

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
  // We need a QObject to be able to qobject_cast<> to the plugin types:
  QObject* oPlugin = as_qobject(plugin);

  if (!oPlugin) {
    return {};
  }

  // Find all the names:
  QString name;
  boost::mp11::mp_for_each<PluginTypeOrder>([oPlugin, &name](auto* p) {
    using plugin_type = std::decay_t<decltype(*p)>;
    if (name.isEmpty() && qobject_cast<plugin_type*>(oPlugin)) {
      auto tname = PluginTypeName<plugin_type>::value();
      if (!tname.isEmpty()) {
        name = tname;
      }
    }
    });

  return name;
}


QStringList PluginContainer::pluginFileNames() const
{
  QStringList result;
  for (QPluginLoader *loader : m_PluginLoaders) {
    result.append(loader->fileName());
  }
  std::vector<IPluginProxy *> proxyList = bf::at_key<IPluginProxy>(m_Plugins);
  for (IPluginProxy *proxy : proxyList) {
    QStringList proxiedPlugins = proxy->pluginList(
            QCoreApplication::applicationDirPath() + "/" + ToQString(AppConfig::pluginPath()));
    result.append(proxiedPlugins);
  }
  return result;
}


QObject* PluginContainer::as_qobject(MOBase::IPlugin* plugin) const
{
  // Find the correspond QObject - Can this be done safely with a cast?
  auto& objects = bf::at_key<QObject>(m_Plugins);
  auto it = std::find_if(std::begin(objects), std::end(objects), [plugin](QObject* obj) {
    return qobject_cast<IPlugin*>(obj) == plugin;
    });

  if (it == std::end(objects)) {
    return nullptr;
  }

  return *it;
}

bool PluginContainer::initPlugin(IPlugin *plugin, IPluginProxy *pluginProxy)
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
  }

  // Check if it is a proxy plugin:
  bool isProxy = dynamic_cast<IPluginProxy*>(plugin);

  if (!m_Organizer && !isProxy) {
    return true;
  }

  if (!plugin->init(proxy)) {
    log::warn("plugin failed to initialize");
    return false;
  }

  m_Requirements.emplace(plugin, PluginRequirements(this, plugin, proxy, pluginProxy));

  return true;
}

void PluginContainer::registerGame(IPluginGame *game)
{
  m_SupportedGames.insert({ game->gameName(), game });
}

bool PluginContainer::registerPlugin(QObject *plugin, const QString &fileName, MOBase::IPluginProxy* pluginProxy)
{
  // Storing the original QObject* is a bit of a hack as I couldn't figure out any
  // way to cast directly between IPlugin* and IPluginDiagnose*
  bf::at_key<QObject>(m_Plugins).push_back(plugin);

  // generic treatment for all plugins
  IPlugin *pluginObj = qobject_cast<IPlugin*>(plugin);
  if (pluginObj == nullptr) {
    log::debug("not an IPlugin");
    return false;
  }
  bf::at_key<QString>(m_AccessPlugins)[pluginObj->name()] = pluginObj;

  plugin->setProperty("filename", fileName);

  if (m_Organizer) {
    m_Organizer->settings().plugins().registerPlugin(pluginObj);
  }

  { // diagnosis plugin
    IPluginDiagnose *diagnose = qobject_cast<IPluginDiagnose*>(plugin);
    if (diagnose != nullptr) {
      bf::at_key<IPluginDiagnose>(m_Plugins).push_back(diagnose);
      bf::at_key<IPluginDiagnose>(m_AccessPlugins)[diagnose] = pluginObj;
      m_DiagnosisConnections.push_back(
            diagnose->onInvalidated([&] () { emit diagnosisUpdate(); })
            );
    }
  }
  { // file mapper plugin
    IPluginFileMapper *mapper = qobject_cast<IPluginFileMapper*>(plugin);
    if (mapper != nullptr) {
      bf::at_key<IPluginFileMapper>(m_Plugins).push_back(mapper);
      bf::at_key<IPluginFileMapper>(m_AccessPlugins)[mapper] = pluginObj;
    }
  }
  { // mod page plugin
    IPluginModPage *modPage = qobject_cast<IPluginModPage*>(plugin);
    if (initPlugin(modPage, pluginProxy)) {
      bf::at_key<IPluginModPage>(m_Plugins).push_back(modPage);
      return true;
    }
  }
  { // game plugin
    IPluginGame *game = qobject_cast<IPluginGame*>(plugin);
    if (game) {
      game->detectGame();
      if (initPlugin(game, pluginProxy)) {
        bf::at_key<IPluginGame>(m_Plugins).push_back(game);
        registerGame(game);
        return true;
      }
    }
  }
  { // tool plugins
    IPluginTool *tool = qobject_cast<IPluginTool*>(plugin);
    if (initPlugin(tool, pluginProxy)) {
      bf::at_key<IPluginTool>(m_Plugins).push_back(tool);
      return true;
    }
  }
  { // installer plugins
    IPluginInstaller *installer = qobject_cast<IPluginInstaller*>(plugin);
    if (initPlugin(installer, pluginProxy)) {
      bf::at_key<IPluginInstaller>(m_Plugins).push_back(installer);
      if (m_Organizer) {
        m_Organizer->installationManager()->registerInstaller(installer);
      }
      return true;
    }
  }
  { // preview plugins
    IPluginPreview *preview = qobject_cast<IPluginPreview*>(plugin);
    if (initPlugin(preview, pluginProxy)) {
      bf::at_key<IPluginPreview>(m_Plugins).push_back(preview);
      m_PreviewGenerator.registerPlugin(preview);
      return true;
    }
  }
  { // proxy plugins
    IPluginProxy *proxy = qobject_cast<IPluginProxy*>(plugin);
    if (initPlugin(proxy, pluginProxy)) {
      bf::at_key<IPluginProxy>(m_Plugins).push_back(proxy);
      QStringList pluginNames = proxy->pluginList(
            QCoreApplication::applicationDirPath() + "/" + ToQString(AppConfig::pluginPath()));
      for (const QString &pluginName : pluginNames) {
        try {
          // we get a list of matching plugins as proxies don't necessarily have a good way of supporting multiple inheritance
          QList<QObject*> matchingPlugins = proxy->instantiate(pluginName);
          for (QObject *proxiedPlugin : matchingPlugins) {
            if (proxiedPlugin != nullptr) {
              if (registerPlugin(proxiedPlugin, pluginName, proxy)) {
                log::debug("loaded plugin \"{}\"", QFileInfo(pluginName).fileName());
              }
              else {
                log::warn(
                  "plugin \"{}\" failed to load. If this plugin is for an older version of MO "
                  "you have to update it or delete it if no update exists.",
                  pluginName);
              }
            }
          }
        } catch (const std::exception &e) {
          reportError(QObject::tr("failed to initialize plugin %1: %2").arg(pluginName).arg(e.what()));
        }
      }
      return true;
    }
  }

  { // dummy plugins
    // only initialize these, no processing otherwise
    IPlugin *dummy = qobject_cast<IPlugin*>(plugin);
    if (initPlugin(dummy, pluginProxy)) {
      bf::at_key<IPlugin>(m_Plugins).push_back(dummy);
      return true;
    }
  }

  log::debug("no matching plugin interface");

  return false;
}

struct clearPlugins
{
    template<typename T>
    void operator()(T& t) const
    {
      t.second.clear();
    }
};

void PluginContainer::unloadPlugins()
{
  if (m_UserInterface != nullptr) {
    m_UserInterface->disconnectPlugins();
  }

  // disconnect all slots before unloading plugins so plugins don't have to take care of that
  if (m_Organizer) {
    m_Organizer->disconnectPlugins();
  }

  bf::for_each(m_Plugins, clearPlugins());

  for (const boost::signals2::connection &connection : m_DiagnosisConnections) {
    connection.disconnect();
  }
  m_DiagnosisConnections.clear();

  while (!m_PluginLoaders.empty()) {
    QPluginLoader *loader = m_PluginLoaders.back();
    m_PluginLoaders.pop_back();
    if ((loader != nullptr) && !loader->unload()) {
      log::debug("failed to unload {}: {}", loader->fileName(), loader->errorString());
    }
    delete loader;
  }
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

  // Check if the plugin is enabled:
  if (!m_Organizer->persistent(plugin->name(), "enabled", true).toBool()) {
    return false;
  }

  // Check the requirements (if a plugin checks in init(), the requirements have
  // not been computed yet):
  auto it = m_Requirements.find(plugin);

  return it == std::end(m_Requirements) ? true : it->second.canEnable();
}

void PluginContainer::setEnabled(MOBase::IPlugin* plugin, bool enable, bool dependencies)
{
  if (!enable && dependencies) {
    for (auto* p : requirements(plugin).requiredFor()) {
      setEnabled(p, false, false);  // No need to "recurse" here since requiredFor already does it.
    }
  }
  m_Organizer->setPersistent(plugin->name(), "enabled", enable, true);
}

MOBase::IPlugin* PluginContainer::plugin(QString const& pluginName) const
{
  auto& map = bf::at_key<QString>(m_AccessPlugins);
  auto it = map.find(pluginName);
  if (it == std::end(map)) {
    return nullptr;
  }
  return it->second;
}

MOBase::IPlugin* PluginContainer::plugin(MOBase::IPluginDiagnose* diagnose) const
{
  auto& map = bf::at_key<IPluginDiagnose>(m_AccessPlugins);
  auto it = map.find(diagnose);
  if (it == std::end(map)) {
    return nullptr;
  }
  return it->second;
}

MOBase::IPlugin* PluginContainer::plugin(MOBase::IPluginFileMapper* mapper) const
{
  auto& map = bf::at_key<IPluginFileMapper>(m_AccessPlugins);
  auto it = map.find(mapper);
  if (it == std::end(map)) {
    return nullptr;
  }
  return it->second;
}

bool PluginContainer::isEnabled(QString const& pluginName) const {
  IPlugin* p = plugin(pluginName);
  return p ? isEnabled(p) : false;
}
bool PluginContainer::isEnabled(MOBase::IPluginDiagnose* diagnose) const {
  IPlugin* p = plugin(diagnose);
  return p ? isEnabled(p) : false;
}
bool PluginContainer::isEnabled(MOBase::IPluginFileMapper* mapper) const {
  IPlugin* p = plugin(mapper);
  return p ? isEnabled(p) : false;
}

const PluginRequirements& PluginContainer::requirements(IPlugin* plugin) const
{
  return m_Requirements.at(plugin);
}

IPluginGame *PluginContainer::managedGame(const QString &name) const
{
  auto iter = m_SupportedGames.find(name);
  if (iter != m_SupportedGames.end()) {
    return iter->second;
  } else {
    return nullptr;
  }
}

const PreviewGenerator &PluginContainer::previewGenerator() const
{
  return m_PreviewGenerator;
}

void PluginContainer::loadPlugins()
{
  TimeThis tt("PluginContainer::loadPlugins()");

  unloadPlugins();

  for (QObject *plugin : QPluginLoader::staticInstances()) {
    registerPlugin(plugin, "", nullptr);
  }

  QFile loadCheck;
  QString skipPlugin;

  if (m_Organizer) {
    loadCheck.setFileName(qApp->property("dataPath").toString() + "/plugin_loadcheck.tmp");

    if (loadCheck.exists() && loadCheck.open(QIODevice::ReadOnly)) {
      // oh, there was a failed plugin load last time. Find out which plugin was loaded last
      QString fileName;
      while (!loadCheck.atEnd()) {
        fileName = QString::fromUtf8(loadCheck.readLine().constData()).trimmed();
      }

      log::warn("loadcheck file found for plugin '{}'", fileName);

      MOBase::TaskDialog dlg;

      const auto Skip = QMessageBox::Ignore;
      const auto Blacklist = QMessageBox::Cancel;
      const auto Load = QMessageBox::Ok;

      const auto r = dlg
        .title(tr("Plugin error"))
        .main(tr(
          "Mod Organizer failed to load the plugin '%1' last time it was started.")
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

      switch (r)
      {
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

  QString pluginPath = qApp->applicationDirPath() + "/" + ToQString(AppConfig::pluginPath());
  log::debug("looking for plugins in {}", QDir::toNativeSeparators(pluginPath));
  QDirIterator iter(pluginPath, QDir::Files | QDir::NoDotAndDotDot);

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

    QString pluginName = iter.filePath();
    if (QLibrary::isLibrary(pluginName)) {
      std::unique_ptr<QPluginLoader> pluginLoader(new QPluginLoader(pluginName, this));
      if (pluginLoader->instance() == nullptr) {
        m_FailedPlugins.push_back(pluginName);
        log::error(
          "failed to load plugin {}: {}",
          pluginName, pluginLoader->errorString());
      } else {
        if (registerPlugin(pluginLoader->instance(), pluginName, nullptr)) {
          log::debug("loaded plugin \"{}\"", QFileInfo(pluginName).fileName());
          m_PluginLoaders.push_back(pluginLoader.release());
        } else {
          m_FailedPlugins.push_back(pluginName);
          log::warn("plugin \"{}\" failed to load (may be outdated)", pluginName);
        }
      }
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
      QString result = tr("The following plugins could not be loaded. The reason may be missing "
                          "dependencies (i.e. python) or an outdated version:") + "<ul>";
      for (const QString &plugin : m_FailedPlugins) {
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

void PluginContainer::startGuidedFix(unsigned int) const
{
}
