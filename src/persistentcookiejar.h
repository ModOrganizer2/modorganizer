#ifndef PERSISTENTCOOKIEJAR_H
#define PERSISTENTCOOKIEJAR_H

#include <QNetworkCookieJar>

class PersistentCookieJar : public QNetworkCookieJar
{

  Q_OBJECT

public:
  PersistentCookieJar(const QString& fileName, QObject* parent = 0);
  virtual ~PersistentCookieJar();

  void clear();

private:
  void save();

  void restore();

private:
  QString m_FileName;
};

#endif  // PERSISTENTCOOKIEJAR_H
