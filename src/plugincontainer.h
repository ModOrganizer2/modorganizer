#ifndef PLUGINCONTAINER_H
#define PLUGINCONTAINER_H

#include "previewgenerator.h"

class OrganizerCore;
class IUserInterface;

#include <iplugindiagnose.h>
#include <ipluginmodpage.h>
#include <iplugingame.h>
#include <iplugintool.h>
#include <ipluginproxy.h>
#include <iplugininstaller.h>
#include <ipluginfilemapper.h>
#include <QtPlugin>
#include <QPluginLoader>
#include <QFile>
#ifndef Q_MOC_RUN
#include <boost/fusion/container.hpp>
#include <boost/fusion/include/at_key.hpp>
#endif // Q_MOC_RUN
#include <vector>


class PluginContainer : public QObject, public MOBase::IPluginDiagnose
{

  Q_OBJECT
  Q_INTERFACES(MOBase::IPluginDiagnose)

private:

  typedef boost::fusion::map<
  boost::fusion::pair<QObject, std::vector<QObject*>>,
  boost::fusion::pair<MOBase::IPlugin, std::vector<MOBase::IPlugin*>>,
  boost::fusion::pair<MOBase::IPluginDiagnose, std::vector<MOBase::IPluginDiagnose*>>,
  boost::fusion::pair<MOBase::IPluginGame, std::vector<MOBase::IPluginGame*>>,
  boost::fusion::pair<MOBase::IPluginInstaller, std::vector<MOBase::IPluginInstaller*>>,
  boost::fusion::pair<MOBase::IPluginModPage, std::vector<MOBase::IPluginModPage*>>,
  boost::fusion::pair<MOBase::IPluginPreview, std::vector<MOBase::IPluginPreview*>>,
  boost::fusion::pair<MOBase::IPluginTool, std::vector<MOBase::IPluginTool*>>,
  boost::fusion::pair<MOBase::IPluginProxy, std::vector<MOBase::IPluginProxy*>>,
  boost::fusion::pair<MOBase::IPluginFileMapper, std::vector<MOBase::IPluginFileMapper*>>
  > PluginMap;

  static const unsigned int PROBLEM_PLUGINSNOTLOADED = 1;

public:

  /**
   * @brief Retrieved the (localized) names of interfaces implemented by the given
   *     plugin.
   *
   * @param plugin The plugin to retrieve interface for.
   *
   * @return the (localized) names of interfaces implemented by this plugin.
   */
  QStringList implementedInterfaces(const MOBase::IPlugin *plugin) const;

  PluginContainer(OrganizerCore *organizer);
  virtual ~PluginContainer();

  void setUserInterface(IUserInterface *userInterface, QWidget *widget);

  void loadPlugins();
  void unloadPlugins();

  /**
   * @brief Find the game plugin corresponding to the given name.
   *
   * @param name The name of the game to find a plugin for (as returned by
   *     IPluginGame::gameName()).
   *
   * @return the game plugin for the given name, or a null pointer if no
   *     plugin exists for this game.
   */
  MOBase::IPluginGame *managedGame(const QString &name) const;

  /**
   * @brief Retrieve the list of plugins of the given type.
   *
   * @return the list of plugins of the specified type.
   *
   * @tparam T The type of plugin to retrieve.
   */
  template <typename T>
  const std::vector<T*> &plugins() const {
    typename boost::fusion::result_of::at_key<const PluginMap, T>::type temp = boost::fusion::at_key<T>(m_Plugins);
    return temp;
  }

  /**
   * @return the preview generator.
   */
  const PreviewGenerator &previewGenerator() const;

  /**
   * @return the list of plugin file names, including proxied plugins.
   */
  QStringList pluginFileNames() const;

public: // IPluginDiagnose interface

  virtual std::vector<unsigned int> activeProblems() const;
  virtual QString shortDescription(unsigned int key) const;
  virtual QString fullDescription(unsigned int key) const;
  virtual bool hasGuidedFix(unsigned int key) const;
  virtual void startGuidedFix(unsigned int key) const;

signals:

  void diagnosisUpdate();

private:

  bool verifyPlugin(MOBase::IPlugin *plugin);
  void registerGame(MOBase::IPluginGame *game);
  bool registerPlugin(QObject *pluginObj, const QString &fileName);
  bool unregisterPlugin(QObject *pluginObj, const QString &fileName);

  OrganizerCore *m_Organizer;

  IUserInterface *m_UserInterface;

  PluginMap m_Plugins;

  std::map<QString, MOBase::IPluginGame*> m_SupportedGames;
  std::vector<boost::signals2::connection> m_DiagnosisConnections;
  QStringList m_FailedPlugins;
  std::vector<QPluginLoader*> m_PluginLoaders;

  PreviewGenerator m_PreviewGenerator;

  QFile m_PluginsCheck;
};


#endif // PLUGINCONTAINER_H
