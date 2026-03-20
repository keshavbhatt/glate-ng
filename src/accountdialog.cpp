#include "accountdialog.h"
#include "planscomparisiondialog.h"
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
    : QDialog(parent), ui(new Ui::AccountDialog) {
  ui->setupUi(this);
  {
    auto *_esc = new QShortcut(Qt::Key_Escape, this);
    connect(_esc, &QShortcut::activated, this, [this] { close(); });
  }
  setWindowTitle(QApplication::applicationName() +
                 QStringLiteral(" | Account"));
  setWindowModality(Qt::WindowModal);

  connect(ui->copyAccId, &QPushButton::clicked, this,
          [this]() { QApplication::clipboard()->setText(ui->accId->text()); });
  connect(ui->restoreBtn, &QPushButton::clicked, this,
          &AccountDialog::onRefreshClicked);
  connect(ui->purchaseBtn, &QPushButton::clicked, this,
          &AccountDialog::onPurchaseClicked);
  connect(ui->portalBtn, &QPushButton::clicked, this,
          &AccountDialog::onPortalClicked);
  connect(ui->closeBtn, &QPushButton::clicked, this, &QDialog::close);

#ifdef QT_DEBUG
  // Style the debug frame so it stands out visually.
  ui->debugFrame->setStyleSheet(QStringLiteral(
      "QFrame#debugFrame { border: 1px dashed #888; border-radius: 4px; }"));

  connect(ui->debugApplyBtn, &QPushButton::clicked, this, [this]() {
    if (!m_accountManager)
      return;
    using S = AccountLicenseManager::DebugLicenseState;
    static const S kStates[] = {
        S::None,         S::Pro, S::EvaluationActive, S::EvaluationExpired,
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

AccountDialog::~AccountDialog() { delete ui; }

// ── Public interface
// ──────────────────────────────────────────────────────────

void AccountDialog::setAccountManager(AccountLicenseManager *accountManager) {
  m_accountManager = accountManager;
  if (!m_accountManager)
    return;

  connect(m_accountManager,
          &AccountLicenseManager::licenseCheckInProgressChanged, this,
          &AccountDialog::onLicenseCheckProgressChanged);
  connect(m_accountManager, &AccountLicenseManager::licenseStatusUpdated, this,
          &AccountDialog::onLicenseStatusUpdated);
}

void AccountDialog::setSerial(const QString &serial) const {
  ui->accId->setText(serial);
}

void AccountDialog::setAccountType(bool isPro) const {
  ui->accType->setText(isPro ? QStringLiteral("Pro")
                             : QStringLiteral("Evaluation"));
  ui->restoreBtn->setEnabled(true);
  ui->accountHintLabel->setVisible(!isPro);
  ui->planSummaryLabel->setText(
      isPro ? QStringLiteral("Plan status: Pro license active")
            : QStringLiteral("Plan status: Evaluation / Free"));
  refreshPurchaseButtonState();
}

void AccountDialog::setCheckingState(bool checking) const {
  if (checking) {
    ui->planSummaryLabel->setText(QStringLiteral("Verifying licence…"));

    const bool dark =
        QApplication::palette().window().color().lightness() < 128;
    const QString checkingColor =
        dark ? QStringLiteral("#9aa2ab") : QStringLiteral("#555e6a");
    const QString checkingBg = dark ? QStringLiteral("rgba(118,121,124,0.10)")
                                    : QStringLiteral("rgba(85,94,106,0.10)");
    ui->planSummaryLabel->setStyleSheet(QStringLiteral("QLabel {"
                                                       "  border-radius: 4px;"
                                                       "  padding: 4px 10px;"
                                                       "  font-weight: 600;"
                                                       "  border: 1px solid %1;"
                                                       "  color: %1;"
                                                       "  background: %2;"
                                                       "}")
                                            .arg(checkingColor, checkingBg));
    ui->restoreBtn->setEnabled(false);
    ui->purchaseBtn->setEnabled(false);
    ui->portalBtn->setEnabled(false);
    return;
  }
  ui->restoreBtn->setEnabled(true);
  ui->portalBtn->setEnabled(true);
  refreshPurchaseButtonState();
}

// ── Slots
// ────────────────────────────────────────l─────────────────────────────

void AccountDialog::onLicenseStatusUpdated(
    const AccountLicenseManager::LicenseStatus &status) {
  m_isEvaluationActive = status.evaluationActive && !status.active;
  m_evaluationDaysRemaining = status.evaluationDaysRemaining;
  m_isPaidPlan = status.active;

  const QString licenseType = status.licenseType.trimmed().toLower();
  m_canUpgradePaidPlan =
      m_isPaidPlan && (licenseType != QStringLiteral("lifetime"));

  const bool paidPlan = status.active;
  const bool evaluation = status.evaluationActive && !paidPlan;

  ui->accType->setText(paidPlan     ? QStringLiteral("Pro")
                       : evaluation ? QStringLiteral("Evaluation")
                                    : QStringLiteral("Free"));
  ui->restoreBtn->setEnabled(true);
  ui->accountHintLabel->setVisible(!paidPlan);

  refreshPurchaseButtonState();
  refreshPlanVisuals(paidPlan, evaluation);

  QStringList details;
  if (status.fromNetworkError)
    details << QStringLiteral("Using cached status (network unavailable)");

  // Avoid repeating the exact same state already shown in planSummaryLabel.
  if (!status.message.trimmed().isEmpty() && !evaluation && !paidPlan) {
    const QString message = status.message.trimmed();
    if (!message.compare(QStringLiteral("Evaluation period ended"),
                         Qt::CaseInsensitive)) {
      // Already implied by "Free tier" summary.
    } else {
      details << message;
    }
  }

  if (status.hasExpiryDate && !status.expiryDate.trimmed().isEmpty())
    details << QStringLiteral("Expires: ") + status.expiryDate;
  if (status.hasDaysRemaining)
    details << QStringLiteral("Days left: ") +
                   QString::number(status.daysRemaining);

  refreshHintLabel();

  if (status.active)
    emit licenseActivated(true);

  // Defer resize: word-wrapped QLabel heights are only finalised after the
  // current layout pass completes, so adjustSize() must run next tick.
  QTimer::singleShot(0, this, [this]() { adjustSize(); });
}

void AccountDialog::onLicenseCheckProgressChanged(bool inProgress) const {
  setCheckingState(inProgress);
}

void AccountDialog::highlightPurchaseButton() {
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

  static constexpr int kIntervalMs = 400; // ms per half-cycle
  static constexpr int kTotalCycles = 5;  // 5 × 400 ms = 2 s

  const QString kOnStyle = QStringLiteral("QPushButton {"
                                          "  border: 1px solid #e05050;"
                                          "  border-radius: 4px;"
                                          "  color: #e05050;"
                                          "}");

  m_flashCount = 0;
  m_flashTimer = new QTimer(this);
  m_flashTimer->setInterval(kIntervalMs);

  connect(m_flashTimer, &QTimer::timeout, this, [this, kOnStyle]() {
    // odd count → off, even → on
    ui->purchaseBtn->setStyleSheet((m_flashCount % 2 == 0) ? kOnStyle
                                                           : QString());

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

// ── Private helpers
// ───────────────────────────────────────────────────────────

void AccountDialog::refreshPurchaseButtonState() const {
  if (m_isPaidPlan && m_canUpgradePaidPlan) {
    ui->purchaseBtn->setText(QStringLiteral("Upgrade to New Plan"));
    ui->purchaseBtn->setEnabled(true);
    return;
  }
  ui->purchaseBtn->setText(QStringLiteral("Purchase Pro Licence"));
  ui->purchaseBtn->setEnabled(!m_isPaidPlan);
}

void AccountDialog::refreshHintLabel() const {
  if (m_isEvaluationActive) {
    ui->accountHintLabel->setText(QStringLiteral(
        "Evaluation includes all Pro features. "
        "Purchase a Pro licence to continue after the trial ends."));
    return;
  }
  ui->accountHintLabel->setText(
      QStringLiteral("Upgrade to Pro for access to all premium features "
                     "and priority support."));
}

void AccountDialog::refreshPlanVisuals(bool paidPlan, bool evaluation) const {
  ui->labelEvaluationRemaining->setVisible(!paidPlan);
  ui->evaluationRemainingLabel->setVisible(!paidPlan);

  // Detect active theme via the app-level palette — this is always current
  // even during the initial synchronous status update in the constructor,
  // before the per-widget PaletteChange event has been dispatched.
  const bool dark = QApplication::palette().window().color().lightness() < 128;

  // Colour tokens — each tuple: { borderAndText, background, accTypeText }
  struct BadgeColors {
    QString borderText;
    QString background;
    QString accType;
  };

  const BadgeColors proColors =
      dark ? BadgeColors{QStringLiteral("#4fc37a"),
                         QStringLiteral("rgba(79,195,122,0.12)"),
                         QStringLiteral("#4fc37a")}
           : BadgeColors{QStringLiteral("#1a7a40"),
                         QStringLiteral("rgba(26,122,64,0.12)"),
                         QStringLiteral("#1a7a40")};

  const BadgeColors evalColors =
      dark ? BadgeColors{QStringLiteral("#ffcc66"),
                         QStringLiteral("rgba(255,204,102,0.10)"),
                         QStringLiteral("#ffcc66")}
           : BadgeColors{QStringLiteral("#8a6000"),
                         QStringLiteral("rgba(138,96,0,0.12)"),
                         QStringLiteral("#8a6000")};

  const BadgeColors freeColors =
      dark ? BadgeColors{QStringLiteral("#e05050"),
                         QStringLiteral("rgba(224,80,80,0.10)"),
                         QStringLiteral("#c0c0c0")}
           : BadgeColors{QStringLiteral("#b52a2a"),
                         QStringLiteral("rgba(181,42,42,0.10)"),
                         QStringLiteral("#666666")};

  // Shared badge geometry template.
  static const char kBadgeBase[] = "QLabel {"
                                   "  border-radius: 4px;"
                                   "  padding: 4px 10px;"
                                   "  font-weight: 600;"
                                   "  border: 1px solid %1;"
                                   "  color: %1;"
                                   "  background: %2;"
                                   "}";

  auto applyBadge = [&](const BadgeColors &c) {
    ui->planSummaryLabel->setStyleSheet(
        QString::fromLatin1(kBadgeBase).arg(c.borderText, c.background));
    ui->accType->setStyleSheet(
        QStringLiteral("color: %1; font-weight: 600;").arg(c.accType));
  };

  if (paidPlan) {
    ui->planSummaryLabel->setText(
        QStringLiteral("✔  Plan status: Pro license active"));
    applyBadge(proColors);
    ui->evaluationRemainingLabel->setText(QStringLiteral("Not applicable"));
    return;
  }

  if (evaluation) {
    ui->planSummaryLabel->setText(
        QStringLiteral("Plan status: Evaluation active"));
    applyBadge(evalColors);
    ui->evaluationRemainingLabel->setText(
        m_evaluationDaysRemaining >= 0
            ? QString::number(m_evaluationDaysRemaining) +
                  QStringLiteral(" day") +
                  (m_evaluationDaysRemaining == 1 ? QString()
                                                  : QStringLiteral("s"))
            : QStringLiteral("Calculating..."));
    return;
  }

  // Free / expired
  ui->planSummaryLabel->setText(QStringLiteral("Plan status: Free tier"));
  applyBadge(freeColors);
  ui->evaluationRemainingLabel->setText(QStringLiteral("Ended"));
}

void AccountDialog::onRefreshClicked() const {
  if (!m_accountManager)
    return;
  setCheckingState(true);
  m_accountManager->checkPurchase(m_accountManager->serial(), /*force=*/true);
}

void AccountDialog::onPurchaseClicked() const {
  if (!m_accountManager)
    return;

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
        nullptr, QStringLiteral("Upgrade License"),
        QStringLiteral("Your current Pro license is active.\n\n"
                       "Do you want to open the purchase page to upgrade?"),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Yes)
      return;
  }

  const QString url = m_accountManager->config().checkoutUrlTemplate.arg(
      m_accountManager->serial());
  QDesktopServices::openUrl(QUrl(url));
}

void AccountDialog::onPortalClicked() const {
  if (!m_accountManager)
    return;
  QDesktopServices::openUrl(
      QUrl(m_accountManager->config().selfServicePortalUrl));
}

void AccountDialog::showEvent(QShowEvent *event) {
  QDialog::showEvent(event);
  // Re-apply badge colours on every open — the palette is guaranteed to be
  // fully resolved by the time showEvent fires, even on first open.
  const bool paidPlan = m_isPaidPlan;
  const bool evaluation = m_isEvaluationActive;
  refreshPlanVisuals(paidPlan, evaluation);
}

void AccountDialog::closeEvent(QCloseEvent *event) {
  QDialog::closeEvent(event);
}

void AccountDialog::on_planCompareButton_clicked() {
  PlansComparisionDialog dialog(this);
  dialog.exec();
}
