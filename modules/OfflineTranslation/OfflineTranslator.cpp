#include "OfflineTranslator.h"

#include "TranslationEngine.h"

#include <QPointer>
#include <QThread>

namespace glate::offline {

OfflineTranslator::OfflineTranslator(const ModelStore* store, QObject* parent)
    : QObject(parent)
    , m_store(store)
    , m_thread(new QThread(this))
    , m_worker(new QObject)
    , m_engine(new TranslationEngine)
{
    m_thread->setObjectName(QStringLiteral("glate-offline-translation"));
    m_worker->moveToThread(m_thread);
    m_thread->start(QThread::LowPriority);
}

OfflineTranslator::~OfflineTranslator()
{
    m_thread->quit();
    m_thread->wait();
    // The thread has stopped, so nothing runs on these any more.
    delete m_worker;
    delete m_engine;
}

TranslationPlan OfflineTranslator::plan(const QString& glateFrom, const QString& glateTo) const
{
    TranslationPlan plan;
    if (!TranslationEngine::isAvailable()) {
        return plan;
    }
    if (glateFrom == u"auto" || glateFrom.isEmpty()) {
        plan.status = TranslationPlan::Status::NeedsSourceLanguage;
        return plan;
    }
    const QList<Direction> directions = route(toModelCode(glateFrom), toModelCode(glateTo));
    if (directions.isEmpty()) {
        plan.status = TranslationPlan::Status::SameLanguage;
        return plan;
    }
    for (const Direction& direction : directions) {
        if (auto installed = m_store->find(direction.from, direction.to)) {
            plan.route.append(*installed);
        } else {
            plan.missing.append(direction);
        }
    }
    plan.status =
        plan.missing.isEmpty() ? TranslationPlan::Status::Ready : TranslationPlan::Status::MissingModels;
    return plan;
}

namespace {

QString refusalReason(TranslationPlan::Status status)
{
    switch (status) {
    case TranslationPlan::Status::NoEngine:
        return OfflineTranslator::tr("This build of Glate cannot translate offline.");
    case TranslationPlan::Status::NeedsSourceLanguage:
        return OfflineTranslator::tr("Choose the source language. Detecting it needs the online service.");
    case TranslationPlan::Status::SameLanguage:
        return OfflineTranslator::tr("The source and target languages are the same.");
    default:
        return OfflineTranslator::tr("Download the offline models for this language pair first.");
    }
}

// Translations line up with the non-empty lines; empty lines stay as empty pairs.
QList<QStringList> pairUp(const QStringList& lines, const QStringList& translations)
{
    QList<QStringList> pairs;
    qsizetype next = 0;
    for (const QString& line : lines) {
        if (line.trimmed().isEmpty()) {
            pairs.append({QString(), QString()});
        } else {
            pairs.append({translations.value(next++), line});
        }
    }
    return pairs;
}

} // namespace

quint64 OfflineTranslator::translate(const QString& glateFrom, const QString& glateTo, const QString& text)
{
    const TranslationPlan current = plan(glateFrom, glateTo);
    if (current.status != TranslationPlan::Status::Ready) {
        const QString reason = refusalReason(current.status);
        QMetaObject::invokeMethod(this, [this, reason] { Q_EMIT failed(0, reason); }, Qt::QueuedConnection);
        return 0;
    }
    const quint64 requestId = ++m_lastRequest;

    const QStringList lines = text.split(u'\n');
    QStringList paragraphs;
    for (const QString& line : lines) {
        if (!line.trimmed().isEmpty()) {
            paragraphs.append(line);
        }
    }

    const QPointer<OfflineTranslator> self(this);
    TranslationEngine* engine = m_engine;
    const QList<InstalledModel> route = current.route;
    const auto work = [self, engine, route, lines, paragraphs, requestId] {
        const EngineResult result = paragraphs.isEmpty()
                                        ? EngineResult{.ok = true, .translations = {}, .error = {}}
                                        : engine->translate(route, paragraphs);
        const auto deliver = [self, result, lines, requestId] {
            if (self.isNull()) {
                return;
            }
            if (result.ok) {
                Q_EMIT self->finished(requestId, pairUp(lines, result.translations));
            } else {
                Q_EMIT self->failed(requestId, result.error);
            }
        };
        if (!self.isNull()) {
            QMetaObject::invokeMethod(self.data(), deliver, Qt::QueuedConnection);
        }
    };
    QMetaObject::invokeMethod(m_worker, work, Qt::QueuedConnection);
    return requestId;
}

void OfflineTranslator::reloadModels()
{
    TranslationEngine* engine = m_engine;
    QMetaObject::invokeMethod(m_worker, [engine] { engine->unloadAll(); }, Qt::QueuedConnection);
}

} // namespace glate::offline
