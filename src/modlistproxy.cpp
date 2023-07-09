#include "modlistproxy.h"
#include "modlist.h"
#include "organizerproxy.h"
#include "proxyutils.h"

using namespace MOBase;
using namespace MOShared;

ModListProxy::ModListProxy(OrganizerProxy* oproxy, ModList* modlist)
    : m_OrganizerProxy(oproxy), m_Proxied(modlist)
{}

ModListProxy::~ModListProxy()
{
  disconnectSignals();
}

void ModListProxy::connectSignals()
{
  m_Connections.push_back(m_Proxied->onModInstalled(
      callSignalIfPluginActive(m_OrganizerProxy, m_ModInstalled)));
  m_Connections.push_back(
      m_Proxied->onModMoved(callSignalIfPluginActive(m_OrganizerProxy, m_ModMoved)));
  m_Connections.push_back(m_Proxied->onModRemoved(
      callSignalIfPluginActive(m_OrganizerProxy, m_ModRemoved)));
  m_Connections.push_back(m_Proxied->onModStateChanged(
      callSignalIfPluginActive(m_OrganizerProxy, m_ModStateChanged)));
}

void ModListProxy::disconnectSignals()
{
  for (auto& conn : m_Connections) {
    conn.disconnect();
  }
  m_Connections.clear();
}

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

MOBase::IModInterface* ModListProxy::renameMod(MOBase::IModInterface* mod,
                                               const QString& name)
{
  return m_Proxied->renameMod(mod, name);
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
  return m_ModInstalled.connect(func).connected();
}

bool ModListProxy::onModRemoved(const std::function<void(QString const&)>& func)
{
  return m_ModRemoved.connect(func).connected();
}

bool ModListProxy::onModStateChanged(
    const std::function<void(const std::map<QString, ModStates>&)>& func)
{
  return m_ModStateChanged.connect(func).connected();
}

bool ModListProxy::onModMoved(const std::function<void(const QString&, int, int)>& func)
{
  return m_ModMoved.connect(func).connected();
}
