#ifndef PERSISTENTCOOKIEJAR_H
#define PERSISTENTCOOKIEJAR_H

#include <QNetworkCookieJar>


class PersistentCookieJar : public QNetworkCookieJar {
public:
  PersistentCookieJar(const QString &fileName, QObject *parent = 0);
  virtual ~PersistentCookieJar();
private:

  void save();

  void restore();

private:

  QString m_FileName;

};


#endif // PERSISTENTCOOKIEJAR_H
