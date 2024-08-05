#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include <vector>

#ifndef Q_MOC_RUN
#include <boost/fusion/container.hpp>
#include <boost/fusion/include/at_key.hpp>
#include <boost/mp11.hpp>
#endif  // Q_MOC_RUN

#include <QObject>

#include <uibase/extensions/extension.h>
#include <uibase/extensions/ipluginloader.h>
#include <uibase/iplugin.h>
#include <uibase/iplugindiagnose.h>
#include <uibase/ipluginfilemapper.h>
#include <uibase/iplugingame.h>
#include <uibase/iplugininstaller.h>
#include <uibase/ipluginmodpage.h>
#include <uibase/ipluginpreview.h>
#include <uibase/iplugintool.h>

#include "game_features.h"
#include "previewgenerator.h"

class ExtensionManager;
class IUserInterface;
class OrganizerCore;
class OrganizerProxy;
class PluginManager;

// class containing extra useful information for plugins
//
class PluginDetails
{
public:
  // check if the plugin can be enabled (all requirements are met)
  //
  bool canEnable() const;

  // check if this plugin has requirements (satisfied or not)
  //
  bool hasRequirements() const;

  // the organizer proxy for this plugin
  //
  const auto* proxy() const { return m_organizer; }

  // the "master" of the group this plugin belongs to
  //
  MOBase::IPlugin* master() const { return m_master; }

  // the extension containing this plugin
  //
  const MOBase::PluginExtension& extension() const { return *m_extension; }

  // retrieve the list of problems to be resolved before enabling the plugin
  //
  std::vector<MOBase::IPluginRequirement::Problem> problems() const;

  // retrieve the name of the games (gameName()) this plugin can be used with, or an
  // empty list if this plugin does not require particular games.
  //
  QStringList requiredGames() const;

private:
  // retrieve the requirements from the underlying plugin, take ownership on them and
  // store them
  //
  // we cannot do this in the constructor because we want to have a constructed object
  // before calling init()
  //
  void fetchRequirements();

  friend class PluginManager;

  PluginManager* m_manager;
  MOBase::IPlugin* m_plugin;
  const MOBase::PluginExtension* m_extension;
  MOBase::IPlugin* m_master;
  std::vector<std::shared_ptr<const MOBase::IPluginRequirement>> m_requirements;
  OrganizerProxy* m_organizer;

  PluginDetails(PluginManager* manager, MOBase::PluginExtension const& extension,
                MOBase::IPlugin* plugin, OrganizerProxy* proxy);
};

// manager for plugins
//
class PluginManager : public QObject
{
  Q_OBJECT
public:
  // retrieve the (localized) names of the various plugin interfaces
  //
  static QStringList pluginInterfaces();

public:
  PluginManager(ExtensionManager const& manager, OrganizerCore* core);

public:  // access
  // retrieve the list of plugins of a given type
  //
  // - if no type is specified, return the list of all plugins as IPlugin
  // - if IPlugin is specified, returns only plugins that only extends IPlugin
  //
  //
  template <typename T = void>
  const auto& plugins() const
  {
    if constexpr (std::is_void_v<T>) {

      return m_allPlugins;
    } else {
      return boost::fusion::at_key<T>(m_plugins);
    }
  }

  // retrieve the details for the given plugin
  //
  const auto& details(MOBase::IPlugin* plugin) const { return m_details.at(plugin); }

  // retrieve the (localized) names of interfaces implemented by the given plugin
  //
  QStringList implementedInterfaces(MOBase::IPlugin* plugin) const;

  // retrieve the (localized) name of the most important interface implemented by the
  // given plugin
  //
  QString topImplementedInterface(MOBase::IPlugin* plugin) const;

  // retrieve a plugin from its name or a corresponding non-IPlugin interface
  //
  MOBase::IPlugin* plugin(QString const& pluginName) const;
  MOBase::IPlugin* plugin(MOBase::IPluginDiagnose* diagnose) const;
  MOBase::IPlugin* plugin(MOBase::IPluginFileMapper* mapper) const;

