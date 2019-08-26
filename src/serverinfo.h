#ifndef SERVERINFO_H
#define SERVERINFO_H

#include <QString>
#include <QDate>
#include <QMetaType>

class ServerInfo
{
public:
  ServerInfo();
  ServerInfo(
    QString name, bool premium, QDate lastSeen, int preferred,
    int downloadCount, double downloadSpeed);

  const QString& name() const;
  bool isPremium() const;
  const QDate& lastSeen() const;
  int preferred() const;
  int downloadCount() const;
  double downloadSpeed() const;

  void setPreferred(int i);

private:
  QString m_name;
  bool m_premium;
  QDate m_lastSeen;
  int m_preferred;
  int m_downloadCount;
  double m_downloadSpeed;
};

Q_DECLARE_METATYPE(ServerInfo)


class ServerList
{
public:
  using container = QList<ServerInfo>;
  using iterator = container::iterator;
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

#endif // SERVERINFO_H
