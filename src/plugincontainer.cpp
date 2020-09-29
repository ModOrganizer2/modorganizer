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

template <> struct PluginTypeName<QObject> { static QString value() { return {}; } };
template <> struct PluginTypeName<MOBase::IPlugin> { static QString value() { return QT_TR_NOOP("Plugin"); } };
template <> struct PluginTypeName<MOBase::IPluginDiagnose> { static QString value() { return QT_TR_NOOP("Diagnose"); } };
template <> struct PluginTypeName<MOBase::IPluginGame> { static QString value() { return QT_TR_NOOP("Game"); } };
template <> struct PluginTypeName<MOBase::IPluginInstaller> { static QString value() { return QT_TR_NOOP("Installer"); } };
template <> struct PluginTypeName<MOBase::IPluginModPage> { static QString value() { return QT_TR_NOOP("Mod Page"); } };
template <> struct PluginTypeName<MOBase::IPluginPreview> { static QString value() { return QT_TR_NOOP("Preview"); } };
template <> struct PluginTypeName<MOBase::IPluginTool> { static QString value() { return QT_TR_NOOP("Tool"); } };
template <> struct PluginTypeName<MOBase::IPluginProxy> { static QString value() { return QT_TR_NOOP("Proxy"); } };
template <> struct PluginTypeName<MOBase::IPluginFileMapper> { static QString value() { return QT_TR_NOOP("File Mapper"); } };

QStringList PluginContainer::implementedInterfaces(const IPlugin* plugin) const
{
  // Find the correspond QObject - Can this be done safely with a cast?
  auto& objects = bf::at_key<QObject>(m_Plugins);
  auto it = std::find_if(std::begin(objects), std::end(objects), [plugin](QObject *obj) {
    return qobject_cast<IPlugin*>(obj) == plugin;
  });

  if (it == std::end(objects)) {
    return {};
  }

  // We need a QObject to be able to qobject_cast<> to the plugin types:
  const QObject* oPlugin = *it;

  // Find all the names:
  QStringList names;
  bf::for_each(m_Plugins, [oPlugin, &names](auto const& p) {
    using key_type = typename std::decay_t<decltype(p)>::first_type;
    auto name = PluginTypeName<key_type>::value();
    if (!name.isEmpty()) {
      names.append(name);
    }
  });

  // If the plugin implements at least one interface other than IPlugin, remove IPlugin:
  if (names.size() > 1) {
    names.removeAll(PluginTypeName<IPlugin>::value());
  }

  return names;
}

PluginContainer::PluginContainer(OrganizerCore *organizer)
  : m_Organizer(organizer)
  , m_UserInterface(nullptr)
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

    for (IPluginTool *tool : bf::at_key<IPluginTool>(m_Plugins)) {
      userInterface->registerPluginTool(tool);
    }
  }

  m_UserInterface = userInterface;
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


bool PluginContainer::verifyPlugin(IPlugin *plugin)
{
  if (plugin == nullptr) {
    return false;
  } else if (!plugin->init(new OrganizerProxy(m_Organizer, this, plugin->name()))) {
    log::warn("plugin failed to initialize");
    return false;
  }
  return true;
}


void PluginContainer::registerGame(IPluginGame *game)
{
  m_SupportedGames.insert({ game->gameName(), game });
}


