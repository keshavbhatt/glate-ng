#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

namespace glate::shell {

// The system details shown in About and attached to bug reports. Callers add
// their own lines (offline engine state, theme); the text must never contain
// translated text, history or a path under the home directory.
[[nodiscard]] QString diagnosticsText(const QStringList& appLines);

// "snap", "flatpak" or "none", from the sandbox environment.
[[nodiscard]] QString packageType();

// Replaces the home directory in text with "~", for log lines in reports.
[[nodiscard]] QString redactHome(QString text);

// Drops the query of every http(s) URL in text: request URLs carry the text
// being translated, which must stay out of logs and reports.
[[nodiscard]] QString redactUrlQueries(const QString& text);

// A prefilled report. Long details do not fit in a URL, so the body only says
// that they are on the clipboard.
struct BugReport
{
    QString title;
    QString description;
};

[[nodiscard]] QString reportBody(const BugReport& report);
[[nodiscard]] QUrl issueUrl(const QUrl& newIssueUrl, const BugReport& report);
[[nodiscard]] QUrl mailUrl(const QString& address, const BugReport& report);

} // namespace glate::shell
