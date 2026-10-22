#include "ModelManager.h"

#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>

namespace glate::offline {

namespace {

constexpr int kTransferTimeoutMs = 30'000;
constexpr int kCatalogLimitBytes = 4 * 1024 * 1024;

QNetworkRequest requestFor(const QUrl& url)
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(kTransferTimeoutMs);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Glate"));
    return request;
}

} // namespace

struct ModelManager::Download
{
    ModelEntry entry;
    qsizetype fileIndex = 0;
    qint64 doneBytes = 0; // finished files
    QPointer<QNetworkReply> reply;
    std::unique_ptr<QFile> file;
    QCryptographicHash hash{QCryptographicHash::Sha256};
    qint64 fileBytes = 0;
    QString failure; // set when a file is stopped early on our side
    bool cancelled = false;
};

QUrl ModelManager::defaultCatalogUrl()
{
    const QByteArray overrideUrl = qgetenv("GLATE_MODELS_CATALOG_URL");
    if (!overrideUrl.isEmpty()) {
        return {QString::fromUtf8(overrideUrl)};
    }
    return {QStringLiteral("https://github.com/keshavbhatt/glate-ng/releases/download/models/catalog.json")};
}

ModelManager::ModelManager(ModelStore* store, QUrl catalogUrl, QObject* parent)
    : QObject(parent)
    , m_store(store)
    , m_catalogUrl(std::move(catalogUrl))
{
    QDir().mkpath(m_store->root());
    loadCachedCatalog();
}

ModelManager::~ModelManager()
{
    if (m_active) {
        m_active->cancelled = true;
        if (QNetworkReply* reply = m_active->reply) {
            reply->abort();
        }
    }
}

void ModelManager::setNetworkAccessManager(QNetworkAccessManager* network)
{
    m_network = network;
}

void ModelManager::ensureNetwork()
{
    if (!m_network) {
        m_network = new QNetworkAccessManager(this);
    }
}

void ModelManager::loadCachedCatalog()
{
    // A cached catalogue fetched from another address is not reused.
    QFile source(m_store->catalogCachePath() + QStringLiteral(".url"));
    if (!source.open(QIODevice::ReadOnly) || QString::fromUtf8(source.readAll()) != m_catalogUrl.toString()) {
        return;
    }
    QFile cache(m_store->catalogCachePath());
    if (!cache.open(QIODevice::ReadOnly)) {
        return;
    }
    if (auto catalog = parseCatalog(cache.readAll())) {
        m_catalog = std::move(catalog);
    }
}

void ModelManager::refreshCatalog()
{
    if (m_catalogReply) {
        return;
    }
    ensureNetwork();
    QNetworkReply* reply = m_network->get(requestFor(m_catalogUrl));
    m_catalogReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            Q_EMIT catalogFailed(reply->errorString());
            return;
        }
        const QByteArray body = reply->read(kCatalogLimitBytes);
        QString error;
        auto catalog = parseCatalog(body, &error);
        if (!catalog) {
            Q_EMIT catalogFailed(tr("The model list is damaged: %1").arg(error));
            return;
        }
        m_catalog = std::move(catalog);
        QSaveFile cache(m_store->catalogCachePath());
        QSaveFile source(m_store->catalogCachePath() + QStringLiteral(".url"));
        if (cache.open(QIODevice::WriteOnly) && cache.write(body) == body.size() && cache.commit() &&
            source.open(QIODevice::WriteOnly)) {
            source.write(m_catalogUrl.toString().toUtf8());
            source.commit();
        }
        Q_EMIT catalogChanged();
    });
}

void ModelManager::install(const QString& id)
{
    if (!m_catalog || m_catalog->find(id) == nullptr || isQueued(id)) {
        return;
    }
    m_queue.append(id);
    Q_EMIT queueChanged();
    startNext();
}

void ModelManager::cancel(const QString& id)
{
    if (m_queue.removeAll(id) > 0) {
        Q_EMIT queueChanged();
    }
    if (m_active && m_active->entry.id() == id) {
        m_active->cancelled = true;
        if (QNetworkReply* reply = m_active->reply) {
            reply->abort(); // finishes through onFileFinished()
        }
    }
}

bool ModelManager::remove(const QString& id)
{
    cancel(id);
    if (!m_store->remove(id)) {
        return false;
    }
    Q_EMIT removed(id);
    return true;
}

bool ModelManager::isQueued(const QString& id) const
{
    return m_queue.contains(id) || activeDownload() == id;
}

QString ModelManager::activeDownload() const
{
    return m_active ? m_active->entry.id() : QString();
}

