#include "licenseservice.h"

#include <QApplication>
#include <QDate>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
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

void LicenseService::setAccountManager(AccountLicenseManager *mgr) {
    m_accountManager = mgr;
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
            "&nbsp;🔊&nbsp; Unlimited TTS playback<br>"
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
            "&nbsp;🔊&nbsp; Unlimited TTS playback<br>"
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

// ── TTS daily-limit gate ──────────────────────────────────────────────────────

// Keys are prefixed with "license/" so AccountLicenseStore treats them as
// sensitive and stores them XOR-encrypted with obfuscated key names.
static const QString kTtsDateKey  = QStringLiteral("license/tts_free/date");
static const QString kTtsPlaysKey = QStringLiteral("license/tts_free/plays");

int LicenseService::ttsPlaysToday() const
{
    const QString today = QDate::currentDate().toString(Qt::ISODate);

    if (m_accountManager) {
        if (m_accountManager->storeValue(kTtsDateKey).toString() != today)
            return 0;
        return m_accountManager->storeValue(kTtsPlaysKey, 0).toInt();
    }

    // Fallback (should not happen in normal operation)
    QSettings s;
    if (s.value(kTtsDateKey).toString() != today)
        return 0;
    return s.value(kTtsPlaysKey, 0).toInt();
}

void LicenseService::incrementTtsPlays()
{
    const QString today = QDate::currentDate().toString(Qt::ISODate);

    if (m_accountManager) {
        if (m_accountManager->storeValue(kTtsDateKey).toString() != today) {
            m_accountManager->setStoreValue(kTtsDateKey,  today);
            m_accountManager->setStoreValue(kTtsPlaysKey, 1);
        } else {
            m_accountManager->setStoreValue(
                kTtsPlaysKey,
                m_accountManager->storeValue(kTtsPlaysKey, 0).toInt() + 1);
        }
        return;
    }

    // Fallback
    QSettings s;
    if (s.value(kTtsDateKey).toString() != today) {
        s.setValue(kTtsDateKey,  today);
        s.setValue(kTtsPlaysKey, 1);
    } else {
        s.setValue(kTtsPlaysKey, s.value(kTtsPlaysKey, 0).toInt() + 1);
    }
}

bool LicenseService::checkTtsAccess()
{
    // Paid users (Pro / active Evaluation) have unlimited TTS.
    if (m_hasPaidAccess)
        return true;

    const int playsToday = ttsPlaysToday();

    if (playsToday < kFreeTierDailyTtsLimit) {
        incrementTtsPlays();
        return true;
    }

    // Limit reached — show upgrade prompt.
    const QString headline =
        QStringLiteral("<span style='font-size:11pt; font-weight:600;'>"
                       "Daily TTS limit reached</span>");

    const QString body =
        QStringLiteral(
            "<p>Free tier allows <b>%1 TTS plays per day</b>. "
            "You have used all %1 today.</p>"
            "<p>Upgrade to <b>Glate Pro</b> for unlimited text-to-speech "
            "and all other premium features.</p>"
            "<p style='margin:0; line-height:1.7;'>"
            "&nbsp;🔊&nbsp; Unlimited TTS playback<br>"
            "&nbsp;🎵&nbsp; Export translations as audio (MP3)<br>"
            "&nbsp;🔗&nbsp; Create public share links<br>"
            "&nbsp;❤️&nbsp; Support independent development"
            "</p>"
            "<p style='margin-top:10px; color:gray; font-size:8pt;'>"
            "One-time purchase &nbsp;·&nbsp; No subscription &nbsp;·&nbsp; Lifetime updates"
            "</p>")
        .arg(kFreeTierDailyTtsLimit);

    QWidget *parent = QApplication::activeWindow();
    QMessageBox msgBox(parent);
    msgBox.setWindowTitle(QApplication::applicationName()
                          + QStringLiteral(" | Daily TTS Limit Reached"));
    msgBox.setIcon(QMessageBox::NoIcon);
    msgBox.setText(headline);
    msgBox.setInformativeText(body);
    msgBox.setTextFormat(Qt::RichText);

    QPushButton *upgradeBtn =
        msgBox.addButton(QStringLiteral("  Upgrade to Pro  "),
                         QMessageBox::AcceptRole);
    msgBox.addButton(QStringLiteral("Maybe Later"), QMessageBox::RejectRole);

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

    msgBox.setDefaultButton(upgradeBtn);
    msgBox.exec();

    if (msgBox.clickedButton() == upgradeBtn)
        emit upgradeRequested();

    return false;
}

