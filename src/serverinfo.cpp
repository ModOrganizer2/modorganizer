#include "serverinfo.h"

ServerInfo::ServerInfo()
  : ServerInfo({}, false, {}, false)
{
}

ServerInfo::ServerInfo(QString n, bool premium, QDate last, bool preferred) :
  m_name(std::move(n)), m_premium(premium), m_lastSeen(std::move(last)),
  m_preferred(preferred)
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

bool ServerInfo::isPreferred() const
{
  return m_preferred;
}
