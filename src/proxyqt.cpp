#include "proxyqt.h"

#include <log.h>

using namespace MOBase;

void ProxyQtLoader::QPluginLoaderDeleter::operator()(QPluginLoader* loader) const
{
  if (loader) {
    loader->unload();
    delete loader;
  }
}

ProxyQtLoader::ProxyQtLoader() {}

bool ProxyQtLoader::initialize(QString& errorMessage)
{
  return true;
}

QList<QList<QObject*>> ProxyQtLoader::load(const MOBase::PluginExtension& extension)
{
  QList<QList<QObject*>> plugins;

  // TODO - retrieve plugins from extension instead of listing them

  QDirIterator iter(QDir(extension.directory(), {}, QDir::NoSort,
                         QDir::Files | QDir::NoDotAndDotDot));
  while (iter.hasNext()) {
    iter.next();
    const auto filePath = iter.filePath();

    // not a library, skip
    if (!QLibrary::isLibrary(filePath)) {
      continue;
    }

    // check if we have proper metadata - this does not load the plugin (metaData()
    // should be very lightweight)
    auto loader = QPluginLoaderPtr(new QPluginLoader(filePath));
    if (loader->metaData().isEmpty()) {
      log::debug("no metadata found in '{}', skipping", filePath);
      continue;
    }

    QObject* instance = loader->instance();
    if (!instance) {
      log::warn("failed to load plugin from '{}', skipping", filePath);
      continue;
    }

    m_loaders[&extension].push_back(std::move(loader));
    plugins.push_back({instance});
  }

  return plugins;
}

void ProxyQtLoader::unload(const MOBase::PluginExtension& extension)
{
  if (auto it = m_loaders.find(&extension); it != m_loaders.end()) {
    m_loaders.erase(it);
  }
}

void ProxyQtLoader::unloadAll()
{
  m_loaders.clear();
}
