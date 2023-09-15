#include "persistentcookiejar.h"
#include <QDataStream>
#include <QNetworkCookie>
#include <QTemporaryFile>
#include <log.h>

using namespace MOBase;

PersistentCookieJar::PersistentCookieJar(const QString& fileName, QObject* parent)
    : QNetworkCookieJar(parent), m_FileName(fileName)
{
  restore();
}

PersistentCookieJar::~PersistentCookieJar()
{
  log::debug("save {}", m_FileName);
  save();
}

void PersistentCookieJar::clear()
{
  for (const QNetworkCookie& cookie : allCookies()) {
    deleteCookie(cookie);
  }
}

void PersistentCookieJar::save()
{
  QTemporaryFile file;
  if (!file.open()) {
    log::error("failed to save cookies: couldn't create temporary file");
    return;
  }
  QDataStream data(&file);

  QList<QNetworkCookie> cookies = allCookies();
  data << static_cast<quint32>(cookies.size());

  for (const QNetworkCookie& cookie : allCookies()) {
    data << cookie.toRawForm();
  }

  {
    QFile oldCookies(m_FileName);
    if (oldCookies.exists()) {
      if (!oldCookies.remove()) {
        log::error("failed to save cookies: failed to remove {}", m_FileName);
        return;
      }
    }  // if it doesn't exists that's fine
  }

  if (!file.copy(m_FileName)) {
    log::error("failed to save cookies: failed to write {}", m_FileName);
  }
}

void PersistentCookieJar::restore()
{
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
