#include "pluginlistproxy.h"
#include "organizerproxy.h"
#include "proxyutils.h"

using namespace MOBase;

PluginListProxy::PluginListProxy(OrganizerProxy* oproxy, IPluginList* pluginlist) :
  m_OrganizerProxy(oproxy), m_Proxied(pluginlist) { }

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
  return m_Proxied->isMaster(name);
}

QStringList PluginListProxy::masters(const QString& name) const
{
  return m_Proxied->masters(name);
}

QString PluginListProxy::origin(const QString& name) const
{
  return m_Proxied->origin(name);
}

bool PluginListProxy::onRefreshed(const std::function<void()>& callback) 
{
  return m_Proxied->onRefreshed(MOShared::callIfPluginActive(m_OrganizerProxy, callback));
}

bool PluginListProxy::onPluginMoved(const std::function<void(const QString&, int, int)>& func)
{
  return m_Proxied->onPluginMoved(MOShared::callIfPluginActive(m_OrganizerProxy, func));
}

bool PluginListProxy::onPluginStateChanged(const std::function<void(const std::map<QString, PluginStates>&)> &func)
{
  return m_Proxied->onPluginStateChanged(MOShared::callIfPluginActive(m_OrganizerProxy, func));
}
