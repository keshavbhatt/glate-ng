#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

namespace glate::shell {

// One <release> of an AppStream metainfo file: the release notes software
// centres show, reused for the What's new window so they are written once.
struct Release
{
    QString version;
    QString date;        // yyyy-MM-dd, may be empty
    QString description; // HTML subset: <p>, <ul>, <ol>, <li>, <em>, <code>
};

// Releases in file order (newest first in a well-formed metainfo file).
// Returns an empty list for malformed XML.
[[nodiscard]] QList<Release> parseReleases(const QByteArray& metainfoXml);

// True when the app was updated since the user last saw What's new: never on a
// first run (seenVersion empty) or when the version is unchanged.
[[nodiscard]] bool shouldShowWhatsNew(const QString& seenVersion, const QString& currentVersion);

// The release to open on: the running version if it has notes, else the newest.
[[nodiscard]] qsizetype releaseIndexFor(const QList<Release>& releases, const QString& version);

} // namespace glate::shell
