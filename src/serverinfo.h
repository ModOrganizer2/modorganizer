#ifndef SERVERINFO_H
#define SERVERINFO_H

#include <QString>
#include <QDate>
#include <QMetaType>

class ServerInfo
{
public:
  ServerInfo();
  ServerInfo(QString name, bool premium, QDate lastSeen, bool preferred);

  const QString& name() const;
  bool isPremium() const;
  const QDate& lastSeen() const;
  bool isPreferred() const;

private:
  QString m_name;
  bool m_premium;
  QDate m_lastSeen;
  bool m_preferred;
};

Q_DECLARE_METATYPE(ServerInfo)

#endif // SERVERINFO_H
