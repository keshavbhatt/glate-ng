#include "glateshell.h"

#include "AboutDialog.h"
#include "BugReportDialog.h"
#include "Changelog.h"
#include "Diagnostics.h"
#include "LogSink.h"
#include "Logging.h"
#include "ShortcutsDialog.h"
#include "WhatsNewDialog.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <utility>

using namespace Qt::StringLiterals;
using namespace glate::shell;

namespace {

constexpr auto kSeenVersionKey = "shell/whatsNewSeenVersion";
constexpr int kReportLogLines = 200;

const QString kRepository = u"https://github.com/keshavbhatt/glate-ng"_s;

QList<Release> bundledReleases()
{
    QFile file(u":/meta/com.ktechpit.glate.metainfo.xml"_s);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcApp) << "release notes are missing from the resources";
        return {};
    }
    return parseReleases(file.readAll());
}

QUrl rateUrl()
{
    const QString package = packageType();
    if (package == u"flatpak"_s)
        return QUrl(u"https://flathub.org/apps/com.ktechpit.glate"_s);
    if (package == u"snap"_s)
        return QUrl(u"snap://glate"_s);
    return QUrl(u"https://snapcraft.io/glate"_s);
}

QList<AboutInfo::Row> credits()
{
    return {
        {QApplication::translate("GlateShell", "Offline translation"),
         QApplication::translate(
             "GlateShell", "Engine and models by Mozilla (Firefox Translations), MPL-2.0. Models are "
                           "downloaded on demand from this project's mirror.")},
        {QApplication::translate("GlateShell", "Online translation"),
         QApplication::translate("GlateShell", "Google Translate web service.")},
        {QApplication::translate("GlateShell", "App name"),
         QApplication::translate("GlateShell", "Alan Pope helped choose the name.")},
        {QApplication::translate("GlateShell", "Interface translations"),
         QApplication::translate("GlateShell",
                                         "German, French, Brazilian Portuguese and Italian.")},
    };
}

QList<AboutInfo::Row> openSource()
{
    const QString licensing = kRepository + u"/blob/main/LICENSING.md"_s;
    return {
        {u"Glate"_s,
         QApplication::translate("GlateShell",
                                         "GPL-3.0-or-later; the account and licence code is "
                                         "source-available (<a href=\"%1\">LICENSING.md</a>)")
                     .arg(licensing)},
        {u"Qt %1"_s.arg(QString::fromLatin1(qVersion())), u"LGPL-3.0"_s},
        {QApplication::translate("GlateShell", "Mozilla translations engine"),
         QApplication::translate("GlateShell", "MPL-2.0, with Marian, sentencepiece, intgemm, ruy "
                                                       "and ssplit (MIT, Apache-2.0, BSD)")},
        {u"QtWaitingSpinner"_s, u"MIT"_s},
        {u"Remix Icon"_s, u"Apache-2.0"_s},
    };
}

} // namespace

GlateShell::GlateShell(QWidget* window, std::function<QStringList()> appLines)
    : QObject(window)
    , m_window(window)
    , m_appLines(std::move(appLines))
{
}

QString GlateShell::logDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(u"logs"_s);
}

QString GlateShell::logFilePath()
{
    return QDir(logDirectory()).filePath(u"glate.log"_s);
}

QString GlateShell::diagnostics() const
{
    return diagnosticsText(m_appLines ? m_appLines() : QStringList());
}

