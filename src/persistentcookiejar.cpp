#include "persistentcookiejar.h"
#include <QTemporaryFile>


PersistentCookieJar::PersistentCookieJar(const QString &fileName, QObject *parent)
: QNetworkCookieJar(parent), m_FileName(fileName)
{
  restore();
}

PersistentCookieJar::~PersistentCookieJar() {
  qDebug("save %s", qPrintable(m_FileName));
  save();
}

void PersistentCookieJar::save() {
  QTemporaryFile file;
  if (!file.open()) {
    qCritical("failed to save cookies: couldn't create temporary file");
    return;
  }
  QDataStream data(&file);

  QList<QNetworkCookie> cookies = allCookies();
  data << static_cast<quint32>(cookies.size());

  foreach (const QNetworkCookie &cookie, allCookies()) {
    data << cookie.toRawForm();
  }

  {
    QFile oldCookies(m_FileName);
    if (oldCookies.exists()) {
      if (!oldCookies.remove()) {
        qCritical("failed to save cookies: failed to remove %s", qPrintable(m_FileName));
        return;
      }
    } // if it doesn't exists that's fine
  }

  if (!file.copy(m_FileName)) {
    qCritical("failed to save cookies: failed to write %s", qPrintable(m_FileName));
  }
}

void PersistentCookieJar::restore() {
  QFile file(m_FileName);
  if (!file.open(QIODevice::ReadOnly)) {
    // not necessarily a problem, the file may just not exist (yet)
    return;
  }

  QList<QNetworkCookie> allCookies;

  QDataStream data(&file);
  quint32 count;
  data >> count;
  for (quint32 i = 0; i < count; ++i) {
    QByteArray cookieRaw;
    data >> cookieRaw;
    allCookies.append(QNetworkCookie::parseCookies(cookieRaw));
  }
  setAllCookies(allCookies);
}
