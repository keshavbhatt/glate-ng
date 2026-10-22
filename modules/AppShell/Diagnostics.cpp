#include "Diagnostics.h"

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QSysInfo>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

namespace glate::shell {

QString packageType()
{
    if (qEnvironmentVariableIsSet("SNAP")) {
        return u"snap"_s;
    }
    if (qEnvironmentVariableIsSet("FLATPAK_ID")) {
        return u"flatpak"_s;
    }
    return u"none"_s;
}

QString redactHome(QString text)
{
    const QString home = QDir::homePath();
    if (home.size() > 1) {
        text.replace(home, u"~"_s);
    }
    return text;
}

QString redactUrlQueries(const QString& text)
{
    // Qt writes "Error transferring <url> - server replied: ...", with spaces decoded,
    // so a query runs up to that marker when it is there.
    static const QRegularExpression kQuery(u"(https?://[^\\s?#]+)[?#](?:.*?(?= - server replied:)|\\S*)"_s);
    QString result = text;
    return result.replace(kQuery, u"\\1?..."_s);
}

QString diagnosticsText(const QStringList& appLines)
{
    QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    if (desktop.isEmpty()) {
        desktop = u"unknown"_s;
    }
    QStringList lines{
        u"%1 %2"_s.arg(QCoreApplication::applicationName(), QCoreApplication::applicationVersion()),
        u"Qt %1 (built with %2)"_s.arg(QString::fromLatin1(qVersion()), QStringLiteral(QT_VERSION_STR)),
        u"OS: %1, kernel %2, %3"_s.arg(QSysInfo::prettyProductName(), QSysInfo::kernelVersion(),
                                       QSysInfo::currentCpuArchitecture()),
        u"Session: %1 (%2), package: %3"_s.arg(QGuiApplication::platformName(), desktop, packageType()),
    };
    lines.append(appLines);
    return redactHome(lines.join(u'\n'));
}

QString reportBody(const BugReport& report)
{
    const QString described = report.description.trimmed().isEmpty()
                                  ? QCoreApplication::translate("glate::shell", "(no description given)")
                                  : report.description.trimmed();
    return described + u"\n\n"_s +
           QCoreApplication::translate("glate::shell",
                                       "The details are on your clipboard: paste them below this line.");
}

QUrl issueUrl(const QUrl& newIssueUrl, const BugReport& report)
{
    QUrl url(newIssueUrl);
    QUrlQuery query;
    query.addQueryItem(u"title"_s, report.title.trimmed());
    query.addQueryItem(u"body"_s, reportBody(report));
    url.setQuery(query);
    return url;
}

QUrl mailUrl(const QString& address, const BugReport& report)
{
    QUrl url(u"mailto:"_s + address);
    QUrlQuery query;
    query.addQueryItem(u"subject"_s, report.title.trimmed());
    query.addQueryItem(u"body"_s, reportBody(report));
    url.setQuery(query);
    return url;
}

} // namespace glate::shell