void GlateShell::present(QDialog* dialog)
{
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::NonModal);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void GlateShell::showAbout()
{
    if (m_about) {
        m_about->raise();
        m_about->activateWindow();
        return;
    }
    AboutInfo info;
    info.appName = QApplication::applicationName();
    info.tagline = tr("Translate text and listen to it, online or on your computer.");
    info.version = QApplication::applicationVersion();
    info.author = tr("Designed and developed by Keshav Bhatt, ktechpit.com");
    info.icon = QIcon(u":/icons/app/icon-128.png"_s);
    info.links = {
        {tr("Report a bug..."), u":/icons/bug-line.png"_s, {}},
        {tr("Source code"), u":/icons/code-s-slash-line.png"_s, QUrl(kRepository)},
        {tr("Website"),
         u":/icons/global-line.png"_s,
         QUrl(u"https://ktechpit.com/USS/public/product.php?slug=glate"_s)},
        {tr("Donate"), u":/icons/heart-line.png"_s, QUrl(u"https://paypal.me/keshavnrj/9"_s)},
        {tr("More apps"),
         u":/icons/apps-line.png"_s,
         QUrl(u"https://snapcraft.io/publisher/keshavnrj"_s)},
        {tr("Rate"), u":/icons/star-line.png"_s, rateUrl()},
    };
    info.credits = credits();
    info.openSource = openSource();
    info.diagnostics = diagnostics();
    info.logDirectory = logDirectory();
    auto* about = new AboutDialog(info, m_window);
    connect(about, &AboutDialog::whatsNewRequested, this, &GlateShell::showWhatsNew);
    connect(about, &AboutDialog::bugReportRequested, this, [this]() { showBugReport(); });
    m_about = about;
    present(about);
}

void GlateShell::showWhatsNew()
{
    if (m_whatsNew) {
        m_whatsNew->raise();
        m_whatsNew->activateWindow();
        return;
    }
    m_whatsNew = new WhatsNewDialog(QApplication::applicationName(), bundledReleases(),
                                    QApplication::applicationVersion(), m_window);
    present(m_whatsNew);
}

void GlateShell::showBugReport(const QString& description)
{
    if (m_bugReport)
        m_bugReport->close();
    auto* report =
        new BugReportDialog(diagnostics(), LogSink::recentLines(kReportLogLines),
                            {QUrl(kRepository + u"/issues/new"_s), u"keshavnrj@gmail.com"_s},
                            m_window);
    if (!description.isEmpty())
        report->setDescription(description);
    m_bugReport = report;
    present(report);
}

void GlateShell::showShortcuts()
{
    if (m_shortcuts) {
        m_shortcuts->raise();
        return;
    }
    m_shortcuts = new ShortcutsDialog(
        {
            {u"Shift+Return"_s, tr("Translate")},
            {u"F1"_s, tr("Keyboard shortcuts")},
            {u"Esc"_s, tr("Close a window")},
            {u"Return"_s, tr("History: load the selected translation")},
            {u"Del"_s, tr("History: remove the selected translation")},
        },
        m_window);
    present(m_shortcuts);
}

void GlateShell::showWhatsNewIfUpdated()
{
    QSettings settings;
    const QString current = QApplication::applicationVersion();
    QString seen = settings.value(QLatin1String(kSeenVersionKey)).toString();
    // Before 4.5 nothing recorded the version; "firstRun" marks an existing install.
    if (seen.isEmpty() && settings.contains(u"firstRun"_s))
        seen = u"4.4"_s;
    settings.setValue(QLatin1String(kSeenVersionKey), current);
    if (shouldShowWhatsNew(seen, current)) {
        qCInfo(lcApp) << "updated from" << seen << "to" << current;
        // After the main window is up, so it opens on top of it.
        QTimer::singleShot(800, this, &GlateShell::showWhatsNew);
    }
}

QMenu* GlateShell::createHelpMenu(QWidget* parent, const std::function<void()>& tour)
{
    auto* menu = new QMenu(parent);
    menu->setMinimumWidth(230);
    menu->addAction(QIcon(u":/icons/sparkling-line.png"_s), tr("Welcome tour"), this, tour);
    menu->addAction(QIcon(u":/icons/star-line.png"_s),
                    tr("What's new in %1").arg(QApplication::applicationVersion()), this,
                    &GlateShell::showWhatsNew);
    QAction* keys =
        menu->addAction(QIcon(u":/icons/keyboard-line.png"_s), tr("Keyboard shortcuts"), this, &GlateShell::showShortcuts);
    keys->setShortcut(QKeySequence(u"F1"_s));
    menu->addSeparator();
    menu->addAction(QIcon(u":/icons/bug-line.png"_s), tr("Report a bug..."), this, [this]() { showBugReport(); });
    menu->addAction(QIcon(u":/icons/folder-line.png"_s), tr("Open log folder"), this, []() {
        QDir().mkpath(logDirectory());
        QDesktopServices::openUrl(QUrl::fromLocalFile(logDirectory()));
    });
    return menu;
}