  // find the game plugin corresponding to the given name, returns a null pointer if no
  // game exists
  //
  MOBase::IPluginGame* game(const QString& name) const;

  // retrieve the IPlugin interface to the currently managed game.
  //
  MOBase::IPluginGame* managedGame() const;

  // retrieve the game features
  //
  GameFeatures& gameFeatures() const { return *m_gameFeatures; }

  // retrieve the preview generator
  //
  const PreviewGenerator& previewGenerator() const { return *m_previews; }

public:  // checks
  // check if a plugin implement a given plugin interface
  //
  template <typename T>
  bool implementInterface(MOBase::IPlugin* plugin) const
  {
    // we need a QObject to be able to qobject_cast<> to the plugin types
    QObject* oPlugin = as_qobject(plugin);

    if (!oPlugin) {
      return false;
    }

    return qobject_cast<T*>(oPlugin);
  }

  // check if a plugin is enabled
  //
  bool isEnabled(MOBase::IPlugin* plugin) const;
  bool isEnabled(QString const& pluginName) const;
  bool isEnabled(MOBase::IPluginDiagnose* diagnose) const;
  bool isEnabled(MOBase::IPluginFileMapper* mapper) const;

public:  // load
  // load all plugins from the extension manager
  //
  void loadPlugins();

  // load plugins from the given extension
  //
  bool loadPlugins(const MOBase::PluginExtension& extension);
  bool unloadPlugins(const MOBase::PluginExtension& extension);
  bool reloadPlugins(const MOBase::PluginExtension& extension);

  // start the plugins
  //
  // this function should not be called before MO2 is ready and plugins can be started,
  // and will do the following:
  // - connect the callbacks of the plugins,
  // - set the parent widget for plugins that can have one,
  // - notify plugins that MO2 has been started, including:
  // - triggering a call to the "profile changed" callback for the initial profile,
  // - triggering a call to the "user interface initialized" callback.
  //
  void startPlugins(IUserInterface* userInterface);

signals:

  // emitted when plugins are enabled or disabled
  //
  void pluginEnabled(MOBase::IPlugin*);
  void pluginDisabled(MOBase::IPlugin*);

  // emitted when plugins are registered or unregistered
  //
  void pluginRegistered(MOBase::IPlugin*);
  void pluginUnregistered(MOBase::IPlugin*);

  // enmitted when a diagnose plugin invalidates() itself
  //
  void diagnosePluginInvalidated(MOBase::IPluginDiagnose*);

private:
  friend class PluginDetails;

private:
  using PluginMap = boost::fusion::map<
      boost::fusion::pair<QObject, std::vector<QObject*>>,
      boost::fusion::pair<MOBase::IPlugin, std::vector<MOBase::IPlugin*>>,
      boost::fusion::pair<MOBase::IPluginDiagnose,
                          std::vector<MOBase::IPluginDiagnose*>>,
      boost::fusion::pair<MOBase::IPluginGame, std::vector<MOBase::IPluginGame*>>,
      boost::fusion::pair<MOBase::IPluginInstaller,
                          std::vector<MOBase::IPluginInstaller*>>,
      boost::fusion::pair<MOBase::IPluginModPage, std::vector<MOBase::IPluginModPage*>>,
      boost::fusion::pair<MOBase::IPluginPreview, std::vector<MOBase::IPluginPreview*>>,
      boost::fusion::pair<MOBase::IPluginTool, std::vector<MOBase::IPluginTool*>>,
      boost::fusion::pair<MOBase::IPluginFileMapper,
                          std::vector<MOBase::IPluginFileMapper*>>>;

  using AccessPluginMap = boost::fusion::map<
      boost::fusion::pair<MOBase::IPluginDiagnose,
                          std::map<MOBase::IPluginDiagnose*, MOBase::IPlugin*>>,
      boost::fusion::pair<MOBase::IPluginFileMapper,
                          std::map<MOBase::IPluginFileMapper*, MOBase::IPlugin*>>,
      boost::fusion::pair<QString, std::map<QString, MOBase::IPlugin*>>>;

