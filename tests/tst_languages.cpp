#include "Languages.h"

#include <QTest>

using glate::offline::Direction;

class TestLanguages : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void renamesGoogleCodes()
    {
        QCOMPARE(glate::offline::toModelCode(QStringLiteral("zh-CN")), QStringLiteral("zh-Hans"));
        QCOMPARE(glate::offline::toModelCode(QStringLiteral("zh-TW")), QStringLiteral("zh-Hant"));
        QCOMPARE(glate::offline::toModelCode(QStringLiteral("iw")), QStringLiteral("he"));
        QCOMPARE(glate::offline::toModelCode(QStringLiteral("no")), QStringLiteral("nb"));
        QCOMPARE(glate::offline::toModelCode(QStringLiteral("de")), QStringLiteral("de"));
    }

    void renamesBack()
    {
        QCOMPARE(glate::offline::toGlateCode(QStringLiteral("zh-Hans")), QStringLiteral("zh-CN"));
        QCOMPARE(glate::offline::toGlateCode(QStringLiteral("nb")), QStringLiteral("no"));
        QCOMPARE(glate::offline::toGlateCode(QStringLiteral("fr")), QStringLiteral("fr"));
    }

    void directRouteWhenEnglishIsOnOneSide()
    {
        const auto toGerman = glate::offline::route(QStringLiteral("en"), QStringLiteral("de"));
        QCOMPARE(toGerman.size(), 1);
        QCOMPARE(toGerman.first().id(), QStringLiteral("en-de"));

        const auto fromGerman = glate::offline::route(QStringLiteral("de"), QStringLiteral("en"));
        QCOMPARE(fromGerman.size(), 1);
        QCOMPARE(fromGerman.first().id(), QStringLiteral("de-en"));
    }

    void pivotsThroughEnglishOtherwise()
    {
        const auto steps = glate::offline::route(QStringLiteral("de"), QStringLiteral("fr"));
        QCOMPARE(steps.size(), 2);
        QCOMPARE(steps.at(0), (Direction{QStringLiteral("de"), QStringLiteral("en")}));
        QCOMPARE(steps.at(1), (Direction{QStringLiteral("en"), QStringLiteral("fr")}));
    }

    void noRouteForSameOrEmpty()
    {
        QVERIFY(glate::offline::route(QStringLiteral("de"), QStringLiteral("de")).isEmpty());
        QVERIFY(glate::offline::route(QString(), QStringLiteral("de")).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestLanguages)
#include "tst_languages.moc"
