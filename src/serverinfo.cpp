#include "serverinfo.h"
#include "log.h"

using namespace MOBase;

const std::size_t MaxDownloadCount = 5;

ServerInfo::ServerInfo() : ServerInfo({}, false, {}, 0, {}) {}

ServerInfo::ServerInfo(QString name, bool premium, QDate last, int preferred,
                       SpeedList lastDownloads)
    : m_name(std::move(name)), m_premium(premium), m_lastSeen(std::move(last)),
      m_preferred(preferred), m_lastDownloads(std::move(lastDownloads))
{
  if (m_lastDownloads.size() > MaxDownloadCount) {
    m_lastDownloads.resize(MaxDownloadCount);
  }
}

const QString& ServerInfo::name() const
{
  return m_name;
}

bool ServerInfo::isPremium() const
{
  return m_premium;
}

void ServerInfo::setPremium(bool b)
{
  m_premium = b;
}

const QDate& ServerInfo::lastSeen() const
{
  return m_lastSeen;
}

void ServerInfo::updateLastSeen()
{
  m_lastSeen = QDate::currentDate();
}

int ServerInfo::preferred() const
{
  return m_preferred;
}

void ServerInfo::setPreferred(int i)
{
  m_preferred = i;
}

const ServerInfo::SpeedList& ServerInfo::lastDownloads() const
{
  return m_lastDownloads;
}

int ServerInfo::averageSpeed() const
{
  int count = 0;
  int total = 0;

  for (const auto& s : m_lastDownloads) {
    if (s > 0) {
      ++count;
      total += s;
    }
  }

  if (count > 0) {
    return static_cast<double>(total) / count;
  }

  return 0;
}

void ServerInfo::addDownload(int bytesPerSecond)
{
  if (bytesPerSecond <= 0) {
    log::error("trying to add download with {} B/s to server '{}'; ignoring",
               bytesPerSecond, m_name);

    return;
  }

  if (m_lastDownloads.size() == MaxDownloadCount) {
    std::rotate(m_lastDownloads.begin(), m_lastDownloads.begin() + 1,
                m_lastDownloads.end());

    m_lastDownloads.back() = bytesPerSecond;
  } else {
    m_lastDownloads.push_back(bytesPerSecond);
  }

  log::debug("added download at {} B/s to server '{}'", bytesPerSecond, m_name);
}

void ServerList::add(ServerInfo s)
{
  m_servers.push_back(std::move(s));

  std::sort(m_servers.begin(), m_servers.end(), [](auto&& a, auto&& b) {
    return (a.preferred() < b.preferred());
  });
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

ServerList::container ServerList::getPreferred() const
{
  container v;

  for (const auto& server : m_servers) {
    if (server.preferred() > 0) {
      v.push_back(server);
    }
  }

  return v;
}

void ServerList::cleanup()
{
  QDate now = QDate::currentDate();

  for (auto itor = m_servers.begin(); itor != m_servers.end();) {
    const QDate lastSeen = itor->lastSeen();

    if (lastSeen.daysTo(now) > 30) {
      log::debug("removing server {} since it hasn't been available for downloads "
                 "in over a month",
                 itor->name());

      itor = m_servers.erase(itor);
    } else {
      ++itor;
    }
  }
}
