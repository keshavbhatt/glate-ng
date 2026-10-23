#include "AboutDialog.h"
#include "BugReportDialog.h"
#include "StatusPill.h"
#include "WelcomeCard.h"
#include "WhatsNewDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTabWidget>
#include <QTest>
#include <QTextBrowser>

using namespace Qt::StringLiterals;
using namespace glate::shell;

namespace {

QList<Release> sampleReleases()
{
    return {
        {.version = u"2.0"_s, .date = u"2026-10-22"_s, .description = u"<p>Two</p>"_s},
        {.version = u"1.0"_s, .date = u"2026-01-01"_s, .description = u"<p>One</p>"_s},
    };
}

} // namespace

class TestShellDialogs : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void whatsNewOpensOnTheRunningVersion()
    {
        const WhatsNewDialog dialog(u"App"_s, sampleReleases(), u"1.0"_s);
        QCOMPARE(dialog.shownVersion(), u"1.0"_s);
        const auto* notes = dialog.findChild<QTextBrowser*>(u"whatsNewNotes"_s);
        QVERIFY(notes->toPlainText().contains(u"One"_s));
        auto* picker = dialog.findChild<QComboBox*>(u"versionPicker"_s);
        QCOMPARE(picker->itemText(0), u"2.0 (latest)"_s);
        picker->setCurrentIndex(0);
        QVERIFY(notes->toPlainText().contains(u"Two"_s));
    }

    void whatsNewFallsBackToTheNewest()
    {
        const WhatsNewDialog dialog(u"App"_s, sampleReleases(), u"9.9"_s);
        QCOMPARE(dialog.shownVersion(), u"2.0"_s);
    }

    void aboutShowsVersionTabsAndSignals()
    {
        AboutInfo info;
        info.appName = u"App"_s;
        info.version = u"2.0"_s;
        info.links = {
            {.text = u"Report a bug"_s, .iconPath = {}, .url = {}},
            {.text = u"Site"_s, .iconPath = {}, .url = QUrl(u"https://example.org"_s)},
        };
        info.credits = {{.name = u"Engine"_s, .text = u"MPL-2.0"_s}};
        info.diagnostics = u"diag text"_s;
        const AboutDialog dialog(info);
        QVERIFY(dialog.findChild<QLabel*>(u"aboutVersion"_s)->text().contains(u"2.0"_s));
        QCOMPARE(dialog.findChild<QTabWidget*>(u"aboutTabs"_s)->count(), 3);
        QCOMPARE(dialog.findChild<QPlainTextEdit*>(u"aboutDiagnostics"_s)->toPlainText(), u"diag text"_s);

        const QSignalSpy bug(&dialog, &AboutDialog::bugReportRequested);
        dialog.findChild<QPushButton*>(u"aboutBugReport"_s)->click();
        QCOMPARE(bug.count(), 1);
        const QSignalSpy whatsNew(&dialog, &AboutDialog::whatsNewRequested);
        Q_EMIT dialog.findChild<QLabel*>(u"aboutWhatsNew"_s)->linkActivated(u"#"_s);
        QCOMPARE(whatsNew.count(), 1);
    }

    void aboutKeepsItsSizeAcrossTabs()
    {
        AboutInfo info;
        info.appName = u"App"_s;
        info.credits = {{.name = u"Engine"_s, .text = u"MPL-2.0"_s}};
        for (int i = 0; i < 40; ++i) {
            info.openSource.append({.name = u"Library %1"_s.arg(i), .text = u"MIT"_s});
        }
        info.diagnostics = u"one\ntwo\nthree"_s;
        AboutDialog dialog(info);
        dialog.show();
        auto* tabs = dialog.findChild<QTabWidget*>(u"aboutTabs"_s);
        const QSize size = dialog.size();
        for (int i = 0; i < tabs->count(); ++i) {
            tabs->setCurrentIndex(i);
            QCOMPARE(dialog.size(), size);
        }
        // The long list scrolls instead of stretching the window off the screen.
        const auto* scroll = qobject_cast<QScrollArea*>(tabs->widget(1));
        QVERIFY(scroll != nullptr);
        QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
    }

    void bugReportDetailsFollowTheLogBox()
    {
        BugReportDialog dialog(
            u"system"_s, {u"line one"_s, u"line two"_s},
            {.newIssueUrl = QUrl(u"https://example.org/new"_s), .mailAddress = u"a@b.c"_s});
        dialog.findChild<QLineEdit*>(u"bugTitle"_s)->setText(u"Title"_s);
        dialog.setDescription(u"It failed"_s);
        QCOMPARE(dialog.report().title, u"Title"_s);
        QCOMPARE(dialog.report().description, u"It failed"_s);
        QVERIFY(dialog.details().contains(u"line two"_s));
        dialog.findChild<QCheckBox*>(u"includeLog"_s)->setChecked(false);
        QCOMPARE(dialog.details(), u"system"_s);
        QCOMPARE(dialog.findChild<QPlainTextEdit*>(u"detailsView"_s)->toPlainText(), u"system"_s);
    }

    void bugReportWithoutLogHidesTheBox()
    {
        const BugReportDialog dialog(u"system"_s, {}, {});
        QVERIFY(!dialog.findChild<QCheckBox*>(u"includeLog"_s)->isChecked());
        QCOMPARE(dialog.details(), u"system"_s);
    }

    void welcomeCardButtonSignals()
    {
        const WelcomeCard card(u"Welcome"_s, u"Sub"_s, {{.iconPath = u":/none.png"_s, .text = u"Tip"_s}},
                               u"Tour"_s);
        const QSignalSpy clicked(&card, &WelcomeCard::buttonClicked);
        card.findChild<QPushButton*>(u"welcomeButton"_s)->click();
        QCOMPARE(clicked.count(), 1);
    }

    void statusPillIsClickableAndSized()
    {
        StatusPill pill(u"Offline"_s, u":/none.png"_s);
        QVERIFY(pill.sizeHint().width() > pill.fontMetrics().horizontalAdvance(u"Offline"_s));
        const QSignalSpy clicked(&pill, &StatusPill::clicked);
        pill.click();
        QCOMPARE(clicked.count(), 1);
    }
};

QTEST_MAIN(TestShellDialogs)
#include "tst_shell_dialogs.moc"
