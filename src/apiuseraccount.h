#ifndef APIUSERACCOUNT_H
#define APIUSERACCOUNT_H

#include <QString>

/**
 * represents user account types on a mod provider website such as nexus
 */
enum class APIUserAccountTypes
{
  // not logged in
  None = 0,

  // regular account
  Regular,

  // premium account
  Premium
};

QString localizedUserAccountType(APIUserAccountTypes t);

/**
 * current limits imposed on the user account
 **/
struct APILimits
{
  // maximum number of requests per day
  int maxDailyRequests = 0;

  // remaining number of requests today
  int remainingDailyRequests = 0;

  // maximum number of requests per hour
  int maxHourlyRequests = 0;

  // remaining number of requests this hour
  int remainingHourlyRequests = 0;
};

/**
 * API statistics
 */
struct APIStats
{
  // number of API requests currently queued
  int requestsQueued = 0;
};

/**
 * represents a user account on the mod provider website
 */
class APIUserAccount
{
public:
  // when the number of remaining requests is under this number, further
  // requests will be throttled by avoiding non-critical ones
  static const int ThrottleThreshold = 200;

  APIUserAccount();

  /**
   * whether the user is logged in
   */
  bool isValid() const;

  /**
   * api key
   */
  const QString& apiKey() const;

  /**
   * user id
   */
  const QString& id() const;

  /**
   * user name
   */
  const QString& name() const;

  /**
   * account type
   */
  APIUserAccountTypes type() const;

  /**
   * current API limits
   */
  const APILimits& limits() const;

  /**
   * sets the api key
   */
  APIUserAccount& apiKey(const QString& key);

  /**
   * sets the user id
   */
  APIUserAccount& id(const QString& id);

  /**
   * sets the user name
   **/
  APIUserAccount& name(const QString& name);

  /**
   * sets the account type
   */
  APIUserAccount& type(APIUserAccountTypes type);

  /**
   * sets the current limits
   */
  APIUserAccount& limits(const APILimits& limits);

  /**
   * returns the number of remaining requests
   */
  int remainingRequests() const;

  /**
   * whether the number of remaining requests is low enough that further
   * requests should be throttled
   */
  bool shouldThrottle() const;

  /**
   * true if all the remaining requests have been used and the API will refuse
   * further requests
   */
  bool exhausted() const;

private:
  QString m_key, m_id, m_name;
  APIUserAccountTypes m_type;
  APILimits m_limits;
};

#endif  // APIUSERACCOUNT_H
