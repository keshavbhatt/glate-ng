#include "ModelManager.h"
#include "test_catalog.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using glate::offline::ModelManager;
using glate::offline::ModelStore;
using namespace glate::test;

namespace {

void writeFile(const QString& path, const QByteArray& content)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

} // namespace

// Serves a catalogue and its assets from file:// URLs, which Qt Network reads
// through the same reply interface as https.
class TestModelManager : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_served;
    QUrl m_catalogUrl;

    void publish(const QJsonArray& models)
    {
        writeFile(QDir(m_served).filePath(QStringLiteral("catalog.json")),
                  catalogJson(models, QUrl::fromLocalFile(m_served + u'/').toString()));
    }

private Q_SLOTS:
    void init()
    {
        QVERIFY(m_dir.isValid());
        m_served = QDir(m_dir.path()).filePath(QStringLiteral("served"));
        QDir(m_served).removeRecursively();
        QDir(m_dir.path()).removeRecursively();
        QVERIFY(QDir().mkpath(m_served));
        for (const QString& name : {
                 QStringLiteral("model.ende.bin"),
                 QStringLiteral("lex.ende.bin"),
                 QStringLiteral("vocab.ende.spm"),
             }) {
            writeFile(QDir(m_served).filePath(QStringLiteral("x-") + name), name.toUtf8());
        }
        m_catalogUrl = QUrl::fromLocalFile(QDir(m_served).filePath(QStringLiteral("catalog.json")));
        publish({sampleEntry()});
    }

    void fetchesAndCachesTheCatalogue()
    {
        ModelStore store(QDir(m_dir.path()).filePath(QStringLiteral("models")));
        {
            ModelManager manager(&store, m_catalogUrl);
            QSignalSpy changed(&manager, &ModelManager::catalogChanged);
            manager.refreshCatalog();
            QVERIFY(changed.wait());
            QVERIFY(manager.catalog().has_value());
            QCOMPARE(manager.catalog()->models.size(), 1);
        }
        const ModelManager again(&store, m_catalogUrl);
        QVERIFY(again.catalog().has_value());

        const ModelManager elsewhere(&store, QUrl(QStringLiteral("https://example.org/other.json")));
        QVERIFY(!elsewhere.catalog().has_value());
    }

    void installsAVerifiedModel()
    {
        ModelStore store(QDir(m_dir.path()).filePath(QStringLiteral("models")));
        ModelManager manager(&store, m_catalogUrl);
        QSignalSpy changed(&manager, &ModelManager::catalogChanged);
        manager.refreshCatalog();
        QVERIFY(changed.wait());

        QSignalSpy installed(&manager, &ModelManager::installed);
        const QSignalSpy progress(&manager, &ModelManager::downloadProgress);
        manager.install(QStringLiteral("en-de"));
        QVERIFY(manager.isQueued(QStringLiteral("en-de")));
        QVERIFY(installed.wait());
        QCOMPARE(installed.first().first().toString(), QStringLiteral("en-de"));
        QVERIFY(!progress.isEmpty());
        QVERIFY(store.find(QStringLiteral("en-de")).has_value());
        QVERIFY(!manager.isQueued(QStringLiteral("en-de")));
        QVERIFY(manager.updatesAvailable().isEmpty());

        const QSignalSpy removed(&manager, &ModelManager::removed);
        QVERIFY(manager.remove(QStringLiteral("en-de")));
        QCOMPARE(removed.size(), 1);
        QVERIFY(!store.find(QStringLiteral("en-de")).has_value());
    }

    void refusesAFileThatDoesNotMatchItsChecksum()
    {
        writeFile(QDir(m_served).filePath(QStringLiteral("x-lex.ende.bin")), "tampered!!!!");
        ModelStore store(QDir(m_dir.path()).filePath(QStringLiteral("models")));
        ModelManager manager(&store, m_catalogUrl);
        QSignalSpy changed(&manager, &ModelManager::catalogChanged);
        manager.refreshCatalog();
        QVERIFY(changed.wait());

        QSignalSpy failed(&manager, &ModelManager::installFailed);
        manager.install(QStringLiteral("en-de"));
        QVERIFY(failed.wait());
        QVERIFY(failed.first().at(1).toString().contains(QStringLiteral("lex.ende.bin")));
        QVERIFY(!store.find(QStringLiteral("en-de")).has_value());
        QVERIFY(!QFileInfo::exists(store.stagingDirectoryFor(QStringLiteral("en-de"))));
    }

    void reportsUpdates()
    {
        ModelStore store(QDir(m_dir.path()).filePath(QStringLiteral("models")));
        ModelManager manager(&store, m_catalogUrl);
        QSignalSpy changed(&manager, &ModelManager::catalogChanged);
        manager.refreshCatalog();
        QVERIFY(changed.wait());
        QSignalSpy installed(&manager, &ModelManager::installed);
        manager.install(QStringLiteral("en-de"));
        QVERIFY(installed.wait());

        publish({sampleEntry(QStringLiteral("2.2"))});
        manager.refreshCatalog();
        QVERIFY(changed.wait());
        QCOMPARE(manager.updatesAvailable(), QStringList{QStringLiteral("en-de")});
    }

    void ignoresUnknownIds()
    {
        ModelStore store(QDir(m_dir.path()).filePath(QStringLiteral("models")));
        ModelManager manager(&store, m_catalogUrl);
        manager.install(QStringLiteral("en-xx"));
        QVERIFY(!manager.isQueued(QStringLiteral("en-xx")));
    }
};

QTEST_GUILESS_MAIN(TestModelManager)
#include "tst_model_manager.moc"
