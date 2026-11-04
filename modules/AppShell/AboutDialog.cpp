#include "AboutDialog.h"

#include "FlowLayout.h"
#include "ShellIcons.h"
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
#include <QScrollArea>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace glate::shell {

namespace {
constexpr int kHeroIcon = 80;
constexpr int kLinkIcon = 16;
constexpr int kRowNameWidth = 190;
constexpr int kWidth = 700;
constexpr int kDiagnosticsLines = 7;

// A page that scrolls vertically when its content is taller than the tab.
QWidget* scrollPage(QWidget* content, QWidget* parent)
{
    auto* scroll = new QScrollArea(parent);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(u"QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; }"_s);
    scroll->setWidget(content);
    return scroll;
}
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
        row->setContentsMargins(14, 11, 14, 11);
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
    , m_links(buildLinks(info))
{
    setObjectName(u"aboutDialog"_s);
    QBoxLayout* body = setUpDialog(this, tr("About %1").arg(info.appName));
    // Roomier than the other shell dialogs (DOCS/mocks/about.html).
    body->setContentsMargins(28, 24, 28, 22);
    body->setSpacing(22);
    body->addWidget(buildHeader(info));
    body->addWidget(m_links);
    m_body = body;

    auto* tabs = new QTabWidget(this);
    tabs->setObjectName(u"aboutTabs"_s);
    tabs->addTab(scrollPage(makeRowList(info.credits, tabs), tabs), tr("Credits"));
    tabs->addTab(scrollPage(makeRowList(info.openSource, tabs), tabs), tr("Open source"));
    tabs->addTab(buildDiagnostics(info), tr("Diagnostics"));
    body->addWidget(tabs);
    // A taller window leaves the room below the tabs; nothing above stretches.
    body->addStretch(1);

    // A link without a URL (Report a bug) sits in the footer, apart from the
    // links that leave the app.
    QBoxLayout* footer = addFooter(this);
    for (const AboutInfo::Link& link : info.links) {
        if (link.url.isEmpty()) {
            auto* button = new QPushButton(link.text, this);
            button->setObjectName(u"aboutBugReport"_s);
            if (!link.icon.isEmpty()) {
                icons::bind(button, link.icon);
            }
            connect(button, &QPushButton::clicked, this, &AboutDialog::bugReportRequested);
            footer->addWidget(button);
        }
    }
    footer->addStretch(1);
    addCloseButton(this, footer);

    // One height for every tab, so switching tabs never moves anything: the
    // tallest page, capped; a longer list scrolls inside its page instead of
    // being cut off.
    ensurePolished();
    constexpr int kPageWidth = kWidth - 56;
    constexpr int kMaxPageHeight = 300;
    int pageHeight = 0;
    for (int i = 0; i < tabs->count(); ++i) {
        const QWidget* page = tabs->widget(i);
        const auto* scroll = qobject_cast<const QScrollArea*>(page);
        const QWidget* content = scroll != nullptr ? scroll->widget() : page;
        const int height =
            content->hasHeightForWidth() ? content->heightForWidth(kPageWidth) : content->sizeHint().height();
        pageHeight = std::max(pageHeight, height);
    }
    pageHeight = std::min(pageHeight, kMaxPageHeight);
    tabs->setFixedHeight(tabs->tabBar()->sizeHint().height() + pageHeight + 4);
    fitToContent(this, kWidth);
}

void AboutDialog::addBelowLinks(QWidget* widget)
{
    widget->setParent(this);
    // Never taller than it needs, however tall the window is made.
    widget->setSizePolicy(widget->sizePolicy().horizontalPolicy(), QSizePolicy::Maximum);
    m_body->insertWidget(m_body->indexOf(m_links) + 1, widget);
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
    row->setSpacing(20);
    auto* icon = new QLabel(header);
    icon->setPixmap(info.icon.pixmap(kHeroIcon, kHeroIcon));
    row->addWidget(icon, 0, Qt::AlignVCenter);

    auto* text = new QVBoxLayout;
    text->setSpacing(4);
    auto* name = new QLabel(info.appName, header);
    setRelativeFont(name, 1.6, true);
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
    auto* flow = new FlowLayout(links, 8);
    for (const AboutInfo::Link& link : info.links) {
        if (link.url.isEmpty()) {
            continue; // in the footer
        }
        auto* button = new QPushButton(link.text, links);
        if (!link.icon.isEmpty()) {
            icons::bind(button, link.icon);
        }
        button->setIconSize(QSize(kLinkIcon, kLinkIcon));
        button->setToolTip(link.url.toString());
        const QUrl url = link.url;
        connect(button, &QPushButton::clicked, this, [url] { QDesktopServices::openUrl(url); });
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
    auto* copy = new QPushButton(tr("Copy"), page);
    icons::bind(copy, u"copy"_s);
    connect(copy, &QPushButton::clicked, this,
            [this] { QApplication::clipboard()->setText(m_diagnostics->toPlainText()); });
    buttons->addWidget(copy);
    if (!info.logDirectory.isEmpty()) {
        auto* folder = new QPushButton(tr("Open log folder"), page);
        icons::bind(folder, u"folder"_s);
        const QUrl url = QUrl::fromLocalFile(info.logDirectory);
        connect(folder, &QPushButton::clicked, this, [url] { QDesktopServices::openUrl(url); });
        buttons->addWidget(folder);
    }
    buttons->addStretch(1);
    column->addLayout(buttons);
    return page;
}

} // namespace glate::shell
