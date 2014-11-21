#ifndef PLUGINCONTAINER_H
#define PLUGINCONTAINER_H


#include "organizercore.h"
#include "previewgenerator.h"
#include <iplugindiagnose.h>
#include <ipluginmodpage.h>
#include <iplugingame.h>
#include <iplugintool.h>
#include <ipluginproxy.h>
#include <iplugininstaller.h>
#include <QtPlugin>
#include <QPluginLoader>
#include <QFile>
#ifndef Q_MOC_RUN
#include <boost/fusion/container.hpp>
#endif // Q_MOC_RUN
#include <vector>


class PluginContainer : public QObject, public MOBase::IPluginDiagnose
{

  Q_OBJECT
  Q_INTERFACES(MOBase::IPluginDiagnose)

public:

  PluginContainer(OrganizerCore *organizer);

  void setUserInterface(IUserInterface *userInterface, QWidget *widget);

  void loadPlugins();
  void unloadPlugins();

  MOBase::IPluginGame *managedGame(const QString &name) const;

  template <typename T>
  std::vector<T*> plugins() const {
    return boost::fusion::at_key<T>(m_Plugins);
  }

  const PreviewGenerator &previewGenerator() const;

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
  void registerPluginTool(MOBase::IPluginTool *tool);
  void registerModPage(MOBase::IPluginModPage *modPage);
  void registerGame(MOBase::IPluginGame *game);
  bool registerPlugin(QObject *pluginObj, const QString &fileName);
  bool unregisterPlugin(QObject *pluginObj, const QString &fileName);

private:

  typedef boost::fusion::map<
  boost::fusion::pair<MOBase::IPlugin, std::vector<MOBase::IPlugin*>>,
  boost::fusion::pair<MOBase::IPluginDiagnose, std::vector<MOBase::IPluginDiagnose*>>,
  boost::fusion::pair<MOBase::IPluginGame, std::vector<MOBase::IPluginGame*>>,
  boost::fusion::pair<MOBase::IPluginInstaller, std::vector<MOBase::IPluginInstaller*>>,
  boost::fusion::pair<MOBase::IPluginModPage, std::vector<MOBase::IPluginModPage*>>,
  boost::fusion::pair<MOBase::IPluginPreview, std::vector<MOBase::IPluginPreview*>>,
  boost::fusion::pair<MOBase::IPluginTool, std::vector<MOBase::IPluginTool*>>,
  boost::fusion::pair<MOBase::IPluginProxy, std::vector<MOBase::IPluginProxy*>>
  > PluginMap;

  static const unsigned int PROBLEM_PLUGINSNOTLOADED = 1;

private:

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
