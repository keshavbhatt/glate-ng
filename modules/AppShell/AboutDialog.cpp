#include "AboutDialog.h"

#include "FlowLayout.h"
#include "ShellStyle.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace glate::shell {

namespace {
constexpr int kHeroIcon = 72;
constexpr int kLinkIcon = 16;
constexpr int kRowNameWidth = 190;
constexpr int kWidth = 640;
constexpr int kDiagnosticsLines = 7;
} // namespace

QWidget* makeRowList(const QList<AboutInfo::Row>& rows, QWidget* parent)
{
    // A bordered list with a line between rows, as in the mock.
    // The tab pages share the tallest page's height: the list sits at the top of its page.
    auto* page = new QWidget(parent);
    auto* pageColumn = new QVBoxLayout(page);
    pageColumn->setContentsMargins(0, 12, 0, 0);
    auto* frame = new QFrame(page);
    frame->setObjectName(u"aboutRows"_s);
    frame->setAttribute(Qt::WA_StyledBackground, true);
    const QColor line = borderColor();
    frame->setStyleSheet(u"#aboutRows { border: 1px solid %1; border-radius: 8px; background: transparent; }"
                         "#aboutRows QFrame#rowLine { background: %1; border: none; }"_s.arg(line.name()));
    auto* column = new QVBoxLayout(frame);
    column->setContentsMargins(1, 1, 1, 1);
    column->setSpacing(0);
    bool first = true;
    for (const AboutInfo::Row& item : rows) {
        if (!first) {
            auto* rule = new QFrame(frame);
            rule->setObjectName(u"rowLine"_s);
            rule->setFixedHeight(1);
            rule->setAttribute(Qt::WA_StyledBackground, true);
            column->addWidget(rule);
        }
        first = false;
        auto* row = new QHBoxLayout;
        row->setContentsMargins(12, 8, 12, 8);
        row->setSpacing(12);
        auto* name = new QLabel(u"<b>%1</b>"_s.arg(item.name.toHtmlEscaped()), frame);
        name->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        name->setFixedWidth(kRowNameWidth);
        auto* text = new QLabel(item.text, frame);
        text->setWordWrap(true);
        text->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        text->setTextInteractionFlags(Qt::TextBrowserInteraction);
        text->setOpenExternalLinks(true);
        row->addWidget(name);
        row->addWidget(text, 1);
        column->addLayout(row);
    }
    pageColumn->addWidget(frame);
    pageColumn->addStretch(1);
    return page;
}

AboutDialog::AboutDialog(const AboutInfo& info, QWidget* parent)
    : QDialog(parent)
{
    setObjectName(u"aboutDialog"_s);
    QBoxLayout* body = setUpDialog(this, tr("About %1").arg(info.appName));
    body->setSpacing(14);
    body->addWidget(buildHeader(info));
    body->addWidget(buildLinks(info));

    auto* tabs = new QTabWidget(this);
    tabs->setObjectName(u"aboutTabs"_s);
    tabs->setDocumentMode(true);
    tabs->addTab(makeRowList(info.credits, tabs), tr("Credits"));
    tabs->addTab(makeRowList(info.openSource, tabs), tr("Open source"));
    tabs->addTab(buildDiagnostics(info), tr("Diagnostics"));
    body->addWidget(tabs, 1);

    QBoxLayout* footer = addFooter(this);
    footer->addStretch(1);
    addCloseButton(this, footer);
    fitToContent(this, kWidth);
}

void AboutDialog::setDiagnostics(const QString& text)
{
    m_diagnostics->setPlainText(text);
}

QWidget* AboutDialog::buildHeader(const AboutInfo& info)
{
    auto* header = new QWidget(this);
    auto* row = new QHBoxLayout(header);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(16);
    auto* icon = new QLabel(header);
    icon->setPixmap(info.icon.pixmap(kHeroIcon, kHeroIcon));
    row->addWidget(icon, 0, Qt::AlignVCenter);

    auto* text = new QVBoxLayout;
    text->setSpacing(2);
    auto* name = new QLabel(info.appName, header);
    setRelativeFont(name, 1.45, true);
    auto* tagline = new QLabel(info.tagline, header);
    tagline->setWordWrap(true);
    setRelativeFont(tagline, 1.07);
    makeMuted(tagline);

    auto* versionRow = new QHBoxLayout;
    versionRow->setSpacing(8);
    auto* version = new QLabel(tr("Version %1").arg(info.version), header);
    version->setObjectName(u"aboutVersion"_s);
    version->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* whatsNew =
        new QLabel(u"<a href=\"#\" style=\"text-decoration:none\">%1</a>"_s.arg(tr("What's new")), header);
    whatsNew->setObjectName(u"aboutWhatsNew"_s);
    connect(whatsNew, &QLabel::linkActivated, this, &AboutDialog::whatsNewRequested);
    setRelativeFont(version, 0.9);
    setRelativeFont(whatsNew, 0.9);
    versionRow->addWidget(version);
    versionRow->addWidget(whatsNew);
    versionRow->addStretch(1);

    auto* author = new QLabel(info.author, header);
    setRelativeFont(author, 0.9);
    makeMuted(author);
    text->addWidget(name);
    text->addWidget(tagline);
    text->addLayout(versionRow);
    text->addWidget(author);
    row->addLayout(text, 1);
    return header;
}

QWidget* AboutDialog::buildLinks(const AboutInfo& info)
{
    auto* links = new QWidget(this);
    auto* flow = new FlowLayout(links, 6);
    for (const AboutInfo::Link& link : info.links) {
        auto* button = new QPushButton(QIcon(link.iconPath), link.text, links);
        button->setIconSize(QSize(kLinkIcon, kLinkIcon));
        if (link.url.isEmpty()) {
            button->setObjectName(u"aboutBugReport"_s);
            connect(button, &QPushButton::clicked, this, &AboutDialog::bugReportRequested);
        } else {
            button->setToolTip(link.url.toString());
            const QUrl url = link.url;
            connect(button, &QPushButton::clicked, this, [url] { QDesktopServices::openUrl(url); });
        }
        flow->addWidget(button);
    }
    return links;
}

QWidget* AboutDialog::buildDiagnostics(const AboutInfo& info)
{
    auto* page = new QWidget(this);
    auto* column = new QVBoxLayout(page);
    column->setContentsMargins(0, 12, 0, 0);
    m_diagnostics = new QPlainTextEdit(info.diagnostics, page);
    m_diagnostics->setObjectName(u"aboutDiagnostics"_s);
    m_diagnostics->setReadOnly(true);
    m_diagnostics->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_diagnostics->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_diagnostics->setFixedHeight((m_diagnostics->fontMetrics().lineSpacing() * kDiagnosticsLines) + 24);
    column->addWidget(m_diagnostics);

    auto* buttons = new QHBoxLayout;
    auto* copy = new QPushButton(QIcon(u":/icons/file-copy-line.png"_s), tr("Copy"), page);
    connect(copy, &QPushButton::clicked, this,
            [this] { QApplication::clipboard()->setText(m_diagnostics->toPlainText()); });
    buttons->addWidget(copy);
    if (!info.logDirectory.isEmpty()) {
        auto* folder = new QPushButton(QIcon(u":/icons/folder-line.png"_s), tr("Open log folder"), page);
        const QUrl url = QUrl::fromLocalFile(info.logDirectory);
        connect(folder, &QPushButton::clicked, this, [url] { QDesktopServices::openUrl(url); });
        buttons->addWidget(folder);
    }
    buttons->addStretch(1);
    column->addLayout(buttons);
    return page;
}

} // namespace glate::shell
