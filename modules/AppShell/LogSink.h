#pragma once

#include <QString>
#include <QStringList>

namespace glate::shell {

// Process-wide log sink, ported from omnidict. Installs a Qt message handler
// that formats every message once, forwards it to the previous handler (stderr,
// QTest), appends it to an optional log file and keeps the last lines in memory
// for bug reports.
class LogSink
{
public:
    // Idempotent. Safe to call before QCoreApplication exists.
    static void install();

    // Starts (or switches) file logging; an empty path turns the file off.
    // Rotates <path> to <path>.1 when it grows past maxBytes.
    static void setLogFile(const QString& path, qint64 maxBytes = qint64{2} * 1024 * 1024);
    [[nodiscard]] static QString logFilePath();

    // Most recent lines, oldest first (at most 1000).
    [[nodiscard]] static QStringList recentLines(int count = 1000);
};

} // namespace glate::shell
