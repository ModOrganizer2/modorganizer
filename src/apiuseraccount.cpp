#include "apiuseraccount.h"

APIUserAccount::APIUserAccount()
  : m_type(APIUserAccountTypes::None)
{
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
  return std::max(
    m_limits.remainingDailyRequests,
    m_limits.remainingHourlyRequests);
}

bool APIUserAccount::shouldThrottle() const
{
  return (remainingRequests() < ThrottleThreshold);
}

bool APIUserAccount::exhausted() const
{
  return (remainingRequests() <= 0);
}
