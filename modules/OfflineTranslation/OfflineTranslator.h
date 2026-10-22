#pragma once

#include "Languages.h"
#include "ModelStore.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

class QThread;

namespace glate::offline {

class TranslationEngine;

// What an offline translation between two glate languages needs.
struct TranslationPlan
{
    enum class Status
    {
        Ready,               // every direction is installed
        MissingModels,       // `missing` lists what to download
        NeedsSourceLanguage, // "auto": detection needs the online service
        SameLanguage,
        NoEngine, // built without the engine
    };

    Status status = Status::NoEngine;
    QList<Direction> missing;
    QList<InstalledModel> route;
};

// Translates offline on a worker thread. Lives on the GUI thread; results come
// back as signals there.
class OfflineTranslator : public QObject
{
    Q_OBJECT

public:
    explicit OfflineTranslator(const ModelStore* store, QObject* parent = nullptr);
    ~OfflineTranslator() override;
    OfflineTranslator(const OfflineTranslator&) = delete;
    OfflineTranslator& operator=(const OfflineTranslator&) = delete;
    OfflineTranslator(OfflineTranslator&&) = delete;
    OfflineTranslator& operator=(OfflineTranslator&&) = delete;

    // `glateFrom` may be "auto". Codes are glate's (Google's).
    [[nodiscard]] TranslationPlan plan(const QString& glateFrom, const QString& glateTo) const;

    // Starts a translation and returns its id, or 0 when the plan is not Ready
    // (failed() then follows with request id 0 and the reason).
    quint64 translate(const QString& glateFrom, const QString& glateTo, const QString& text);

    // Call after models were installed, updated or removed.
    void reloadModels();

Q_SIGNALS:
    // One [translation, source] pair per non-empty input line, in order. Empty
    // lines are kept as empty pairs so the layout survives.
    void finished(quint64 requestId, const QList<QStringList>& pairs);
    void failed(quint64 requestId, const QString& message);

private:
    const ModelStore* m_store;
    QThread* m_thread;
    QObject* m_worker;           // context object living on m_thread; queued work runs there
    TranslationEngine* m_engine; // owned, only touched on m_thread
    quint64 m_lastRequest = 0;
};

} // namespace glate::offline
