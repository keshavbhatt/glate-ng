#include "BugReportDialog.h"

#include "ShellStyle.h"

#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QClipboard>
#include <QDesktopServices>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>

#include <utility>

using namespace Qt::StringLiterals;

namespace glate::shell {

namespace {
constexpr int kTitleMax = 100;
} // namespace

BugReportDialog::BugReportDialog(QString diagnostics, QStringList logLines, Targets targets, QWidget* parent)
    : QDialog(parent)
    , m_diagnostics(std::move(diagnostics))
    , m_logLines(std::move(logLines))
    , m_targets(std::move(targets))
    , m_title(new QLineEdit(this))
    , m_description(new QPlainTextEdit(this))
    , m_includeLog(new QCheckBox(this))
    , m_disclosure(new QToolButton(this))
    , m_detailsView(new QPlainTextEdit(this))
{
    setObjectName(u"bugReportDialog"_s);
    buildForm(setUpDialog(this, tr("Report a bug")));
    buildFooter(addFooter(this));
    connect(m_disclosure, &QToolButton::toggled, this, [this](bool open) {
        m_disclosure->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        m_detailsView->setVisible(open);
    });
    connect(m_includeLog, &QCheckBox::toggled, this, &BugReportDialog::refreshDetails);
    refreshDetails();
    resize(580, 560);
}

void BugReportDialog::buildForm(QBoxLayout* body)
{
    auto* intro = new QLabel(tr("Describe what went wrong. Glate does not send anything itself: it opens a "
                                "GitHub issue or an email for you to review and send."),
                             this);
    intro->setWordWrap(true);
    makeMuted(intro);
    body->addWidget(intro);

    body->addWidget(new QLabel(tr("Title"), this));
    m_title->setObjectName(u"bugTitle"_s);
    m_title->setMaxLength(kTitleMax);
    body->addWidget(m_title);
    body->addWidget(new QLabel(tr("What happened?"), this));
    m_description->setObjectName(u"bugDescription"_s);
    m_description->setTabChangesFocus(true);
    body->addWidget(m_description, 1);

    m_includeLog->setText(
        tr("Include the last %n lines of the log", nullptr, static_cast<int>(m_logLines.size())));
    m_includeLog->setObjectName(u"includeLog"_s);
    m_includeLog->setChecked(!m_logLines.isEmpty());
    m_includeLog->setVisible(!m_logLines.isEmpty());
    body->addWidget(m_includeLog);

    m_disclosure->setText(tr("Details that will be included"));
    m_disclosure->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_disclosure->setArrowType(Qt::RightArrow);
    m_disclosure->setCheckable(true);
    m_disclosure->setAutoRaise(true);
    body->addWidget(m_disclosure);
    m_detailsView->setObjectName(u"detailsView"_s);
    m_detailsView->setReadOnly(true);
    m_detailsView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_detailsView->setVisible(false);
    body->addWidget(m_detailsView, 1);
}

void BugReportDialog::buildFooter(QBoxLayout* footer)
{
    auto* copy = new QPushButton(QIcon(u":/icons/file-copy-line.png"_s), tr("Copy details"), this);
    connect(copy, &QPushButton::clicked, this, &BugReportDialog::copyDetails);
    footer->addWidget(copy);
    footer->addStretch(1);
    auto* mail = new QPushButton(QIcon(u":/icons/mail-line.png"_s), tr("Send by email"), this);
    mail->setObjectName(u"sendMail"_s);
    connect(mail, &QPushButton::clicked, this, &BugReportDialog::openMail);
    footer->addWidget(mail);
    auto* issue = new QPushButton(QIcon(u":/icons/external-link-line.png"_s), tr("Open GitHub issue"), this);
    issue->setObjectName(u"openIssue"_s);
    issue->setDefault(true);
    issue->setProperty("accent", true);
    connect(issue, &QPushButton::clicked, this, &BugReportDialog::openIssue);
    footer->addWidget(issue);
}

void BugReportDialog::setDescription(const QString& text)
{
    m_description->setPlainText(text);
}

BugReport BugReportDialog::report() const
{
    return {.title = m_title->text(), .description = m_description->toPlainText()};
}

QString BugReportDialog::details() const
{
    QString text = m_diagnostics;
    if (m_includeLog->isChecked() && !m_logLines.isEmpty()) {
        text += u"\n--- log ---\n"_s + redactHome(m_logLines.join(u'\n'));
    }
    return text;
}

void BugReportDialog::copyDetails() const
{
    QApplication::clipboard()->setText(u"```\n"_s + details() + u"\n```"_s);
}

void BugReportDialog::openIssue()
{
    copyDetails();
    QDesktopServices::openUrl(issueUrl(m_targets.newIssueUrl, report()));
}

void BugReportDialog::openMail()
{
    copyDetails();
    QDesktopServices::openUrl(mailUrl(m_targets.mailAddress, report()));
}

void BugReportDialog::refreshDetails()
{
    m_detailsView->setPlainText(details());
}

} // namespace glate::shell
