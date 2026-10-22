#include "ModelManager.h"
#include "ModelStore.h"
#include "offlinemodelsdialog.h"
#include "test_catalog.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>

using glate::offline::ModelManager;
using glate::offline::ModelStore;
using namespace glate::test;

// Drives the languages dialog against a catalogue served from file:// URLs.
// GLATE_TEST_GRAB=<file.png> saves a picture of the dialog for review.
class TestOfflineModelsDialog : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    // German and French, each with both directions, three small files apiece.
    QUrl publish()
    {
        const QString served = QDir(m_dir.path()).filePath(QStringLiteral("served"));
        QDir().mkpath(served);
        QJsonArray models;
        for (const auto& [from, to] : {
                 std::pair{u"de", u"en"},
                 std::pair{u"en", u"de"},
                 std::pair{u"fr", u"en"},
                 std::pair{u"en", u"fr"},
             }) {
            const QString pair = QString(from) + QString(to);
            QJsonArray files;
            for (const auto& [kind, name] : {
                     std::pair{QStringLiteral("model"), QStringLiteral("model.%1.bin")},
                     std::pair{QStringLiteral("lex"), QStringLiteral("lex.%1.bin")},
                     std::pair{QStringLiteral("vocab"), QStringLiteral("vocab.%1.spm")},
                 }) {
                const QString fileName = name.arg(pair);
                const QByteArray content = fileName.toUtf8().repeated(1000);
                QFile out(QDir(served).filePath(QStringLiteral("x-") + fileName));
                if (!out.open(QIODevice::WriteOnly)) {
                    return {}; // the test then fails on the empty URL
                }
                out.write(content);
                files.append(fileJson(kind, fileName, content));
            }
            models.append(entryJson(QString(from), QString(to), QStringLiteral("1.0"), files));
        }
        QFile catalog(QDir(served).filePath(QStringLiteral("catalog.json")));
        if (catalog.open(QIODevice::WriteOnly)) {
            catalog.write(catalogJson(models, QUrl::fromLocalFile(served + u'/').toString()));
        }
        return QUrl::fromLocalFile(catalog.fileName());
    }

    static int rowOf(const QTableWidget* table, const QString& name)
    {
        for (int row = 0; row < table->rowCount(); ++row) {
            if (table->item(row, 0)->text() == name) {
                return row;
            }
        }
        return -1;
    }

private Q_SLOTS:
    void downloadsALanguage()
    {
        ModelStore store(QDir(m_dir.path()).filePath(QStringLiteral("models")));
        ModelManager manager(&store, publish());
        OfflineModelsDialog dialog(&manager, &store,
                                   {
                                       {QStringLiteral("de"), QStringLiteral("German")},
                                       {QStringLiteral("fr"), QStringLiteral("French")},
                                   });
        dialog.show();

        const auto* table = dialog.findChild<QTableWidget*>();
        QVERIFY(table != nullptr);
        QTRY_COMPARE(table->rowCount(), 2);
        QCOMPARE(table->item(0, 0)->text(), QStringLiteral("French")); // sorted by name

        const int german = rowOf(table, QStringLiteral("German"));
        QCOMPARE(dialog.directionsFor(QStringLiteral("de")),
                 (QStringList{QStringLiteral("de-en"), QStringLiteral("en-de")}));
        auto* button = qobject_cast<QPushButton*>(table->cellWidget(german, 3));
        QCOMPARE(button->text(), QStringLiteral("Download"));

        const QSignalSpy installed(&manager, &ModelManager::installed);
        button->click();
        QTRY_COMPARE(installed.size(), 2);
        QTRY_COMPARE(table->item(german, 2)->text(), QStringLiteral("Downloaded"));
        QCOMPARE(button->text(), QStringLiteral("Remove"));
        QVERIFY(store.find(QStringLiteral("de-en")).has_value());
        QVERIFY(store.find(QStringLiteral("en-de")).has_value());

        const QByteArray grab = qgetenv("GLATE_TEST_GRAB");
        if (!grab.isEmpty()) {
            dialog.grab().save(QString::fromLocal8Bit(grab));
        }

        button->click();
        QTRY_COMPARE(button->text(), QStringLiteral("Download"));
        QVERIFY(!store.find(QStringLiteral("de-en")).has_value());
    }
};

QTEST_MAIN(TestOfflineModelsDialog)
#include "tst_offline_models_dialog.moc"
