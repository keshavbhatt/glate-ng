#include "Changelog.h"
#include "Diagnostics.h"
#include "LogSink.h"
#include "Logging.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>

using namespace Qt::StringLiterals;
using namespace glate::shell;

namespace {

constexpr char kMetainfo[] = R"(<?xml version="1.0" encoding="UTF-8"?>
<component type="desktop-application">
  <id>com.example.app</id>
  <description><p>Not a release.</p></description>
  <releases>
    <release version="2.0.0" date="2026-10-22">
      <description>
        <p>Big &amp; better.</p>
        <ul><li>One <code>thing</code></li><li>Two</li></ul>
      </description>
      <url>https://example.org/2.0.0</url>
    </release>
    <release version="1.0" date="2026-01-01">
      <description><p>First.</p></description>
    </release>
  </releases>
</component>)";

} // namespace

class TestAppShell : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesReleasesInOrder()
    {
        const QList<Release> releases = parseReleases(kMetainfo);
        QCOMPARE(releases.size(), 2);
        QCOMPARE(releases.at(0).version, u"2.0.0"_s);
        QCOMPARE(releases.at(0).date, u"2026-10-22"_s);
        QCOMPARE(releases.at(1).version, u"1.0"_s);
    }

    void keepsDescriptionMarkupAndEscapesText()
    {
        const Release release = parseReleases(kMetainfo).first();
        QCOMPARE(release.description,
                 u"<p>Big &amp; better.</p><ul><li>One<code>thing</code></li><li>Two</li></ul>"_s);
    }

    void malformedXmlGivesNoReleases()
    {
        QVERIFY(parseReleases("<releases><release version=\"1\">").isEmpty());
    }

    void bundledMetainfoHasNotesForThisVersion()
    {
        QFile file(QStringLiteral(GLATE_METAINFO_PATH));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QList<Release> releases = parseReleases(file.readAll());
        QVERIFY(!releases.isEmpty());
        QCOMPARE(releases.first().version, QStringLiteral(GLATE_VERSION));
        QVERIFY(!releases.first().description.isEmpty());
    }

    void whatsNewOnlyAfterAnUpdate()
    {
        QVERIFY(!shouldShowWhatsNew({}, u"1.0"_s));       // first run
        QVERIFY(!shouldShowWhatsNew(u"1.0"_s, u"1.0"_s)); // same version
        QVERIFY(shouldShowWhatsNew(u"0.9"_s, u"1.0"_s));  // updated
    }

    void opensOnTheRunningVersionOrTheNewest()
    {
        const QList<Release> releases = parseReleases(kMetainfo);
        QCOMPARE(releaseIndexFor(releases, u"1.0"_s), 1);
        QCOMPARE(releaseIndexFor(releases, u"3.0"_s), 0);
        QCOMPARE(releaseIndexFor({}, u"1.0"_s), -1);
    }

    void diagnosticsHoldNoHomePath()
    {
        const QString text = diagnosticsText({QDir::homePath() + u"/secret"_s});
        QVERIFY(!text.contains(QDir::homePath()));
        QVERIFY(text.contains(u"~/secret"_s));
        QVERIFY(text.contains(u"Qt "_s));
    }

    void urlQueriesAreDropped()
    {
        const QString message =
            u"Error transferring https://host/translate_a/single?client=gtx&q=secret text - "
            u"server replied: Bad Request"_s;
        const QString redacted = redactUrlQueries(message);
        QVERIFY(!redacted.contains(u"secret"_s));
        QVERIFY(!redacted.contains(u"text"_s));
        QVERIFY(redacted.endsWith(u"server replied: Bad Request"_s));
        QVERIFY(redacted.contains(u"https://host/translate_a/single?..."_s));
        QCOMPARE(redactUrlQueries(u"no url here"_s), u"no url here"_s);
    }

    void issueUrlCarriesTitleAndBody()
    {
        const QUrl url = issueUrl(QUrl(u"https://github.com/o/r/issues/new"_s),
                                  {.title = u"  It broke "_s, .description = u"Steps"_s});
        const QUrlQuery query(url);
        QCOMPARE(url.host(), u"github.com"_s);
        QCOMPARE(query.queryItemValue(u"title"_s, QUrl::FullyDecoded), u"It broke"_s);
        QVERIFY(query.queryItemValue(u"body"_s, QUrl::FullyDecoded).startsWith(u"Steps\n\n"_s));
    }

    void mailUrlUsesSubject()
    {
        const QUrl url = mailUrl(u"a@example.org"_s, {.title = u"Hi"_s, .description = {}});
        QCOMPARE(url.scheme(), u"mailto"_s);
        QCOMPARE(QUrlQuery(url).queryItemValue(u"subject"_s), u"Hi"_s);
    }

    void logSinkKeepsLinesAndWritesTheFile()
    {
        LogSink::install();
        LogSink::install(); // idempotent
        const QTemporaryDir dir;
        const QString path = dir.filePath(u"logs/test.log"_s);
        LogSink::setLogFile(path, 400);
        QCOMPARE(LogSink::logFilePath(), path);
        for (int i = 0; i < 20; ++i) {
            qCWarning(lcOffline) << "event" << i;
        }
        const QStringList lines = LogSink::recentLines(3);
        QCOMPARE(lines.size(), 3);
        QVERIFY(lines.last().contains(u"[glate.offline] warning: event 19"_s));
        QVERIFY(QFile::exists(path));
        QVERIFY(QFile::exists(path + u".1"_s)); // rotated past 400 bytes
        LogSink::setLogFile({});
        QVERIFY(LogSink::logFilePath().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestAppShell)
#include "tst_app_shell.moc"
