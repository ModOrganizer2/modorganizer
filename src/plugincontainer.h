#ifndef PLUGINCONTAINER_H
#define PLUGINCONTAINER_H

#include "previewgenerator.h"

class OrganizerCore;
class IUserInterface;

#include <QFile>
#include <QPluginLoader>
#include <QtPlugin>
#include <iplugindiagnose.h>
#include <ipluginfilemapper.h>
#include <iplugingame.h>
#include <iplugininstaller.h>
#include <ipluginmodpage.h>
#include <ipluginproxy.h>
#include <iplugintool.h>
#ifndef Q_MOC_RUN
#include <boost/fusion/container.hpp>
#include <boost/fusion/include/at_key.hpp>
#include <boost/mp11.hpp>
#endif  // Q_MOC_RUN
#include <memory>
#include <vector>

class OrganizerProxy;

/**
 * @brief Class that wrap multiple requirements for a plugin together. THis
 *     class owns the requirements.
 */
class PluginRequirements
{
public:
  /**
   * @return true if the plugin can be enabled (all requirements are met).
   */
  bool canEnable() const;

  /**
   * @return true if this is a core plugin, i.e. a plugin that should not be
   *     manually enabled or disabled by the user.
   */
  bool isCorePlugin() const;

  /**
   * @return true if this plugin has requirements (satisfied or not).
   */
  bool hasRequirements() const;

  /**
   * @return the proxy that created this plugin, if any.
   */
  MOBase::IPluginProxy* proxy() const;

  /**
   * @return the list of plugins this plugin proxies (if it's a proxy plugin).
   */
  std::vector<MOBase::IPlugin*> proxied() const;

  /**
   * @return the master of this plugin, if any.
   */
  MOBase::IPlugin* master() const;

  /**
   * @return the plugins this plugin is master of.
   */
  std::vector<MOBase::IPlugin*> children() const;

  /**
   * @return the list of problems to be resolved before enabling the plugin.
   */
  std::vector<MOBase::IPluginRequirement::Problem> problems() const;

  /**
   * @return the name of the games (gameName()) this plugin can be used with, or an
   * empty list if this plugin does not require particular games.
   */
  QStringList requiredGames() const;

  /**
   * @return the list of plugins currently enabled that would have to be disabled
   *     if this plugin was disabled.
   */
  std::vector<MOBase::IPlugin*> requiredFor() const;

private:
  // The list of "Core" plugins.
  static const std::set<QString> s_CorePlugins;

  // Accumulator version for requiredFor() to avoid infinite recursion.
  void requiredFor(std::vector<MOBase::IPlugin*>& required,
                   std::set<MOBase::IPlugin*>& visited) const;

  // Retrieve the requirements from the underlying plugin, take ownership on them
  // and store them. We cannot do this in the constructor because we want to have a
  // constructed object before calling init().
  void fetchRequirements();

  // Set the master for this plugin. This is required to "fake" masters for proxied
  // plugins.
  void setMaster(MOBase::IPlugin* master);

  friend class OrganizerCore;
  friend class PluginContainer;

  PluginContainer* m_PluginContainer;
  MOBase::IPlugin* m_Plugin;
  MOBase::IPluginProxy* m_PluginProxy;
  MOBase::IPlugin* m_Master;
  std::vector<std::shared_ptr<const MOBase::IPluginRequirement>> m_Requirements;
  OrganizerProxy* m_Organizer;
  std::vector<MOBase::IPlugin*> m_RequiredFor;

  PluginRequirements(PluginContainer* pluginContainer, MOBase::IPlugin* plugin,
                     OrganizerProxy* proxy, MOBase::IPluginProxy* pluginProxy);
};

/**
 *
 */
class PluginContainer : public QObject, public MOBase::IPluginDiagnose
{

  Q_OBJECT
  Q_INTERFACES(MOBase::IPluginDiagnose)

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
      boost::fusion::pair<MOBase::IPluginProxy, std::vector<MOBase::IPluginProxy*>>,
      boost::fusion::pair<MOBase::IPluginFileMapper,
                          std::vector<MOBase::IPluginFileMapper*>>>;

