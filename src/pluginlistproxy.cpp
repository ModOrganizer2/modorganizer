#include "pluginlistproxy.h"
#include "organizerproxy.h"
#include "proxyutils.h"

using namespace MOBase;
using namespace MOShared;

PluginListProxy::PluginListProxy(OrganizerProxy* oproxy, PluginList* pluginlist) :
  m_OrganizerProxy(oproxy), m_Proxied(pluginlist) { }

PluginListProxy::~PluginListProxy()
{
  disconnectSignals();
}

void PluginListProxy::connectSignals()
{
  m_Connections.push_back(m_Proxied->onRefreshed(callSignalIfPluginActive(m_OrganizerProxy, m_Refreshed)));
  m_Connections.push_back(m_Proxied->onPluginMoved(callSignalIfPluginActive(m_OrganizerProxy, m_PluginMoved)));
  m_Connections.push_back(m_Proxied->onPluginStateChanged(callSignalIfPluginActive(m_OrganizerProxy, m_PluginStateChanged)));
}

void PluginListProxy::disconnectSignals()
{
  for (auto& conn : m_Connections) {
    conn.disconnect();
  }
  m_Connections.clear();
}

QStringList PluginListProxy::pluginNames() const
{
  return m_Proxied->pluginNames();
}

IPluginList::PluginStates PluginListProxy::state(const QString& name) const
{
  return m_Proxied->state(name);
}

void PluginListProxy::setState(const QString& name, PluginStates state)
{
  return m_Proxied->setState(name, state);
}

int PluginListProxy::priority(const QString& name) const
{
  return m_Proxied->priority(name);
}

bool PluginListProxy::setPriority(const QString& name, int newPriority)
{
  return m_Proxied->setPriority(name, newPriority);
}

int PluginListProxy::loadOrder(const QString& name) const
{
  return m_Proxied->loadOrder(name);
}

void PluginListProxy::setLoadOrder(const QStringList& pluginList)
{
  return m_Proxied->setLoadOrder(pluginList);
}

bool PluginListProxy::isMaster(const QString& name) const
{
  return m_Proxied->isMasterFlagged(name);
}

QStringList PluginListProxy::masters(const QString& name) const
{
  return m_Proxied->masters(name);
}

QString PluginListProxy::origin(const QString& name) const
{
  return m_Proxied->origin(name);
}

bool PluginListProxy::onRefreshed(const std::function<void()>& func)
{
  return m_Refreshed.connect(func).connected();
}

bool PluginListProxy::onPluginMoved(const std::function<void(const QString&, int, int)>& func)
{
  return m_PluginMoved.connect(func).connected();
}

bool PluginListProxy::onPluginStateChanged(const std::function<void(const std::map<QString, PluginStates>&)> &func)
{
  return m_PluginStateChanged.connect(func).connected();
}

bool PluginListProxy::hasMasterExtension(const QString& name) const
{
  return m_Proxied->hasMasterExtension(name);
}

bool PluginListProxy::hasLightExtension(const QString& name) const
{
  return m_Proxied->hasLightExtension(name);
}

bool PluginListProxy::isMasterFlagged(const QString& name) const
{
  return m_Proxied->isMasterFlagged(name);
}

bool PluginListProxy::isLightFlagged(const QString& name) const
{
  return m_Proxied->isLightFlagged(name);
}