QStringList ModelManager::updatesAvailable() const
{
    QStringList ids;
    if (!m_catalog) {
        return ids;
    }
    const QList<InstalledModel> models = m_store->installed();
    for (const InstalledModel& model : models) {
        const ModelEntry* entry = m_catalog->find(model.id());
        if (entry != nullptr && entry->version != model.version) {
            ids.append(model.id());
        }
    }
    return ids;
}

void ModelManager::startNext()
{
    if (m_active || m_queue.isEmpty() || !m_catalog) {
        return;
    }
    const QString id = m_queue.takeFirst();
    const ModelEntry* entry = m_catalog->find(id);
    if (entry == nullptr) {
        startNext();
        return;
    }
    m_active = std::make_unique<Download>();
    m_active->entry = *entry;

    const QString staging = m_store->stagingDirectoryFor(id);
    QDir(staging).removeRecursively();
    if (!QDir().mkpath(staging)) {
        finishActive(tr("Cannot create %1").arg(staging));
        return;
    }
    ensureNetwork();
    startFile();
}

void ModelManager::startFile()
{
    if (!m_catalog) {
        finishActive(tr("The model list is not available"));
        return;
    }
    Download& download = *m_active;
    const ModelFile& file = download.entry.files.at(download.fileIndex);
    const QString path = QDir(m_store->stagingDirectoryFor(download.entry.id())).filePath(file.name);

    download.file = std::make_unique<QFile>(path);
    if (!download.file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        finishActive(tr("Cannot write %1: %2").arg(path, download.file->errorString()));
        return;
    }
    download.hash.reset();
    download.fileBytes = 0;

    QNetworkReply* reply = m_network->get(requestFor(m_catalog->urlFor(file)));
    download.reply = reply;
    connect(reply, &QNetworkReply::readyRead, this, &ModelManager::onReadyRead);
    connect(reply, &QNetworkReply::finished, this, &ModelManager::onFileFinished);
}

void ModelManager::onReadyRead()
{
    if (!m_active || !m_active->reply) {
        return;
    }
    Download& download = *m_active;
    const ModelFile& file = download.entry.files.at(download.fileIndex);
    const QByteArray chunk = download.reply->readAll();
    download.fileBytes += chunk.size();
    // More bytes than the catalogue promised: the wrong file, stop early.
    if (download.fileBytes > file.size) {
        download.failure = tr("%1 is larger than published").arg(file.name);
        download.reply->abort();
        return;
    }
    if (download.file->write(chunk) != chunk.size()) {
        download.failure = tr("Cannot write %1: %2").arg(file.name, download.file->errorString());
        download.reply->abort();
        return;
    }
    download.hash.addData(chunk);
    Q_EMIT downloadProgress(download.entry.id(), download.doneBytes + download.fileBytes,
                            download.entry.totalSize());
}

void ModelManager::onFileFinished()
{
    if (!m_active) {
        return;
    }
    Download& download = *m_active;
    QNetworkReply* reply = download.reply;
    if (reply == nullptr) {
        return;
    }
    // Bytes that arrived together with the finished signal.
    if (reply->bytesAvailable() > 0 && reply->error() == QNetworkReply::NoError) {
        onReadyRead();
    }
    reply->deleteLater();
    download.reply = nullptr;
    download.file->close();

    const ModelFile& file = download.entry.files.at(download.fileIndex);
    if (download.cancelled) {
        finishActive(QString());
        return;
    }
    if (!download.failure.isEmpty()) {
        finishActive(download.failure);
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        finishActive(reply->errorString());
        return;
    }
    if (download.fileBytes != file.size || download.hash.result().toHex() != file.sha256) {
        finishActive(tr("%1 did not match the published checksum").arg(file.name));
        return;
    }

    download.doneBytes += file.size;
    ++download.fileIndex;
    if (download.fileIndex < download.entry.files.size()) {
        startFile();
        return;
    }
    QString error;
    if (!m_store->commit(download.entry, &error)) {
        finishActive(error);
        return;
    }
    const QString id = download.entry.id();
    m_active.reset();
    Q_EMIT installed(id);
    Q_EMIT queueChanged();
    startNext();
}

void ModelManager::finishActive(const QString& error)
{
    const QString id = m_active->entry.id();
    const bool cancelled = m_active->cancelled;
    if (m_active->file) {
        m_active->file->close();
    }
    m_active.reset();
    QDir(m_store->stagingDirectoryFor(id)).removeRecursively();
    if (!cancelled) {
        Q_EMIT installFailed(id, error);
    }
    Q_EMIT queueChanged();
    startNext();
}

} // namespace glate::offline