  using AccessPluginMap = boost::fusion::map<
      boost::fusion::pair<MOBase::IPluginDiagnose,
                          std::map<MOBase::IPluginDiagnose*, MOBase::IPlugin*>>,
      boost::fusion::pair<MOBase::IPluginFileMapper,
                          std::map<MOBase::IPluginFileMapper*, MOBase::IPlugin*>>,
      boost::fusion::pair<QString, std::map<QString, MOBase::IPlugin*>>>;

  static const unsigned int PROBLEM_PLUGINSNOTLOADED = 1;

  /**
   * This typedefs defines the order of plugin interface. This is increasing order of
   * importance".
   *
   * @note IPlugin is the less important interface, followed by IPluginDiagnose and
   *     IPluginFileMapper as those are usually implemented together with another
   * interface. Other interfaces are in a alphabetical order since it is unlikely a
   * plugin will implement multiple ones.
   */
  using PluginTypeOrder = boost::mp11::mp_transform<
      std::add_pointer_t,
      boost::mp11::mp_list<
          MOBase::IPluginGame, MOBase::IPluginInstaller, MOBase::IPluginModPage,
          MOBase::IPluginPreview, MOBase::IPluginProxy, MOBase::IPluginTool,
          MOBase::IPluginDiagnose, MOBase::IPluginFileMapper, MOBase::IPlugin>>;

  static_assert(boost::mp11::mp_size<PluginTypeOrder>::value ==
                boost::mp11::mp_size<PluginContainer::PluginMap>::value - 1);

public:
  /**
   * @brief Retrieved the (localized) names of the various plugin interfaces.
   *
   * @return the (localized) names of the various plugin interfaces.
   */
  static QStringList pluginInterfaces();

public:
  PluginContainer(OrganizerCore* organizer);
  virtual ~PluginContainer();

  /**
   * @brief Start the plugins.
   *
   * This function should not be called before MO2 is ready and plugins can be
   * started, and will do the following:
   *   - connect the callbacks of the plugins,
   *   - set the parent widget for plugins that can have one,
   *   - notify plugins that MO2 has been started, including:
   *     - triggering a call to the "profile changed" callback for the initial profile,
   *     - triggering a call to the "user interface initialized" callback.
   *
   * @param userInterface The main user interface to use for the plugins.
   */
  void startPlugins(IUserInterface* userInterface);

  /**
   * @brief Load, unload or reload the plugin at the given path.
   *
   */
  void loadPlugin(QString const& filepath);
  void unloadPlugin(QString const& filepath);
  void reloadPlugin(QString const& filepath);

  /**
   * @brief Load all plugins.
   *
   */
  void loadPlugins();

  /**
   * @brief Retrieve the list of plugins of the given type.
   *
   * @return the list of plugins of the specified type.
   *
   * @tparam T The type of plugin to retrieve.
   */
  template <typename T>
  const std::vector<T*>& plugins() const
  {
    typename boost::fusion::result_of::at_key<const PluginMap, T>::type temp =
        boost::fusion::at_key<T>(m_Plugins);
    return temp;
  }

  /**
   * @brief Check if a plugin implement a given interface.
   *
   * @param plugin The plugin to check.
   *
   * @return true if the plugin implements the interface, false otherwise.
   *
   * @tparam The interface type.
   */
  template <typename T>
  bool implementInterface(MOBase::IPlugin* plugin) const
  {
    // We need a QObject to be able to qobject_cast<> to the plugin types:
    QObject* oPlugin = as_qobject(plugin);

    if (!oPlugin) {
      return false;
    }

    return qobject_cast<T*>(oPlugin);
  }

