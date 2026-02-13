#include "apiuseraccount.h"

QString localizedUserAccountType(APIUserAccountTypes t)
{
  switch (t) {
  case APIUserAccountTypes::Regular:
    return QObject::tr("Regular");

  case APIUserAccountTypes::Premium:
    return QObject::tr("Premium");

  case APIUserAccountTypes::None:  // fall-through
  default:
    return QObject::tr("None");
  }
}

APIUserAccount::APIUserAccount() : m_type(APIUserAccountTypes::None) {}

bool APIUserAccount::isValid() const
{
  return !m_accessToken.isEmpty();
}

const QString& APIUserAccount::accessToken() const
{
  return m_accessToken;
}

const QString& APIUserAccount::id() const
{
  return m_id;
}

const QString& APIUserAccount::name() const
{
  return m_name;
}

APIUserAccountTypes APIUserAccount::type() const
{
  return m_type;
}

const APILimits& APIUserAccount::limits() const
{
  return m_limits;
}

APIUserAccount& APIUserAccount::accessToken(const QString& token)
{
  m_accessToken = token;
  return *this;
}

APIUserAccount& APIUserAccount::id(const QString& id)
{
  m_id = id;
  return *this;
}

APIUserAccount& APIUserAccount::name(const QString& name)
{
  m_name = name;
  return *this;
}

APIUserAccount& APIUserAccount::type(APIUserAccountTypes type)
{
  m_type = type;
  return *this;
}

APIUserAccount& APIUserAccount::limits(const APILimits& limits)
{
  m_limits = limits;
  return *this;
}

int APIUserAccount::remainingRequests() const
{
  return std::max(m_limits.remainingDailyRequests, m_limits.remainingHourlyRequests);
}

bool APIUserAccount::shouldThrottle() const
{
  return (remainingRequests() < ThrottleThreshold);
}

bool APIUserAccount::exhausted() const
{
  return isValid() && (remainingRequests() <= 0);
}
