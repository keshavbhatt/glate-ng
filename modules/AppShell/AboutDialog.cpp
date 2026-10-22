#include "AboutDialog.h"

#include "ShellStyle.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFontDatabase>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace glate::shell {

namespace {
constexpr int kHeroIcon = 72;
constexpr int kLinkIcon = 16;
} // namespace

QWidget* makeRowList(const QList<AboutInfo::Row>& rows, QWidget* parent)
{
    auto* scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::StyledPanel);
    auto* inner = new QWidget(scroll);
    auto* grid = new QGridLayout(inner);
    grid->setContentsMargins(12, 10, 12, 10);
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(10);
    int row = 0;
    for (const AboutInfo::Row& item : rows) {
        auto* name = new QLabel(u"<b>%1</b>"_s.arg(item.name.toHtmlEscaped()), inner);
        name->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        auto* text = new QLabel(item.text, inner);
        text->setWordWrap(true);
        text->setTextInteractionFlags(Qt::TextBrowserInteraction);
        text->setOpenExternalLinks(true);
        grid->addWidget(name, row, 0);
        grid->addWidget(text, row, 1);
        ++row;
    }
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(row, 1);
    scroll->setWidget(inner);
    return scroll;
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
    tabs->addTab(makeRowList(info.credits, tabs), tr("Credits"));
    tabs->addTab(makeRowList(info.openSource, tabs), tr("Open source"));
    tabs->addTab(buildDiagnostics(info), tr("Diagnostics"));
    body->addWidget(tabs, 1);

    QBoxLayout* footer = addFooter(this);
    footer->addStretch(1);
    addCloseButton(this, footer);
    resize(660, 600);
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
    row->addWidget(icon, 0, Qt::AlignTop);

    auto* text = new QVBoxLayout;
    text->setSpacing(2);
    auto* name = new QLabel(info.appName, header);
    QFont big = name->font();
    big.setPointSizeF(big.pointSizeF() * 1.45);
    big.setBold(true);
    name->setFont(big);
    auto* tagline = new QLabel(info.tagline, header);
    tagline->setWordWrap(true);
    makeMuted(tagline);

    auto* versionRow = new QHBoxLayout;
    versionRow->setSpacing(8);
    auto* version = new QLabel(tr("Version %1").arg(info.version), header);
    version->setObjectName(u"aboutVersion"_s);
    version->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* whatsNew = new QLabel(u"<a href=\"#\">%1</a>"_s.arg(tr("What's new")), header);
    whatsNew->setObjectName(u"aboutWhatsNew"_s);
    connect(whatsNew, &QLabel::linkActivated, this, &AboutDialog::whatsNewRequested);
    versionRow->addWidget(version);
    versionRow->addWidget(whatsNew);
    versionRow->addStretch(1);

    auto* author = new QLabel(info.author, header);
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
    auto* grid = new QGridLayout(links);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(6);
    constexpr int kColumns = 3;
    int index = 0;
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
        grid->addWidget(button, index / kColumns, index % kColumns);
        ++index;
    }
    return links;
}

QWidget* AboutDialog::buildDiagnostics(const AboutInfo& info)
{
    auto* page = new QWidget(this);
    auto* column = new QVBoxLayout(page);
    column->setContentsMargins(8, 8, 8, 8);
    m_diagnostics = new QPlainTextEdit(info.diagnostics, page);
    m_diagnostics->setObjectName(u"aboutDiagnostics"_s);
    m_diagnostics->setReadOnly(true);
    m_diagnostics->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    column->addWidget(m_diagnostics, 1);

    auto* buttons = new QHBoxLayout;
    auto* copy = new QPushButton(QIcon(u":/icons/file-copy-line.png"_s), tr("Copy"), page);
    connect(copy, &QPushButton::clicked, this,
            [this] { QApplication::clipboard()->setText(m_diagnostics->toPlainText()); });
    buttons->addWidget(copy);
    if (!info.logDirectory.isEmpty()) {
        auto* folder = new QPushButton(QIcon(u":/icons/folder-open-line.png"_s), tr("Open log folder"), page);
        const QUrl url = QUrl::fromLocalFile(info.logDirectory);
        connect(folder, &QPushButton::clicked, this, [url] { QDesktopServices::openUrl(url); });
        buttons->addWidget(folder);
    }
    buttons->addStretch(1);
    column->addLayout(buttons);
    return page;
}

} // namespace glate::shell
