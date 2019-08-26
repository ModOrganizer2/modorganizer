#include "serverinfo.h"

ServerInfo::ServerInfo()
  : ServerInfo({}, false, {}, 0, 0, 0.0)
{
}

ServerInfo::ServerInfo(
  QString name, bool premium, QDate last, int preferred,
  int count, double speed) :
    m_name(std::move(name)), m_premium(premium), m_lastSeen(std::move(last)),
    m_preferred(preferred), m_downloadCount(count), m_downloadSpeed(speed)
{
}

const QString& ServerInfo::name() const
{
  return m_name;
}

bool ServerInfo::isPremium() const
{
  return m_premium;
}

const QDate& ServerInfo::lastSeen() const
{
  return m_lastSeen;
}

int ServerInfo::preferred() const
{
  return m_preferred;
}

int ServerInfo::downloadCount() const
{
  return m_downloadCount;
}

double ServerInfo::downloadSpeed() const
{
  return m_downloadSpeed;
}

void ServerInfo::setPreferred(int i)
{
  m_preferred = i;
}


void ServerList::add(ServerInfo s)
{
  m_servers.push_back(std::move(s));
}

ServerList::iterator ServerList::begin()
{
  return m_servers.begin();
}

ServerList::const_iterator ServerList::begin() const
{
  return m_servers.begin();
}

ServerList::iterator ServerList::end()
{
  return m_servers.end();
}

ServerList::const_iterator ServerList::end() const
{
  return m_servers.end();
}

std::size_t ServerList::size() const
{
  return m_servers.size();
}

bool ServerList::empty() const
{
  return m_servers.empty();
}