  /**
   * @brief Retrieve a plugin from its name or a corresponding non-IPlugin
   *     interface.
   *
   * @param t Name of the plugin to retrieve, or non-IPlugin interface.
   *
   * @return the corresponding plugin, or a null pointer.
   *
   * @note It is possible to have multiple plugins for the same name when
   *     dealing with proxied plugins (e.g. Python), in which case the
   *     most important one will be returned, as specified in PluginTypeOrder.
   */
  MOBase::IPlugin* plugin(QString const& pluginName) const;
  MOBase::IPlugin* plugin(MOBase::IPluginDiagnose* diagnose) const;
  MOBase::IPlugin* plugin(MOBase::IPluginFileMapper* mapper) const;

  /**
   * @brief Find the game plugin corresponding to the given name.
   *
   * @param name The name of the game to find a plugin for (as returned by
   *     IPluginGame::gameName()).
   *
   * @return the game plugin for the given name, or a null pointer if no
   *     plugin exists for this game.
   */
  MOBase::IPluginGame* game(const QString& name) const;

  /**
   * @return the IPlugin interface to the currently managed game.
   */
  MOBase::IPlugin* managedGame() const;

  /**
   * @brief Check if the given plugin is enabled.
   *
   * @param plugin The plugin to check.
   *
   * @return true if the plugin is enabled, false otherwise.
   */
  bool isEnabled(MOBase::IPlugin* plugin) const;

  // These are friendly methods that called isEnabled(plugin(arg)).
  bool isEnabled(QString const& pluginName) const;
  bool isEnabled(MOBase::IPluginDiagnose* diagnose) const;
  bool isEnabled(MOBase::IPluginFileMapper* mapper) const;

  /**
   * @brief Enable or disable a plugin.
   *
   * @param plugin The plugin to enable or disable.
   * @param enable true to enable, false to disable.
   * @param dependencies If true and enable is false, dependencies will also
   *     be disabled (see PluginRequirements::requiredFor).
   */
  void setEnabled(MOBase::IPlugin* plugin, bool enable, bool dependencies = true);

  /**
   * @brief Retrieve the requirements for the given plugin.
   *
   * @param plugin The plugin to retrieve the requirements for.
   *
   * @return the requirements (as proxy) for the given plugin.
   */
  const PluginRequirements& requirements(MOBase::IPlugin* plugin) const;

  /**
   * @brief Retrieved the (localized) names of interfaces implemented by the given
   *     plugin.
   *
   * @param plugin The plugin to retrieve interface for.
   *
   * @return the (localized) names of interfaces implemented by this plugin.
   */
  QStringList implementedInterfaces(MOBase::IPlugin* plugin) const;

  /**
   * @brief Return the (localized) name of the most important interface implemented by
   *     the given plugin.
   *
   * The order of interfaces is defined in X.
   *
   * @param plugin The plugin to retrieve the interface for.
   *
   * @return the (localized) name of the most important interface implemented by this
   * plugin.
   */
  QString topImplementedInterface(MOBase::IPlugin* plugin) const;

  /**
   * @return the preview generator.
   */
  const PreviewGenerator& previewGenerator() const;

  /**
   * @return the list of plugin file names, including proxied plugins.
   */
  QStringList pluginFileNames() const;

public:  // IPluginDiagnose interface
  virtual std::vector<unsigned int> activeProblems() const;
  virtual QString shortDescription(unsigned int key) const;
  virtual QString fullDescription(unsigned int key) const;
  virtual bool hasGuidedFix(unsigned int key) const;
  virtual void startGuidedFix(unsigned int key) const;

signals:

  /**
   * @brief Emitted when plugins are enabled or disabled.
   */
  void pluginEnabled(MOBase::IPlugin*);
  void pluginDisabled(MOBase::IPlugin*);

  /**
   * @brief Emitted when plugins are registered or unregistered.
   */
  void pluginRegistered(MOBase::IPlugin*);
  void pluginUnregistered(MOBase::IPlugin*);

  void diagnosisUpdate();

private:
  friend class PluginRequirements;

  // Unload all the plugins.
  void unloadPlugins();

  // Retrieve the organizer proxy for the given plugin.
  OrganizerProxy* organizerProxy(MOBase::IPlugin* plugin) const;

  // Retrieve the proxy plugin that instantiated the given plugin, or a null pointer
  // if the plugin was not instantiated by a proxy.
  MOBase::IPluginProxy* pluginProxy(MOBase::IPlugin* plugin) const;

