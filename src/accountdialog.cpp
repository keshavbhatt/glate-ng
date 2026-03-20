#include "accountdialog.h"
#include "ui_accountdialog.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QMessageBox>
#include <QShortcut>
#include <QTimer>
#include <QUrl>

AccountDialog::AccountDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AccountDialog)
{
    ui->setupUi(this);
    { auto *_esc = new QShortcut(Qt::Key_Escape, this); connect(_esc, &QShortcut::activated, this, [this]{ close(); }); }
    setWindowTitle(QApplication::applicationName() + QStringLiteral(" | Account"));
    setWindowModality(Qt::WindowModal);

    connect(ui->copyAccId, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(ui->accId->text());
    });
    connect(ui->restoreBtn,  &QPushButton::clicked, this, &AccountDialog::onRefreshClicked);
    connect(ui->purchaseBtn, &QPushButton::clicked, this, &AccountDialog::onPurchaseClicked);
    connect(ui->portalBtn,   &QPushButton::clicked, this, &AccountDialog::onPortalClicked);
    connect(ui->closeBtn,    &QPushButton::clicked, this, &QDialog::close);

#ifdef QT_DEBUG
    // Style the debug frame so it stands out visually.
    ui->debugFrame->setStyleSheet(
        QStringLiteral("QFrame#debugFrame { border: 1px dashed #888; border-radius: 4px; }"));

    connect(ui->debugApplyBtn, &QPushButton::clicked, this, [this]() {
        if (!m_accountManager)
            return;
        using S = AccountLicenseManager::DebugLicenseState;
        static const S kStates[] = {
            S::None,
            S::Pro,
            S::EvaluationActive,
            S::EvaluationExpired,
            S::NetworkError,
        };
        const int idx = ui->debugStateCombo->currentIndex();
        const S state = (idx >= 0 && idx < 5) ? kStates[idx] : S::None;
        m_accountManager->setDebugLicenseState(state);
        // force = true so the scheduler is bypassed and the simulated status
        // fires immediately through the full signal chain.
        m_accountManager->checkPurchase(m_accountManager->serial(), /*force=*/true);
    });
#else
    ui->debugFrame->hide();
#endif
}

AccountDialog::~AccountDialog()
{
    delete ui;
}

// ── Public interface ──────────────────────────────────────────────────────────

void AccountDialog::setAccountManager(AccountLicenseManager *accountManager)
{
    m_accountManager = accountManager;
    if (!m_accountManager)
        return;

    connect(m_accountManager, &AccountLicenseManager::licenseCheckInProgressChanged,
            this, &AccountDialog::onLicenseCheckProgressChanged);
    connect(m_accountManager, &AccountLicenseManager::licenseStatusUpdated,
            this, &AccountDialog::onLicenseStatusUpdated);
}

void AccountDialog::setSerial(const QString &serial) const
{
    ui->accId->setText(serial);
}

void AccountDialog::setAccountType(bool isPro) const
{
    ui->accType->setText(isPro ? QStringLiteral("Pro") : QStringLiteral("Evaluation"));
    ui->restoreBtn->setEnabled(true);
    ui->accountHintLabel->setVisible(!isPro);
    ui->planSummaryLabel->setText(isPro
        ? QStringLiteral("Plan status: Pro license active")
        : QStringLiteral("Plan status: Evaluation / Free"));
    refreshPurchaseButtonState();
}

