#ifndef SERVERINFO_H
#define SERVERINFO_H

#include <QString>
#include <QDate>
#include <QMetaType>

struct ServerInfo
{
  QString name;
  bool premium;
  QDate lastSeen;
  bool preferred;
};

Q_DECLARE_METATYPE(ServerInfo)

#endif // SERVERINFO_H