  // type defining the order of plugin interface, in increasing order of importance
  //
  // IPlugin is the less important interface, followed by IPluginDiagnose and
  // IPluginFileMapper as those are usually implemented together with another interface
  //
  // other interfaces are in a alphabetical order since it is unlikely a plugin will
  // implement multiple ones
  //
  using PluginTypeOrder = boost::mp11::mp_transform<
      std::add_pointer_t,
      boost::mp11::mp_list<MOBase::IPluginGame, MOBase::IPluginInstaller,
                           MOBase::IPluginModPage, MOBase::IPluginPreview,
                           MOBase::IPluginTool, MOBase::IPluginDiagnose,
                           MOBase::IPluginFileMapper, MOBase::IPlugin>>;
  static_assert(boost::mp11::mp_size<PluginTypeOrder>::value ==
                boost::mp11::mp_size<PluginMap>::value - 1);

private:
  // retrieve the (localized) names of interfaces implemented by the given plugin
  //
  // this function can be used to get implemented interfaces before registering a plugin
  //
  QStringList implementedInterfaces(QObject* plugin) const;

  // check if the left plugin implements a "better" interface than the right one, as
  // specified by PluginTypeOrder
  //
  bool isBetterInterface(QObject* lhs, QObject* rhs) const;

  // find the QObject* corresponding to the given plugin
  //
  QObject* as_qobject(MOBase::IPlugin* plugin) const;

  // see startPlugins for more details
  //
  // this is simply an intermediate function that can be used when loading plugins after
  // initialization which uses the user interface in m_userInterface
  //
  void startPluginsImpl(const std::vector<QObject*>& plugins) const;

  // unload the given plugin
  //
  // this function is not public because it's kind of dangerous trying to unload plugin
  // directly since some plugins are linked together
  //
  void unloadPlugin(MOBase::IPlugin* plugin, QObject* object);

  // unload all plugins
  //
  void unloadPlugins();

  // register/unregister a game plugin
  //
  void registerGame(MOBase::IPluginGame* game);
  void unregisterGame(MOBase::IPluginGame* game);

  // initialize a plugin and creates approriate PluginDetails for it
  //
  bool initPlugin(MOBase::PluginExtension const& extension, MOBase::IPlugin* plugin,
                  bool skipInit);

  // register a plugin for the given extension
  //
  MOBase::IPlugin* registerPlugin(const MOBase::PluginExtension& extension,
                                  QObject* pluginObj,
                                  QList<QObject*> const& pluginGroup);

private:
  struct PluginLoaderDeleter
  {
    PluginLoaderDeleter(QPluginLoader* qPluginLoader = nullptr);

    void operator()(MOBase::IPluginLoader* loader) const;

  private:
    QPluginLoader* m_qPluginLoader;
  };

  using PluginLoaderPtr = std::unique_ptr<MOBase::IPluginLoader, PluginLoaderDeleter>;

  // create the loaders
  //
  std::vector<PluginLoaderPtr> makeLoaders();

private:
  const ExtensionManager& m_extensions;

  // core organizer, can be null (e.g. on first MO2 startup).
  OrganizerCore* m_core;

  // main user interface, can be null until MW has been initialized.
  IUserInterface* m_userInterface;

  // Game features
  std::unique_ptr<GameFeatures> m_gameFeatures;

  // plugin loaders
  std::vector<PluginLoaderPtr> m_loaders;

  PluginMap m_plugins;
  std::vector<MOBase::IPlugin*> m_allPlugins;

  // this maps allow access to IPlugin* from name or diagnose/mapper object, and from
  // game
  AccessPluginMap m_accessPlugins;
  std::map<QString, MOBase::IPluginGame*> m_supportedGames;

  // details for plugins
  std::map<MOBase::IPlugin*, PluginDetails> m_details;

  // the preview generator
  PreviewGenerator* m_previews;
};

#endif
