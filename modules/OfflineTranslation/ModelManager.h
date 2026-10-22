#pragma once

#include "ModelCatalog.h"
#include "ModelStore.h"

#include <QCryptographicHash>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QUrl>

#include <memory>
#include <optional>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

namespace glate::offline {

// Fetches the model catalogue and downloads, verifies and installs model
// directions, one at a time. Lives on the GUI thread.
class ModelManager : public QObject
{
    Q_OBJECT

public:
    // The glate-ng repository's rolling `models` release (DOCS/DECISIONS.md ADR-004).
    // GLATE_MODELS_CATALOG_URL in the environment overrides it for testing.
    static QUrl defaultCatalogUrl();

    ModelManager(ModelStore* store, QUrl catalogUrl, QObject* parent = nullptr);
    ~ModelManager() override;
    ModelManager(const ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;
    ModelManager(ModelManager&&) = delete;
    ModelManager& operator=(ModelManager&&) = delete;

    // Uses a shared access manager instead of a private one. Call before any request.
    void setNetworkAccessManager(QNetworkAccessManager* network);

    // The last catalogue fetched, or the cached copy from an earlier run.
    [[nodiscard]] const std::optional<Catalog>& catalog() const { return m_catalog; }
    void refreshCatalog();

    // Queues the direction for download; `id` is "en-de". Needs a catalogue.
    void install(const QString& id);
    void cancel(const QString& id);
    bool remove(const QString& id);

    [[nodiscard]] bool isQueued(const QString& id) const;
    [[nodiscard]] QString activeDownload() const;

    // Directions with a newer version in the catalogue than on disk.
    [[nodiscard]] QStringList updatesAvailable() const;

Q_SIGNALS:
    void catalogChanged();
    void catalogFailed(const QString& reason);
    void downloadProgress(const QString& id, qint64 received, qint64 total);
    void installed(const QString& id);
    void installFailed(const QString& id, const QString& reason);
    void removed(const QString& id);
    void queueChanged();

private:
    struct Download;

    void loadCachedCatalog();
    void ensureNetwork();
    void startNext();
    void startFile();
    void onReadyRead();
    void onFileFinished();
    void finishActive(const QString& error);

    ModelStore* m_store;
    QUrl m_catalogUrl;
    QPointer<QNetworkAccessManager> m_network;
    QPointer<QNetworkReply> m_catalogReply;
    std::optional<Catalog> m_catalog;
    QStringList m_queue;
    std::unique_ptr<Download> m_active;
};

} // namespace glate::offline
