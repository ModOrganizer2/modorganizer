#ifndef SERVERINFO_H
#define SERVERINFO_H

#include <QDate>
#include <QMetaType>
#include <QString>

struct ServerInfo {
    QString name;
    bool premium;
    QDate lastSeen;
    bool preferred;
};

Q_DECLARE_METATYPE(ServerInfo)

#endif // SERVERINFO_H
