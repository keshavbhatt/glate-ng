#include "ModelCatalog.h"
#include "test_catalog.h"

#include <QTest>

using glate::offline::parseCatalog;
using namespace glate::test;

class TestModelCatalog : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesAValidCatalogue()
    {
        QString error;
        const auto catalog = parseCatalog(catalogJson({sampleEntry()}), &error);
        QVERIFY2(catalog.has_value(), qPrintable(error));
        QCOMPARE(catalog->models.size(), 1);
        const auto* entry = catalog->find(QStringLiteral("en"), QStringLiteral("de"));
        QVERIFY(entry != nullptr);
        QCOMPARE(entry->id(), QStringLiteral("en-de"));
        QCOMPARE(entry->version, QStringLiteral("2.1"));
        QCOMPARE(entry->totalSize(), qint64(14 + 12 + 14));
        QVERIFY(entry->file(u"lex") != nullptr);
        QCOMPARE(catalog->urlFor(*entry->file(u"model")),
                 QUrl(QStringLiteral("https://example.org/models/x-model.ende.bin")));
        QVERIFY(catalog->find(QStringLiteral("de-en")) == nullptr);
    }

    void refusesOtherSchemaVersions()
    {
        QJsonObject root = QJsonDocument::fromJson(catalogJson({sampleEntry()})).object();
        root[QStringLiteral("schema_version")] = 2;
        QString error;
        QVERIFY(!parseCatalog(QJsonDocument(root).toJson(), &error));
        QVERIFY(error.contains(QStringLiteral("schema_version")));
    }

    void refusesPlainHttp()
    {
        QVERIFY(!parseCatalog(catalogJson({sampleEntry()}, QStringLiteral("http://example.org/"))));
    }

    void refusesUnsafeFileNames_data()
    {
        QTest::addColumn<QString>("name");
        QTest::newRow("parent") << QStringLiteral("../model.bin");
        QTest::newRow("slash") << QStringLiteral("a/model.bin");
        QTest::newRow("hidden") << QStringLiteral(".model.bin");
        QTest::newRow("dots") << QStringLiteral("model..bin");
    }

    void refusesUnsafeFileNames()
    {
        QFETCH(QString, name); // NOLINT(misc-const-correctness): the macro declares it
        const QJsonObject entry = entryJson(QStringLiteral("en"), QStringLiteral("de"), QStringLiteral("1"),
                                            {
                                                fileJson(QStringLiteral("model"), name, "m"),
                                                fileJson(QStringLiteral("lex"), QStringLiteral("l"), "l"),
                                                fileJson(QStringLiteral("vocab"), QStringLiteral("v"), "v"),
                                            });
        QVERIFY(!parseCatalog(catalogJson({entry})));
    }

    void needsModelLexAndVocabulary()
    {
        const QJsonObject noLex = entryJson(QStringLiteral("en"), QStringLiteral("de"), QStringLiteral("1"),
                                            {
                                                fileJson(QStringLiteral("model"), QStringLiteral("m"), "m"),
                                                fileJson(QStringLiteral("vocab"), QStringLiteral("v"), "v"),
                                            });
        QVERIFY(!parseCatalog(catalogJson({noLex})));

        const QJsonObject splitVocab =
            entryJson(QStringLiteral("zh-Hans"), QStringLiteral("en"), QStringLiteral("1"),
                      {
                          fileJson(QStringLiteral("model"), QStringLiteral("m"), "m"),
                          fileJson(QStringLiteral("lex"), QStringLiteral("l"), "l"),
                          fileJson(QStringLiteral("srcvocab"), QStringLiteral("s"), "s"),
                          fileJson(QStringLiteral("trgvocab"), QStringLiteral("t"), "t"),
                      });
        QVERIFY(parseCatalog(catalogJson({splitVocab})));
    }

    void refusesDuplicates()
    {
        QVERIFY(!parseCatalog(catalogJson({sampleEntry(), sampleEntry(QStringLiteral("2.2"))})));
    }

    void refusesGarbage()
    {
        QString error;
        QVERIFY(!parseCatalog("not json", &error));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestModelCatalog)
#include "tst_model_catalog.moc"
