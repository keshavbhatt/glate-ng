#include "WhatsNewDialog.h"

#include "ShellStyle.h"

#include <QComboBox>
#include <QDate>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QTextBrowser>

#include <utility>

using namespace Qt::StringLiterals;

namespace glate::shell {

WhatsNewDialog::WhatsNewDialog(const QString& appName, QList<Release> releases, const QString& runningVersion,
                               QWidget* parent)
    : QDialog(parent)
    , m_releases(std::move(releases))
    , m_picker(new QComboBox(this))
    , m_date(new QLabel(this))
    , m_notes(new QTextBrowser(this))
{
    setObjectName(u"whatsNewDialog"_s);
    QBoxLayout* body = setUpDialog(this, tr("What's new in %1").arg(appName));

    auto* row = new QHBoxLayout;
    row->setSpacing(10);
    auto* label = new QLabel(tr("Version"), this);
    makeMuted(label);
    m_picker->setObjectName(u"versionPicker"_s);
    m_picker->setMinimumWidth(180);
    for (qsizetype i = 0; i < m_releases.size(); ++i) {
        const QString version = m_releases.at(i).version;
        m_picker->addItem(i == 0 ? tr("%1 (latest)").arg(version) : version);
    }
    makeMuted(m_date);
    row->addWidget(label);
    row->addWidget(m_picker);
    row->addWidget(m_date);
    row->addStretch(1);
    body->addLayout(row);

    m_notes->setObjectName(u"whatsNewNotes"_s);
    m_notes->setOpenExternalLinks(true);
    m_notes->document()->setDocumentMargin(12);
    body->addWidget(m_notes, 1);

    connect(m_picker, &QComboBox::currentIndexChanged, this, &WhatsNewDialog::showIndex);

    QBoxLayout* footer = addFooter(this);
    footer->addStretch(1);
    addCloseButton(this, footer);
    resize(560, 440);
    showVersion(runningVersion);
}

void WhatsNewDialog::showVersion(const QString& version)
{
    const auto index = static_cast<int>(releaseIndexFor(m_releases, version));
    if (index < 0) {
        m_notes->setPlainText(tr("No release notes are available."));
        return;
    }
    if (m_picker->currentIndex() == index) {
        showIndex(index);
    } else {
        m_picker->setCurrentIndex(index);
    }
}

QString WhatsNewDialog::shownVersion() const
{
    const int index = m_picker->currentIndex();
    return index >= 0 && index < m_releases.size() ? m_releases.at(index).version : QString();
}

void WhatsNewDialog::showIndex(int index)
{
    if (index < 0 || index >= m_releases.size()) {
        return;
    }
    const Release& release = m_releases.at(index);
    const QDate date = QDate::fromString(release.date, Qt::ISODate);
    m_date->setText(date.isValid() ? QLocale().toString(date, u"MMMM yyyy"_s) : QString());
    m_notes->setHtml(release.description);
}

} // namespace glate::shell
