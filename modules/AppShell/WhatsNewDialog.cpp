#include "WhatsNewDialog.h"

#include "ShellStyle.h"

#include <QComboBox>
#include <QDate>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QTextBrowser>
#include <QTextDocument>

#include <algorithm>
#include <memory>
#include <utility>

using namespace Qt::StringLiterals;

namespace glate::shell {

namespace {
constexpr int kWidth = 560;
constexpr int kNotesMargin = 14;
constexpr int kListIndent = 18;
constexpr int kMaxNotesHeight = 380;
} // namespace

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
    setRelativeFont(label, 0.9);
    makeMuted(label);
    m_picker->setObjectName(u"versionPicker"_s);
    m_picker->setMinimumWidth(180);
    for (qsizetype i = 0; i < m_releases.size(); ++i) {
        const QString version = m_releases.at(i).version;
        m_picker->addItem(i == 0 ? tr("%1 (latest)").arg(version) : version);
    }
    setRelativeFont(m_date, 0.9);
    makeMuted(m_date);
    row->addWidget(label);
    row->addWidget(m_picker);
    row->addWidget(m_date);
    row->addStretch(1);
    body->addLayout(row);

    m_notes->setObjectName(u"whatsNewNotes"_s);
    m_notes->setOpenExternalLinks(true);
    m_notes->setFrameShape(QFrame::NoFrame);
    m_notes->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_notes->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_notes->setStyleSheet(
        u"#whatsNewNotes { border: 1px solid %1; border-radius: 8px; background: transparent; padding: 0; }"_s
            .arg(borderColor().name()));
    m_notes->document()->setDocumentMargin(kNotesMargin);
    m_notes->document()->setIndentWidth(kListIndent);
    m_notes->document()->setDefaultStyleSheet(u"p { margin: 0; } ul { margin-top: 8px; margin-bottom: 0; } "
                                              "li { margin-bottom: 4px; }"_s);
    body->addWidget(m_notes, 1);

    connect(m_picker, &QComboBox::currentIndexChanged, this, &WhatsNewDialog::showIndex);

    QBoxLayout* footer = addFooter(this);
    footer->addStretch(1);
    addCloseButton(this, footer);
    showVersion(runningVersion);
    fitNotes();
    fitToContent(this, kWidth);
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
    fitNotes();
}

void WhatsNewDialog::fitNotes()
{
    // As tall as the notes, up to a limit; longer notes scroll.
    // Measured on a copy: the browser resets its own document to the viewport width.
    constexpr int kInnerWidth = kWidth - 40 - 2;
    const std::unique_ptr<QTextDocument> copy(m_notes->document()->clone());
    copy->setTextWidth(kInnerWidth);
    const int height = static_cast<int>(copy->size().height()) + 6;
    m_notes->setFixedHeight(std::min(height, kMaxNotesHeight));
}

} // namespace glate::shell