bool PluginContainer::registerPlugin(QObject *plugin, const QString &fileName)
{
  // Storing the original QObject* is a bit of a hack as I couldn't figure out any
  // way to cast directly between IPlugin* and IPluginDiagnose*
  bf::at_key<QObject>(m_Plugins).push_back(plugin);

  { // generic treatment for all plugins
    IPlugin *pluginObj = qobject_cast<IPlugin*>(plugin);
    if (pluginObj == nullptr) {
      log::debug("not an IPlugin");
      return false;
    }
    plugin->setProperty("filename", fileName);
    m_Organizer->settings().plugins().registerPlugin(pluginObj);
  }

  { // diagnosis plugin
    IPluginDiagnose *diagnose = qobject_cast<IPluginDiagnose*>(plugin);
    if (diagnose != nullptr) {
      bf::at_key<IPluginDiagnose>(m_Plugins).push_back(diagnose);
      m_DiagnosisConnections.push_back(
            diagnose->onInvalidated([&] () { emit diagnosisUpdate(); })
            );
    }
  }
  { // file mapper plugin
    IPluginFileMapper *mapper = qobject_cast<IPluginFileMapper*>(plugin);
    if (mapper != nullptr) {
      bf::at_key<IPluginFileMapper>(m_Plugins).push_back(mapper);
    }
  }
  { // mod page plugin
    IPluginModPage *modPage = qobject_cast<IPluginModPage*>(plugin);
    if (verifyPlugin(modPage)) {
      bf::at_key<IPluginModPage>(m_Plugins).push_back(modPage);
      return true;
    }
  }
  { // game plugin
    IPluginGame *game = qobject_cast<IPluginGame*>(plugin);
    if (verifyPlugin(game)) {
      bf::at_key<IPluginGame>(m_Plugins).push_back(game);
      registerGame(game);
      return true;
    }
  }
  { // tool plugins
    IPluginTool *tool = qobject_cast<IPluginTool*>(plugin);
    if (verifyPlugin(tool)) {
      bf::at_key<IPluginTool>(m_Plugins).push_back(tool);
      return true;
    }
  }
  { // installer plugins
    IPluginInstaller *installer = qobject_cast<IPluginInstaller*>(plugin);
    if (verifyPlugin(installer)) {
      bf::at_key<IPluginInstaller>(m_Plugins).push_back(installer);
      m_Organizer->installationManager()->registerInstaller(installer);
      return true;
    }
  }
  { // preview plugins
    IPluginPreview *preview = qobject_cast<IPluginPreview*>(plugin);
    if (verifyPlugin(preview)) {
      bf::at_key<IPluginPreview>(m_Plugins).push_back(preview);
      m_PreviewGenerator.registerPlugin(preview);
      return true;
    }
  }
  { // proxy plugins
    IPluginProxy *proxy = qobject_cast<IPluginProxy*>(plugin);
    if (verifyPlugin(proxy)) {
      bf::at_key<IPluginProxy>(m_Plugins).push_back(proxy);
      QStringList pluginNames = proxy->pluginList(
            QCoreApplication::applicationDirPath() + "/" + ToQString(AppConfig::pluginPath()));
      for (const QString &pluginName : pluginNames) {
        try {
          // we get a list of matching plugins as proxies don't necessarily have a good way of supporting multiple inheritance
          QList<QObject*> matchingPlugins = proxy->instantiate(pluginName);
          for (QObject *proxiedPlugin : matchingPlugins) {
            if (proxiedPlugin != nullptr) {
              if (registerPlugin(proxiedPlugin, pluginName)) {
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
    if (verifyPlugin(dummy)) {
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
  if (m_Organizer != nullptr) {
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
    registerPlugin(plugin, "");
  }

  QFile loadCheck(qApp->property("dataPath").toString() + "/plugin_loadcheck.tmp");
  if (loadCheck.exists() && loadCheck.open(QIODevice::ReadOnly)) {
    // oh, there was a failed plugin load last time. Find out which plugin was loaded last
    QString fileName;
    while (!loadCheck.atEnd()) {
      fileName = QString::fromUtf8(loadCheck.readLine().constData()).trimmed();
    }
    if (QMessageBox::question(nullptr, QObject::tr("Plugin error"),
      QObject::tr("It appears the plugin \"%1\" failed to load last startup and caused MO to crash. Do you want to disable it?\n"
         "(Please note: If this is the first time you see this message for this plugin you may want to give it another try. "
         "The plugin may be able to recover from the problem)").arg(fileName),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
      m_Organizer->settings().plugins().addBlacklist(fileName);
    }
    loadCheck.close();
  }

  loadCheck.open(QIODevice::WriteOnly);

  QString pluginPath = qApp->applicationDirPath() + "/" + ToQString(AppConfig::pluginPath());
  log::debug("looking for plugins in {}", QDir::toNativeSeparators(pluginPath));
  QDirIterator iter(pluginPath, QDir::Files | QDir::NoDotAndDotDot);

  while (iter.hasNext()) {
    iter.next();
    if (m_Organizer->settings().plugins().blacklisted(iter.fileName())) {
      log::debug("plugin \"{}\" blacklisted", iter.fileName());
      continue;
    }
    loadCheck.write(iter.fileName().toUtf8());
    loadCheck.write("\n");
    loadCheck.flush();
    QString pluginName = iter.filePath();
    if (QLibrary::isLibrary(pluginName)) {
      std::unique_ptr<QPluginLoader> pluginLoader(new QPluginLoader(pluginName, this));
      if (pluginLoader->instance() == nullptr) {
        m_FailedPlugins.push_back(pluginName);
        log::error(
          "failed to load plugin {}: {}",
          pluginName, pluginLoader->errorString());
      } else {
        if (registerPlugin(pluginLoader->instance(), pluginName)) {
          log::debug("loaded plugin \"{}\"", QFileInfo(pluginName).fileName());
          m_PluginLoaders.push_back(pluginLoader.release());
        } else {
          m_FailedPlugins.push_back(pluginName);
          log::warn("plugin \"{}\" failed to load (may be outdated)", pluginName);
        }
      }
    }
  }

  // remove the load check file on success
  loadCheck.remove();

  bf::at_key<IPluginDiagnose>(m_Plugins).push_back(m_Organizer);
  bf::at_key<IPluginDiagnose>(m_Plugins).push_back(this);

  m_Organizer->connectPlugins(this);
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
