#pragma once

#include "ModelStore.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <memory>

namespace glate::offline {

struct EngineResult
{
    bool ok = false;
    QStringList translations; // one per input paragraph
    QString error;
};

// Runs translations with Mozilla's inference engine. Blocking: call it from a
// worker thread. Not thread-safe; one instance per worker. Keeps the last few
// directions loaded, since loading a model takes about 100 ms.
class TranslationEngine
{
public:
    TranslationEngine();
    ~TranslationEngine();
    TranslationEngine(const TranslationEngine&) = delete;
    TranslationEngine& operator=(const TranslationEngine&) = delete;
    TranslationEngine(TranslationEngine&&) = delete;
    TranslationEngine& operator=(TranslationEngine&&) = delete;

    // Translates each paragraph through the directions in `route` (one, or two
    // through the pivot language).
    [[nodiscard]] EngineResult translate(const QList<InstalledModel>& route, const QStringList& paragraphs);

    // Drops loaded models, for example after a model was removed or updated.
    void unloadAll();

    // False when glate was built without the engine (GLATE_OFFLINE_ENGINE=OFF).
    [[nodiscard]] static bool isAvailable();

private:
    struct Private;
    std::unique_ptr<Private> m_d;
};

} // namespace glate::offline
