#include "github.h"
#include <QEventLoop>
#include <QJsonDocument>
#include <QNetworkRequest>

#include <QCoreApplication>
#include <QThread>

static const QString GITHUB_URL("https://api.github.com");
static const QString USER_AGENT("GitHubPP");

GitHub::GitHub(const char* clientId) : m_AccessManager(new QNetworkAccessManager(this))
{}

GitHub::~GitHub()
{
  // delete all the replies since they depend on the access manager, which is
  // about to be deleted
  for (auto* reply : m_replies) {
    reply->disconnect();
    delete reply;
  }
}

QJsonArray GitHub::releases(const Repository& repo)
{
  QJsonDocument result = request(
      Method::GET, QString("repos/%1/%2/releases").arg(repo.owner, repo.project),
      QByteArray(), true);
  return result.array();
}

void GitHub::releases(const Repository& repo,
                      const std::function<void(const QJsonArray&)>& callback)
{
  request(
      Method::GET, QString("repos/%1/%2/releases").arg(repo.owner, repo.project),
      QByteArray(),
      [callback](const QJsonDocument& result) {
        callback(result.array());
      },
      true);
}

QJsonDocument GitHub::handleReply(QNetworkReply* reply)
{
  int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if (statusCode != 200) {
    return QJsonDocument(QJsonObject(
        {{"http_status", statusCode},
         {"redirection",
          reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toString()},
         {"reason",
          reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString()}}));
  }

  QByteArray data = reply->readAll();
  if (data.isNull() || data.isEmpty() || (strcmp(data.constData(), "null") == 0)) {
    return QJsonDocument();
  }

  QJsonParseError parseError;
  QJsonDocument result = QJsonDocument::fromJson(data, &parseError);

  if (parseError.error != QJsonParseError::NoError) {
    return QJsonDocument(QJsonObject({{"parse_error", parseError.errorString()}}));
  }

  return result;
}

QNetworkReply* GitHub::genReply(Method method, const QString& path,
                                const QByteArray& data, bool relative)
{
  QNetworkRequest request(relative ? GITHUB_URL + "/" + path : path);

  request.setHeader(QNetworkRequest::UserAgentHeader, USER_AGENT);
  request.setRawHeader("Accept", "application/vnd.github.v3+json");

  switch (method) {
  case Method::GET:
    return m_AccessManager->get(request);
  case Method::POST:
    return m_AccessManager->post(request, data);
  default:
    // this shouldn't be possible as all enum options are handled
    throw std::runtime_error("invalid method");
  }
}

QJsonDocument GitHub::request(Method method, const QString& path,
                              const QByteArray& data, bool relative)
{
  QEventLoop wait;
  QNetworkReply* reply = genReply(method, path, data, relative);

  connect(reply, SIGNAL(finished), &wait, SLOT(quit()));
  wait.exec();
  QJsonDocument result = handleReply(reply);
  reply->deleteLater();

  QJsonObject object = result.object();
  if (object.value("http_status").toDouble() == 301.0) {
    return request(method, object.value("redirection").toString(), data, false);
  } else {
    return result;
  }
}

void GitHub::request(Method method, const QString& path, const QByteArray& data,
                     const std::function<void(const QJsonDocument&)>& callback,
                     bool relative)
{
  // make sure the timer is owned by this so it's deleted correctly and
  // doesn't fire after the GitHub object is destroyed; this happens when
  // restarting MO by switching instances, for example
  QTimer* timer = new QTimer(this);
  timer->setSingleShot(true);
  timer->setInterval(10000);

  QNetworkReply* reply = genReply(method, path, data, relative);

  // remember this reply so it can be deleted in the destructor if necessary
  m_replies.push_back(reply);

  Request req = {method, data, callback, timer, reply};

  // finished
  connect(reply, &QNetworkReply::finished, [this, req] {
    onFinished(req);
  });

  // error
  connect(reply, qOverload<QNetworkReply::NetworkError>(&QNetworkReply::errorOccurred),
          [this, req](auto&& error) {
            onError(req, error);
          });

  // timeout
  connect(timer, &QTimer::timeout, [this, req] {
    onTimeout(req);
  });

  timer->start();
}

void GitHub::onFinished(const Request& req)
{
  QJsonDocument result = handleReply(req.reply);
  QJsonObject object   = result.object();

  req.timer->stop();

  if (object.value("http_status").toInt() == 301) {
    request(req.method, object.value("redirection").toString(), req.data, req.callback,
            false);
  } else {
    req.callback(result);
  }

  deleteReply(req.reply);
}

void GitHub::onError(const Request& req, QNetworkReply::NetworkError error)
{
  // the only way the request can be aborted is when there's a timeout, which
  // already logs a message
  if (error != QNetworkReply::OperationCanceledError) {
    qCritical().noquote().nospace()
        << "Github: request for " << req.reply->url().toString() << " failed, "
        << req.reply->errorString() << " (" << error << ")";
  }

  req.timer->stop();
  req.reply->disconnect();

  QJsonObject root({{"network_error", req.reply->errorString()}});
  QJsonDocument doc(root);

  req.callback(doc);

  deleteReply(req.reply);
}

void GitHub::onTimeout(const Request& req)
{
  qCritical().noquote().nospace()
      << "Github: request for " << req.reply->url().toString() << " timed out";

  // don't delete the reply, abort will fire the error() handler above
  req.reply->abort();
}

void GitHub::deleteReply(QNetworkReply* reply)
{
  // remove from the list
  auto itor = std::find(m_replies.begin(), m_replies.end(), reply);
  if (itor != m_replies.end()) {
    m_replies.erase(itor);
  }

  // delete
  reply->deleteLater();
}
