#ifndef SERVERINFO_H
#define SERVERINFO_H

#include <QDate>
#include <QMetaType>
#include <QString>

class ServerInfo
{
public:
  using SpeedList = std::vector<int>;

  ServerInfo();
  ServerInfo(QString name, bool premium, QDate lastSeen, int preferred,
             SpeedList lastDownloads);

  const QString& name() const;

  bool isPremium() const;
  void setPremium(bool b);

  const QDate& lastSeen() const;
  void updateLastSeen();

  int preferred() const;
  void setPreferred(int i);

  const SpeedList& lastDownloads() const;
  int averageSpeed() const;
  void addDownload(int bytesPerSecond);

private:
  QString m_name;
  bool m_premium;
  QDate m_lastSeen;
  int m_preferred;
  SpeedList m_lastDownloads;
};

Q_DECLARE_METATYPE(ServerInfo)

class ServerList
{
public:
  using container      = std::vector<ServerInfo>;
  using iterator       = container::iterator;
  using const_iterator = container::const_iterator;

  void add(ServerInfo s);

  iterator begin();
  const_iterator begin() const;
  iterator end();
  const_iterator end() const;
  std::size_t size() const;
  bool empty() const;

  container getPreferred() const;

  // removes servers that haven't been seen in a while
  //
  void cleanup();

private:
  container m_servers;
};

#endif  // SERVERINFO_H
