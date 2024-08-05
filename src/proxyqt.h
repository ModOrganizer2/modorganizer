#ifndef PROXYQTLOADER_H
#define PROXYQTLOADER_H

#include <map>

#include <uibase/extensions/ipluginloader.h>

class ProxyQtLoader : public MOBase::IPluginLoader
{
  Q_OBJECT
  Q_INTERFACES(MOBase::IPluginLoader)
  Q_PLUGIN_METADATA(IID "org.mo2.ProxyQt")

public:
  ProxyQtLoader();

  bool initialize(QString& errorMessage) override;
  QList<QList<QObject*>> load(const MOBase::PluginExtension& extension) override;
  void unload(const MOBase::PluginExtension& extension) override;
  void unloadAll() override;

private:
  struct QPluginLoaderDeleter
  {
    void operator()(QPluginLoader*) const;
  };
  using QPluginLoaderPtr = std::unique_ptr<QPluginLoader, QPluginLoaderDeleter>;

  std::map<const MOBase::PluginExtension*, std::vector<QPluginLoaderPtr>> m_loaders;
};

#endif
