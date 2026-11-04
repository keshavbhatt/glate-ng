#include "request.h"

Request::Request(QObject *parent) : QObject(parent) {
  _cache_path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
}

Request::~Request() {}

void Request::get(const QUrl &url) {
  auto m_netwManager = new QNetworkAccessManager(this);
  auto diskCache = new QNetworkDiskCache(this);
  diskCache->setCacheDirectory(_cache_path);
  m_netwManager->setCache(diskCache);
  connect(m_netwManager, &QNetworkAccessManager::finished, this,
          [=](QNetworkReply *rep) {
            if (rep->error() == QNetworkReply::NoError) {
              QString repStr = rep->readAll();
              emit requestFinished(repStr);
            } else {
              // Qt puts the full URL in the message, and its query holds the
              // text being translated: keep only the part before the query.
              QString message = rep->errorString();
              const QUrl url = rep->url();
              const QString bare =
                  url.adjusted(QUrl::RemoveQuery | QUrl::RemoveFragment)
                      .toString() +
                  QStringLiteral("?...");
              message.replace(url.toString(), bare);
              message.replace(url.toString(QUrl::FullyEncoded), bare);
              emit downloadError(
                  message,
                  rep->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                      .toInt());
            }
            rep->deleteLater();
            m_netwManager->deleteLater();
          });

  QNetworkRequest request(url);
  // Says who is asking, the same way the speech downloader does.
  const QString appVersion = QCoreApplication::applicationVersion().isEmpty()
                                 ? QStringLiteral("unknown")
                                 : QCoreApplication::applicationVersion();
  request.setHeader(
      QNetworkRequest::UserAgentHeader,
      QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) Glate/%1").arg(appVersion));
  m_netwManager->get(request);
  emit requestStarted();
}

void Request::downloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
  double progress = (double)bytesReceived / bytesTotal * 100.0;
  int intProgress = static_cast<int>(progress);
  emit _downloadProgress(intProgress);
}
