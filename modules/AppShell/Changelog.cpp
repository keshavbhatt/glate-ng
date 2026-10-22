#include "Changelog.h"

#include <QStringList>
#include <QXmlStreamReader>

using namespace Qt::StringLiterals;

namespace glate::shell {

namespace {

bool isKeptElement(QStringView name)
{
    static const QStringList kKept{u"p"_s, u"ul"_s, u"ol"_s, u"li"_s, u"em"_s, u"code"_s};
    return kKept.contains(name);
}

// Copies a <description> element's markup (the reader sits on its start tag),
// keeping only the AppStream description elements and escaping text.
QString readDescription(QXmlStreamReader& xml)
{
    QString html;
    int depth = 1;
    while (depth > 0 && !xml.atEnd()) {
        switch (xml.readNext()) {
        case QXmlStreamReader::StartElement:
            ++depth;
            if (isKeptElement(xml.name())) {
                html += u'<' + xml.name().toString() + u'>';
            }
            break;
        case QXmlStreamReader::EndElement:
            --depth;
            if (depth > 0 && isKeptElement(xml.name())) {
                html += u"</"_s + xml.name().toString() + u'>';
            }
            break;
        case QXmlStreamReader::Characters:
            html += xml.text().toString().simplified().toHtmlEscaped();
            break;
        default:
            break;
        }
    }
    return html;
}

Release readRelease(QXmlStreamReader& xml)
{
    Release release;
    release.version = xml.attributes().value(u"version"_s).toString();
    release.date = xml.attributes().value(u"date"_s).toString();
    while (xml.readNextStartElement()) {
        if (xml.name() == u"description"_s) {
            release.description = readDescription(xml);
        } else {
            xml.skipCurrentElement();
        }
    }
    return release;
}

} // namespace

QList<Release> parseReleases(const QByteArray& metainfoXml)
{
    QList<Release> releases;
    QXmlStreamReader xml(metainfoXml);
    bool inReleases = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == u"releases"_s) {
            inReleases = true;
        } else if (xml.isEndElement() && xml.name() == u"releases"_s) {
            inReleases = false;
        } else if (inReleases && xml.isStartElement() && xml.name() == u"release"_s) {
            releases.append(readRelease(xml));
        }
    }
    if (xml.hasError()) {
        return {};
    }
    return releases;
}

bool shouldShowWhatsNew(const QString& seenVersion, const QString& currentVersion)
{
    return !seenVersion.isEmpty() && seenVersion != currentVersion;
}

qsizetype releaseIndexFor(const QList<Release>& releases, const QString& version)
{
    for (qsizetype i = 0; i < releases.size(); ++i) {
        if (releases.at(i).version == version) {
            return i;
        }
    }
    return releases.isEmpty() ? -1 : 0;
}

} // namespace glate::shell