  // Retrieve the path to the file or folder corresponding to the plugin.
  QString filepath(MOBase::IPlugin* plugin) const;

  // Load plugins from the given filepath using the given proxy.
  std::vector<QObject*> loadProxied(const QString& filepath,
                                    MOBase::IPluginProxy* proxy);

  // Load the Qt plugin from the given file.
  QObject* loadQtPlugin(const QString& filepath);

  // check if a plugin is folder containing a Qt plugin, it is, return the path to the
  // DLL containing the plugin in the folder, otherwise return an empty optional
  //
  // a Qt plugin folder is a folder with a DLL containing a library (not in a
  // subdirectory), if multiple plugins are present, only the first one is returned
  //
  // extra DLLs are ignored by Qt so can be present in the folder
  //
  std::optional<QString> isQtPluginFolder(const QString& filepath) const;

  // See startPlugins for more details. This is simply an intermediate function
  // that can be used when loading plugins after initialization. This uses the
  // user interface in m_UserInterface.
  void startPluginsImpl(const std::vector<QObject*>& plugins) const;

  /**
   * @brief Unload the given plugin.
   *
   * This function is not public because it's kind of dangerous trying to unload
   * plugin directly since some plugins are linked together.
   *
   * @param plugin The plugin to unload/unregister.
   * @param object The QObject corresponding to the plugin.
   */
  void unloadPlugin(MOBase::IPlugin* plugin, QObject* object);

  /**
   * @brief Retrieved the (localized) names of interfaces implemented by the given
   *     plugin.
   *
   * @param plugin The plugin to retrieve interface for.
   *
   * @return the (localized) names of interfaces implemented by this plugin.
   *
   * @note This function can be used to get implemented interfaces before registering
   *     a plugin.
   */
  QStringList implementedInterfaces(QObject* plugin) const;

  /**
   * @brief Check if a plugin implements a "better" interface than another
   *     one, as specified by PluginTypeOrder.
   *
   * @param lhs, rhs The plugin to compare.
   *
   * @return true if the left plugin implements a better interface than the right
   *     one, false otherwise (or if both implements the same interface).
   */
  bool isBetterInterface(QObject* lhs, QObject* rhs) const;

  /**
   * @brief Find the QObject* corresponding to the given plugin.
   *
   * @param plugin The plugin to find the QObject* for.
   *
   * @return a QObject* for the given plugin.
   */
  QObject* as_qobject(MOBase::IPlugin* plugin) const;

  /**
   * @brief Initialize a plugin.
   *
   * @param plugin The plugin to initialize.
   * @param proxy The proxy that created this plugin (can be null).
   * @param skipInit If true, IPlugin::init() will not be called, regardless
   *     of the state of the container.
   *
   * @return true if the plugin was initialized correctly, false otherwise.
   */
  bool initPlugin(MOBase::IPlugin* plugin, MOBase::IPluginProxy* proxy, bool skipInit);

  void registerGame(MOBase::IPluginGame* game);
  void unregisterGame(MOBase::IPluginGame* game);

  MOBase::IPlugin* registerPlugin(QObject* pluginObj, const QString& fileName,
                                  MOBase::IPluginProxy* proxy);

  // Core organizer, can be null (e.g. on first MO2 startup).
  OrganizerCore* m_Organizer;

  // Main user interface, can be null until MW has been initialized.
  IUserInterface* m_UserInterface;

  PluginMap m_Plugins;

  // This maps allow access to IPlugin* from name or diagnose/mapper object.
  AccessPluginMap m_AccessPlugins;

  std::map<MOBase::IPlugin*, PluginRequirements> m_Requirements;

  std::map<QString, MOBase::IPluginGame*> m_SupportedGames;
  QStringList m_FailedPlugins;
  std::vector<QPluginLoader*> m_PluginLoaders;

  PreviewGenerator m_PreviewGenerator;

  QFile m_PluginsCheck;
};

#endif  // PLUGINCONTAINER_H