void AccountDialog::setCheckingState(bool checking) const
{
    if (checking) {
        ui->accountStatusLabel->setText(QStringLiteral("Status: Checking account status..."));
        ui->restoreBtn->setEnabled(false);
        ui->purchaseBtn->setEnabled(false);
        ui->portalBtn->setEnabled(false);
        return;
    }
    ui->restoreBtn->setEnabled(true);
    ui->portalBtn->setEnabled(true);
    refreshPurchaseButtonState();
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void AccountDialog::onLicenseStatusUpdated(
    const AccountLicenseManager::LicenseStatus &status)
{
    m_isEvaluationActive     = status.evaluationActive && !status.active;
    m_evaluationDaysRemaining = status.evaluationDaysRemaining;
    m_isPaidPlan              = status.active;

    const QString licenseType = status.licenseType.trimmed().toLower();
    m_canUpgradePaidPlan = m_isPaidPlan
        && (licenseType != QStringLiteral("lifetime"));

    const bool paidPlan  = status.active;
    const bool evaluation = status.evaluationActive && !paidPlan;

    ui->accType->setText(paidPlan    ? QStringLiteral("Pro")
                         : evaluation ? QStringLiteral("Evaluation")
                                      : QStringLiteral("Free"));
    ui->restoreBtn->setEnabled(true);
    ui->accountHintLabel->setVisible(!paidPlan);

    refreshPurchaseButtonState();
    refreshPlanVisuals(paidPlan, evaluation);

    QStringList details;
    if (!status.message.trimmed().isEmpty() && !evaluation)
        details << status.message;
    if (evaluation)
        details << QStringLiteral("Evaluation is active.");
    if (status.hasExpiryDate && !status.expiryDate.trimmed().isEmpty())
        details << QStringLiteral("Expires: ") + status.expiryDate;
    if (status.hasDaysRemaining)
        details << QStringLiteral("Days left: ") + QString::number(status.daysRemaining);

    ui->accountStatusLabel->setText(
        details.isEmpty()
            ? QStringLiteral("Status: No active paid license")
            : QStringLiteral("Status: ") + details.join(QStringLiteral(" — ")));

    refreshHintLabel();

    if (status.active)
        emit licenseActivated(true);

    // Defer resize: word-wrapped QLabel heights are only finalised after the
    // current layout pass completes, so adjustSize() must run next tick.
    QTimer::singleShot(0, this, [this]() { adjustSize(); });
}

void AccountDialog::onLicenseCheckProgressChanged(bool inProgress) const
{
    setCheckingState(inProgress);
}

void AccountDialog::highlightPurchaseButton()
{
    // Cancel any in-progress flash before starting a new one.
    if (m_flashTimer) {
        m_flashTimer->stop();
        m_flashTimer->deleteLater();
        m_flashTimer = nullptr;
    }
    ui->purchaseBtn->setStyleSheet(QString());

    // Only flash when the button is actually actionable.
    if (!ui->purchaseBtn->isEnabled())
        return;

    static constexpr int kIntervalMs  = 400; // ms per half-cycle
    static constexpr int kTotalCycles = 5;  // 5 × 400 ms = 2 s

    const QString kOnStyle  = QStringLiteral(
        "QPushButton {"
        "  border: 1px solid #e05050;"
        "  border-radius: 4px;"
        "  color: #e05050;"
        "}");

    m_flashCount = 0;
    m_flashTimer = new QTimer(this);
    m_flashTimer->setInterval(kIntervalMs);

    connect(m_flashTimer, &QTimer::timeout, this, [this, kOnStyle]() {
        // odd count → off, even → on
        ui->purchaseBtn->setStyleSheet(
            (m_flashCount % 2 == 0) ? kOnStyle : QString());

        if (++m_flashCount >= kTotalCycles * 2) {
            m_flashTimer->stop();
            m_flashTimer->deleteLater();
            m_flashTimer = nullptr;
            ui->purchaseBtn->setStyleSheet(QString());
        }
    });

    // Start with the button lit so the flash is immediately visible.
    ui->purchaseBtn->setStyleSheet(kOnStyle);
    m_flashTimer->start();
}

// ── Private helpers ───────────────────────────────────────────────────────────

void AccountDialog::refreshPurchaseButtonState() const
{
    if (m_isPaidPlan && m_canUpgradePaidPlan) {
        ui->purchaseBtn->setText(QStringLiteral("Upgrade to New Plan"));
        ui->purchaseBtn->setEnabled(true);
        return;
    }
    ui->purchaseBtn->setText(QStringLiteral("Purchase Pro Licence"));
    ui->purchaseBtn->setEnabled(!m_isPaidPlan);
}

void AccountDialog::refreshHintLabel() const
{
    if (m_isEvaluationActive) {
        ui->accountHintLabel->setText(
            QStringLiteral("Evaluation includes all Pro features. "
                           "Purchase a Pro licence to continue after the trial ends."));
        return;
    }
    ui->accountHintLabel->setText(
        QStringLiteral("Upgrade to Pro for access to all premium features "
                       "and priority support."));
}

void AccountDialog::refreshPlanVisuals(bool paidPlan, bool evaluation) const
{
    ui->labelEvaluationRemaining->setVisible(!paidPlan);
    ui->evaluationRemainingLabel->setVisible(!paidPlan);

    if (paidPlan) {
        ui->planSummaryLabel->setText(QStringLiteral("Plan status: Pro license active"));
        ui->accType->setStyleSheet(QStringLiteral("color: #4fc37a; font-weight: 600;"));
        ui->evaluationRemainingLabel->setText(QStringLiteral("Not applicable"));
        return;
    }

    if (evaluation) {
        ui->planSummaryLabel->setText(QStringLiteral("Plan status: Evaluation active"));
        ui->accType->setStyleSheet(QStringLiteral("color: #ffcc66; font-weight: 600;"));
        ui->evaluationRemainingLabel->setText(
            m_evaluationDaysRemaining >= 0
                ? QString::number(m_evaluationDaysRemaining) + QStringLiteral(" day") +
                      (m_evaluationDaysRemaining == 1 ? QString() : QStringLiteral("s"))
                : QStringLiteral("Calculating..."));
        return;
    }

    ui->planSummaryLabel->setText(QStringLiteral("Plan status: Free tier"));
    ui->accType->setStyleSheet(QStringLiteral("color: #d4d4d4; font-weight: 600;"));
    ui->evaluationRemainingLabel->setText(QStringLiteral("Ended"));
}

void AccountDialog::onRefreshClicked() const
{
    if (!m_accountManager) return;
    setCheckingState(true);
    m_accountManager->checkPurchase(m_accountManager->serial(), /*force=*/true);
}

void AccountDialog::onPurchaseClicked() const
{
    if (!m_accountManager) return;

    // Stop the attention flash — the user has seen it.
    if (m_flashTimer) {
        m_flashTimer->stop();
        m_flashTimer->deleteLater();
        // m_flashTimer is mutable-style; cast away const for the cleanup.
        const_cast<AccountDialog *>(this)->m_flashTimer = nullptr;
        ui->purchaseBtn->setStyleSheet(QString());
    }

    if (m_isPaidPlan && m_canUpgradePaidPlan) {
        const int answer = QMessageBox::question(
            nullptr,
            QStringLiteral("Upgrade License"),
            QStringLiteral("Your current Pro license is active.\n\n"
                           "Do you want to open the purchase page to upgrade?"),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel);
        if (answer != QMessageBox::Yes)
            return;
    }

    const QString url =
        m_accountManager->config().checkoutUrlTemplate
            .arg(m_accountManager->serial());
    QDesktopServices::openUrl(QUrl(url));
}

void AccountDialog::onPortalClicked() const
{
    if (!m_accountManager) return;
    QDesktopServices::openUrl(
        QUrl(m_accountManager->config().selfServicePortalUrl));
}

void AccountDialog::closeEvent(QCloseEvent *event)
{
    QDialog::closeEvent(event);
}

