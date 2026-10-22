#include "TranslationEngine.h"

#ifdef GLATE_HAVE_TRANSLATIONS_ENGINE
#include "EngineConfig.h"
#include "common/logging.h"
#include "translator/parser.h"
#include "translator/response_options.h"
#include "translator/service.h"
#endif

#include <exception>
#include <string>
#include <utility>
#include <vector>

namespace glate::offline {

#ifdef GLATE_HAVE_TRANSLATIONS_ENGINE

namespace bergamot = marian::bergamot;

namespace {
constexpr qsizetype kLoadedModelLimit = 4;
} // namespace

struct TranslationEngine::Private
{
    struct Loaded
    {
        QString key; // id plus version, so an updated model is reloaded
        std::shared_ptr<bergamot::TranslationModel> model;
    };

    Private()
        : service(bergamot::BlockingService::Config{})
    {
        // Errors in marian become exceptions instead of std::abort().
        marian::setThrowExceptionOnAbort(true);
    }

    std::shared_ptr<bergamot::TranslationModel> modelFor(const InstalledModel& installed)
    {
        const QString key = installed.id() + u'@' + installed.version;
        for (qsizetype i = 0; i < loaded.size(); ++i) {
            if (loaded.at(i).key == key) {
                loaded.move(i, 0); // most recently used first
                return loaded.first().model;
            }
        }
        const auto options = bergamot::parseOptionsFromString(engineConfig(installed), /*validate=*/false);
        auto model = std::make_shared<bergamot::TranslationModel>(options);
        loaded.prepend(Loaded{.key = key, .model = model});
        while (loaded.size() > kLoadedModelLimit) {
            loaded.removeLast();
        }
        return model;
    }

    bergamot::BlockingService service;
    QList<Loaded> loaded;
};

TranslationEngine::TranslationEngine()
    : m_d(std::make_unique<Private>())
{}

TranslationEngine::~TranslationEngine() = default;

bool TranslationEngine::isAvailable()
{
    return true;
}

void TranslationEngine::unloadAll()
{
    m_d->loaded.clear();
}

EngineResult TranslationEngine::translate(const QList<InstalledModel>& route, const QStringList& paragraphs)
{
    EngineResult result;
    if (route.isEmpty() || route.size() > 2) {
        result.error = QStringLiteral("no route between these languages");
        return result;
    }

    std::vector<std::string> sources;
    sources.reserve(static_cast<size_t>(paragraphs.size()));
    for (const QString& paragraph : paragraphs) {
        sources.push_back(paragraph.toStdString());
    }
    const std::vector<bergamot::ResponseOptions> options(sources.size());

    try {
        std::vector<bergamot::Response> responses;
        if (route.size() == 1) {
            responses =
                m_d->service.translateMultiple(m_d->modelFor(route.first()), std::move(sources), options);
        } else {
            responses = m_d->service.pivotMultiple(m_d->modelFor(route.at(0)), m_d->modelFor(route.at(1)),
                                                   std::move(sources), options);
        }
        for (const bergamot::Response& response : responses) {
            result.translations.append(QString::fromStdString(response.target.text));
        }
        result.ok = true;
    } catch (const std::exception& e) {
        m_d->loaded.clear();
        result.error = QString::fromUtf8(e.what());
    }
    return result;
}

#else // !GLATE_HAVE_TRANSLATIONS_ENGINE

struct TranslationEngine::Private
{
};

TranslationEngine::TranslationEngine()
    : m_d(std::make_unique<Private>())
{}

TranslationEngine::~TranslationEngine() = default;

bool TranslationEngine::isAvailable()
{
    return false;
}

void TranslationEngine::unloadAll() {}

EngineResult TranslationEngine::translate(const QList<InstalledModel>& /*route*/,
                                          const QStringList& /*paragraphs*/)
{
    return EngineResult{.ok = false,
                        .translations = {},
                        .error = QStringLiteral("this build of glate has no offline engine")};
}

#endif

} // namespace glate::offline
