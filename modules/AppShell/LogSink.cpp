#include "LogSink.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>

#include <algorithm>
#include <deque>
#include <iterator>

using namespace Qt::StringLiterals;

namespace glate::shell {

namespace {

constexpr qsizetype kRingCapacity = 1000;

struct State
{
    QMutex mutex;
    QtMessageHandler previous = nullptr;
    bool installed = false;
    QString filePath;
    qint64 maxBytes = 0;
    QFile file;
    std::deque<QString> ring;
};

State& state()
{
    static State s;
    return s;
}

QLatin1StringView levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return "debug"_L1;
    case QtInfoMsg:
        return "info"_L1;
    case QtWarningMsg:
        return "warning"_L1;
    case QtCriticalMsg:
        return "critical"_L1;
    case QtFatalMsg:
        return "fatal"_L1;
    }
    return "?"_L1;
}

QString formatLine(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const QString category =
        context.category != nullptr ? QString::fromLatin1(context.category) : u"default"_s;
    return u"%1 [%2] %3: %4"_s.arg(QDateTime::currentDateTime().toString(u"yyyy-MM-dd hh:mm:ss.zzz"_s),
                                   category, levelName(type), message);
}

bool openForAppend(QFile& file)
{
    return file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

void rotateIfNeeded(State& s)
{
    if (s.maxBytes <= 0 || !s.file.isOpen() || s.file.size() < s.maxBytes) {
        return;
    }
    s.file.close();
    const QString backup = s.filePath + u".1"_s;
    QFile::remove(backup);
    // If the rename fails, keep appending to the current file rather than lose output.
    QFile::rename(s.filePath, backup);
    if (!openForAppend(s.file)) {
        s.filePath.clear();
    }
}

void handler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    State& s = state();
    const QString line = formatLine(type, context, message);
    {
        const QMutexLocker lock(&s.mutex);
        s.ring.push_back(line);
        while (std::ssize(s.ring) > kRingCapacity) {
            s.ring.pop_front();
        }
        if (s.file.isOpen()) {
            s.file.write(line.toUtf8());
            s.file.write("\n");
            s.file.flush();
            rotateIfNeeded(s);
        }
    }
    if (s.previous != nullptr) {
        s.previous(type, context, message);
    }
}

} // namespace

void LogSink::install()
{
    State& s = state();
    const QMutexLocker lock(&s.mutex);
    if (s.installed) {
        return;
    }
    s.installed = true;
    s.previous = qInstallMessageHandler(handler);
}

void LogSink::setLogFile(const QString& path, qint64 maxBytes)
{
    State& s = state();
    const QMutexLocker lock(&s.mutex);
    if (s.file.isOpen()) {
        s.file.close();
    }
    s.filePath = path;
    s.maxBytes = maxBytes;
    if (path.isEmpty()) {
        return;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    s.file.setFileName(path);
    if (!openForAppend(s.file)) {
        s.filePath.clear();
        return;
    }
    rotateIfNeeded(s);
}

QString LogSink::logFilePath()
{
    State& s = state();
    const QMutexLocker lock(&s.mutex);
    return s.filePath;
}

QStringList LogSink::recentLines(int count)
{
    State& s = state();
    const QMutexLocker lock(&s.mutex);
    const auto total = std::ssize(s.ring);
    const auto take = std::min<qsizetype>(total, std::max(count, 0));
    QStringList lines;
    lines.reserve(take);
    for (auto it = s.ring.end() - take; it != s.ring.end(); ++it) {
        lines.append(*it);
    }
    return lines;
}

} // namespace glate::shell
