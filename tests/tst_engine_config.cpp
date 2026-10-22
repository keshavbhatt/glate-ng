#include "EngineConfig.h"

#include <QTest>

using glate::offline::InstalledModel;

class TestEngineConfig : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void sharedVocabularyIsUsedTwice()
    {
        InstalledModel model;
        model.gemmPrecision = QStringLiteral("int8shiftAll");
        model.files = {
            {QStringLiteral("model"), QStringLiteral("/m/model.bin")},
            {QStringLiteral("lex"), QStringLiteral("/m/lex.bin")},
            {QStringLiteral("vocab"), QStringLiteral("/m/vocab.spm")},
        };
        const QString yaml = QString::fromStdString(glate::offline::engineConfig(model));
        QVERIFY(yaml.contains(QStringLiteral("models: [\"/m/model.bin\"]\n")));
        QVERIFY(yaml.contains(QStringLiteral("vocabs: [\"/m/vocab.spm\", \"/m/vocab.spm\"]\n")));
        QVERIFY(yaml.contains(QStringLiteral("shortlist: [\"/m/lex.bin\", false]\n")));
        QVERIFY(yaml.contains(QStringLiteral("gemm-precision: int8shiftAll\n")));
        QVERIFY(yaml.contains(QStringLiteral("beam-size: 1\n")));
    }

    void splitVocabulariesKeepTheirOrder()
    {
        InstalledModel model;
        model.files = {
            {QStringLiteral("model"), QStringLiteral("/m/model.bin")},
            {QStringLiteral("lex"), QStringLiteral("/m/lex.bin")},
            {QStringLiteral("srcvocab"), QStringLiteral("/m/src.spm")},
            {QStringLiteral("trgvocab"), QStringLiteral("/m/trg.spm")},
        };
        const QString yaml = QString::fromStdString(glate::offline::engineConfig(model));
        QVERIFY(yaml.contains(QStringLiteral("vocabs: [\"/m/src.spm\", \"/m/trg.spm\"]\n")));
        QVERIFY(yaml.contains(QStringLiteral("gemm-precision: int8shiftAlphaAll\n")));
    }

    void pathsAreQuotedAndEscaped()
    {
        InstalledModel model;
        model.files = {
            {QStringLiteral("model"), QStringLiteral("/home/a \"b\"/model.bin")},
            {QStringLiteral("lex"), QStringLiteral("/l")},
            {QStringLiteral("vocab"), QStringLiteral("/v")},
        };
        const QString yaml = QString::fromStdString(glate::offline::engineConfig(model));
        QVERIFY(yaml.contains(QStringLiteral("models: [\"/home/a \\\"b\\\"/model.bin\"]")));
    }
};

QTEST_GUILESS_MAIN(TestEngineConfig)
#include "tst_engine_config.moc"
