#include "ModelStore.h"
#include "test_catalog.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using glate::offline::ModelStore;
using glate::offline::parseCatalog;

namespace {

// Writes the sample entry's files into the staging directory, as a download would.
void stage(const ModelStore& store, const glate::offline::ModelEntry& entry)
{
    const QString dir = store.stagingDirectoryFor(entry.id());
    QVERIFY(QDir().mkpath(dir));
    for (const auto& file : entry.files) {
        QFile out(QDir(dir).filePath(file.name));
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(file.name.toUtf8());
    }
}

glate::offline::ModelEntry sample(const QString& version = QStringLiteral("2.1"))
{
    return parseCatalog(glate::test::catalogJson({glate::test::sampleEntry(version)}))->models.first();
}

} // namespace

class TestModelStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void startsEmpty()
    {
        const QTemporaryDir root;
        const ModelStore store(root.path());
        QVERIFY(store.installed().isEmpty());
        QVERIFY(!store.find(QStringLiteral("en"), QStringLiteral("de")));
    }

    void commitInstallsAndFinds()
    {
        const QTemporaryDir root;
        const ModelStore store(root.path());
        const auto entry = sample();
        stage(store, entry);

        QString error;
        QVERIFY2(store.commit(entry, &error), qPrintable(error));
        QVERIFY(!QFileInfo::exists(store.stagingDirectoryFor(entry.id())));

        const auto installed = store.find(QStringLiteral("en"), QStringLiteral("de"));
        QVERIFY(installed.has_value());
        QCOMPARE(installed->version, QStringLiteral("2.1"));
        QCOMPARE(installed->gemmPrecision, QStringLiteral("int8shiftAlphaAll"));
        QVERIFY(
            installed->files.value(QStringLiteral("model")).endsWith(QStringLiteral("en-de/model.ende.bin")));
        QCOMPARE(installed->size, entry.totalSize());
        QCOMPARE(store.installed().size(), 1);
    }

    void commitReplacesAnOlderVersion()
    {
        const QTemporaryDir root;
        const ModelStore store(root.path());
        stage(store, sample(QStringLiteral("1.0")));
        QVERIFY(store.commit(sample(QStringLiteral("1.0"))));
        stage(store, sample(QStringLiteral("2.1")));
        QVERIFY(store.commit(sample(QStringLiteral("2.1"))));

        QCOMPARE(store.find(QStringLiteral("en-de"))->version, QStringLiteral("2.1"));
        QVERIFY(!QFileInfo::exists(store.directoryFor(QStringLiteral("en-de")) + QStringLiteral(".old")));
    }

    void aDamagedInstallIsNotListed()
    {
        const QTemporaryDir root;
        const ModelStore store(root.path());
        const auto entry = sample();
        stage(store, entry);
        QVERIFY(store.commit(entry));

        QFile lex(QDir(store.directoryFor(entry.id())).filePath(QStringLiteral("lex.ende.bin")));
        QVERIFY(lex.open(QIODevice::Append));
        lex.write("extra");
        lex.close();
        QVERIFY(!store.find(entry.id()));
        QVERIFY(store.installed().isEmpty());
    }

    void removeDeletesTheDirectory()
    {
        const QTemporaryDir root;
        const ModelStore store(root.path());
        const auto entry = sample();
        stage(store, entry);
        QVERIFY(store.commit(entry));
        QVERIFY(store.remove(entry.id()));
        QVERIFY(!QFileInfo::exists(store.directoryFor(entry.id())));
        QVERIFY(!store.remove(entry.id()));
    }

    void refusesIdsThatLeaveTheRoot()
    {
        const QTemporaryDir root;
        const ModelStore store(root.path());
        QVERIFY(!store.find(QStringLiteral("../en-de")));
        QVERIFY(!store.find(QStringLiteral(".staging")));
        QVERIFY(!store.remove(QStringLiteral("..")));
    }
};

QTEST_GUILESS_MAIN(TestModelStore)
#include "tst_model_store.moc"
