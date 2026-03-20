#include "licenseservice.h"

#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>

LicenseService *LicenseService::s_instance = nullptr;

LicenseService::LicenseService(QObject *parent)
    : QObject(parent) {}

LicenseService *LicenseService::instance() {
    if (!s_instance)
        s_instance = new LicenseService(qApp);
    return s_instance;
}

void LicenseService::setHasPaidAccess(bool hasPaidAccess) {
    m_hasPaidAccess = hasPaidAccess;
}

void LicenseService::setEvaluationState(EvaluationState state) {
    m_evaluationState = state;
}

bool LicenseService::hasPaidAccess() const       { return m_hasPaidAccess; }
LicenseService::EvaluationState
LicenseService::evaluationState() const          { return m_evaluationState; }

void LicenseService::setEvaluationDurationDays(int days)
{
    if (days > 0)
        m_evaluationDurationDays = days;
}

int LicenseService::evaluationDurationDays() const { return m_evaluationDurationDays; }

// ─────────────────────────────────────────────────────────────────────────────

bool LicenseService::checkAccess(const QString &featureName)
{
    if (m_hasPaidAccess)
        return true;

    const bool evalExpired = (m_evaluationState == EvaluationState::Expired);
    const QString daysStr  = QString::number(m_evaluationDurationDays);

    // ── Compose context-aware copy ────────────────────────────────────────────

    // Headline
    const QString headline = evalExpired
        ? QStringLiteral("<span style='font-size:11pt; font-weight:600;'>"
                         "Your free evaluation has ended</span>")
        : QStringLiteral("<span style='font-size:11pt; font-weight:600;'>"
                         "Pro feature</span>");

    // Feature callout + value proposition
    const QString body = evalExpired
        ? QStringLiteral(
            "<p>You enjoyed full access to <b>%1</b> and all other Pro features "
            "during your <b>%2&#8209;day</b> evaluation period.&nbsp; "
            "Unlock them permanently with a Pro licence.</p>"
            "<p style='margin-top:8px;'><b>Glate Pro includes:</b></p>"
            "<p style='margin:0; line-height:1.7;'>"
            "&nbsp;🎵&nbsp; Export translations as audio (MP3)<br>"
            "&nbsp;🔗&nbsp; Create public share links<br>"
            "&nbsp;⭐&nbsp; All future premium features<br>"
            "&nbsp;❤️&nbsp; Support independent development"
            "</p>"
            "<p style='margin-top:10px; color:gray; font-size:8pt;'>"
            "One-time purchase &nbsp;·&nbsp; No subscription &nbsp;·&nbsp; Lifetime updates"
            "</p>")
          .arg(featureName, daysStr)
        : QStringLiteral(
            "<p><b>%1</b> is part of the <b>Glate Pro</b> plan.</p>"
            "<p>Upgrade to Pro to unlock this and every other premium feature "
            "permanently.</p>"
            "<p style='margin-top:8px;'><b>What you get with Pro:</b></p>"
            "<p style='margin:0; line-height:1.7;'>"
            "&nbsp;🎵&nbsp; Export translations as audio (MP3)<br>"
            "&nbsp;🔗&nbsp; Create public share links<br>"
            "&nbsp;⭐&nbsp; All future premium features<br>"
            "&nbsp;❤️&nbsp; Support independent development"
            "</p>"
            "<p style='margin-top:10px; color:gray; font-size:8pt;'>"
            "One-time purchase &nbsp;·&nbsp; No subscription &nbsp;·&nbsp; Lifetime updates"
            "</p>")
          .arg(featureName);

    // CTA label
    const QString ctaLabel = evalExpired
        ? QStringLiteral("  Unlock Pro Now  ")
        : QStringLiteral("  See Plans & Pricing  ");

    // ── Build and show the dialog ─────────────────────────────────────────────

    QWidget *parent = QApplication::activeWindow();

    QMessageBox msgBox(parent);
    msgBox.setWindowTitle(QApplication::applicationName()
                          + QStringLiteral(" | Upgrade to Pro"));

    // Use NoIcon — the emoji headline and rich text carry all the visual weight.
    msgBox.setIcon(QMessageBox::NoIcon);
    msgBox.setText(headline);
    msgBox.setInformativeText(body);
    msgBox.setTextFormat(Qt::RichText);

    QPushButton *upgradeBtn =
        msgBox.addButton(ctaLabel, QMessageBox::AcceptRole);
    QPushButton *laterBtn =
        msgBox.addButton(QStringLiteral("Maybe Later"), QMessageBox::RejectRole);

    // Style the CTA button to stand out.
    upgradeBtn->setStyleSheet(
        QStringLiteral("QPushButton {"
                       "  background-color: #3daee9;"
                       "  color: #ffffff;"
                       "  border: none;"
                       "  border-radius: 4px;"
                       "  padding: 6px 16px;"
                       "  font-weight: 600;"
                       "}"
                       "QPushButton:hover  { background-color: #4db8f0; }"
                       "QPushButton:pressed{ background-color: #2d9ed8; }"));
    Q_UNUSED(laterBtn)

    msgBox.setDefaultButton(upgradeBtn);
    msgBox.exec();

    if (msgBox.clickedButton() == upgradeBtn)
        emit upgradeRequested();

    return false;
}
