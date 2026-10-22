#include "OfflineTranslator.h"
#include "TranslationEngine.h"

#include <QDir>
#include <QSignalSpy>
#include <QTest>

using glate::offline::ModelStore;
using glate::offline::OfflineTranslator;
using glate::offline::TranslationPlan;

// Plans run everywhere; the translations themselves need the engine and real
// en-de / de-en models (scripts/fetch-test-models.sh), and are skipped without them.
class TestOfflineTranslator : public QObject
{
    Q_OBJECT

private:
    static QString modelsDir() { return QStringLiteral(GLATE_TEST_MODELS); }

    static bool haveModels()
    {
        const ModelStore store(modelsDir());
        return glate::offline::TranslationEngine::isAvailable() && store.find(QStringLiteral("en-de")) &&
               store.find(QStringLiteral("de-en"));
    }

private Q_SLOTS:
    void plansNeedAChosenSourceLanguage()
    {
        if (!glate::offline::TranslationEngine::isAvailable()) {
            QSKIP("built without the engine");
        }
        const ModelStore store(QDir::tempPath() + QStringLiteral("/glate-no-models"));
        const OfflineTranslator translator(&store);
        QCOMPARE(translator.plan(QStringLiteral("auto"), QStringLiteral("de")).status,
                 TranslationPlan::Status::NeedsSourceLanguage);
        QCOMPARE(translator.plan(QStringLiteral("de"), QStringLiteral("de")).status,
                 TranslationPlan::Status::SameLanguage);

        const TranslationPlan pivot = translator.plan(QStringLiteral("de"), QStringLiteral("zh-CN"));
        QCOMPARE(pivot.status, TranslationPlan::Status::MissingModels);
        QCOMPARE(pivot.missing.size(), 2);
        QCOMPARE(pivot.missing.at(1).id(), QStringLiteral("en-zh-Hans"));
    }

    void failsCleanlyWithoutModels()
    {
        const ModelStore store(QDir::tempPath() + QStringLiteral("/glate-no-models"));
        OfflineTranslator translator(&store);
        QSignalSpy failed(&translator, &OfflineTranslator::failed);
        QCOMPARE(translator.translate(QStringLiteral("en"), QStringLiteral("de"), QStringLiteral("Hello")),
                 0U);
        QVERIFY(failed.wait());
        QCOMPARE(failed.first().first().toULongLong(), 0ULL); // refusals carry id 0
    }

    void translatesDirectly()
    {
        if (!haveModels()) {
            QSKIP("no en-de / de-en models in GLATE_TEST_MODELS");
        }
        const ModelStore store(modelsDir());
        OfflineTranslator translator(&store);
        QSignalSpy finished(&translator, &OfflineTranslator::finished);
        const QSignalSpy failed(&translator, &OfflineTranslator::failed);
        const quint64 id =
            translator.translate(QStringLiteral("en"), QStringLiteral("de"),
                                 QStringLiteral("I like to drink coffee.\n\nThe weather is nice."));
        QVERIFY(id != 0);
        QVERIFY(finished.wait(60'000));
        QVERIFY(failed.isEmpty());
        const auto pairs = finished.first().at(1).value<QList<QStringList>>();
        QCOMPARE(pairs.size(), 3);
        QVERIFY2(pairs.at(0).at(0).contains(QStringLiteral("Kaffee")), qPrintable(pairs.at(0).at(0)));
        QCOMPARE(pairs.at(0).at(1), QStringLiteral("I like to drink coffee."));
        QVERIFY(pairs.at(1).at(0).isEmpty());
        QVERIFY(pairs.at(2).at(0).contains(QStringLiteral("Wetter")));
    }

    void roundTripsThroughBothDirections()
    {
        if (!haveModels()) {
            QSKIP("no en-de / de-en models in GLATE_TEST_MODELS");
        }
        const ModelStore store(modelsDir());
        OfflineTranslator translator(&store);
        QSignalSpy finished(&translator, &OfflineTranslator::finished);
        translator.translate(QStringLiteral("de"), QStringLiteral("en"),
                             QStringLiteral("Guten Morgen, wie geht es dir?"));
        QVERIFY(finished.wait(60'000));
        const auto pairs = finished.first().at(1).value<QList<QStringList>>();
        QVERIFY2(pairs.at(0).at(0).contains(QStringLiteral("morning"), Qt::CaseInsensitive),
                 qPrintable(pairs.at(0).at(0)));
    }
};

QTEST_GUILESS_MAIN(TestOfflineTranslator)
#include "tst_offline_translator.moc"
