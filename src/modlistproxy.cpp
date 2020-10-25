#include "modlistproxy.h"
#include "organizerproxy.h"
#include "proxyutils.h"

using namespace MOBase;

ModListProxy::ModListProxy(OrganizerProxy* oproxy, IModList* modlist) :
  m_OrganizerProxy(oproxy), m_Proxied(modlist) { }

QString ModListProxy::displayName(const QString& internalName) const
{
  return m_Proxied->displayName(internalName);
}

QStringList ModListProxy::allMods() const
{
  return m_Proxied->allMods();
}

QStringList ModListProxy::allModsByProfilePriority(MOBase::IProfile* profile) const
{
  return m_Proxied->allModsByProfilePriority(profile);
}

IModInterface* ModListProxy::getMod(const QString& name) const
{
  return m_Proxied->getMod(name);
}

bool ModListProxy::removeMod(MOBase::IModInterface* mod)
{
  return m_Proxied->removeMod(mod);
}

IModList::ModStates ModListProxy::state(const QString& name) const
{
  return m_Proxied->state(name);
}

bool ModListProxy::setActive(const QString& name, bool active)
{
  return m_Proxied->setActive(name, active);
}

int ModListProxy::setActive(const QStringList& names, bool active)
{
  return m_Proxied->setActive(names, active);
}

int ModListProxy::priority(const QString& name) const
{
  return m_Proxied->priority(name);
}

bool ModListProxy::setPriority(const QString& name, int newPriority)
{
  return m_Proxied->setPriority(name, newPriority);
}

bool ModListProxy::onModInstalled(const std::function<void(IModInterface*)>& func)
{
  return m_Proxied->onModInstalled(MOShared::callIfPluginActive(m_OrganizerProxy, func));
}

bool ModListProxy::onModRemoved(const std::function<void(QString const&)>& func)
{
  return m_Proxied->onModRemoved(MOShared::callIfPluginActive(m_OrganizerProxy, func));
}

bool ModListProxy::onModStateChanged(const std::function<void(const std::map<QString, ModStates>&)>& func)
{
  return m_Proxied->onModStateChanged(MOShared::callIfPluginActive(m_OrganizerProxy, func));
}

bool ModListProxy::onModMoved(const std::function<void(const QString&, int, int)>& func)
{
  return m_Proxied->onModMoved(MOShared::callIfPluginActive(m_OrganizerProxy, func));
}
