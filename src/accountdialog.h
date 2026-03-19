#pragma once

#include <QDialog>
#include "AccountLicenseManager.h"

namespace Ui { class AccountDialog; }
class QCloseEvent;

class AccountDialog : public QDialog {
    Q_OBJECT
public:
    explicit AccountDialog(QWidget *parent = nullptr);
    ~AccountDialog() override;

    /// Wire up the manager; connects licenseStatusUpdated / progress signals.
    void setAccountManager(AccountLicenseManager *accountManager);

    void setSerial(const QString &serial) const;
    void setAccountType(bool isPro) const;
    void setCheckingState(bool checking) const;

public slots:
    void onLicenseStatusUpdated(const AccountLicenseManager::LicenseStatus &status);
    void onLicenseCheckProgressChanged(bool inProgress) const;

signals:
    /// Emitted when a valid Pro licence is confirmed.
    void licenseActivated(bool isPro);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void refreshHintLabel() const;
    void refreshPlanVisuals(bool paidPlan, bool evaluation) const;
    void refreshPurchaseButtonState() const;

    void onRefreshClicked() const;
    void onPurchaseClicked() const;
    void onPortalClicked() const;

    Ui::AccountDialog       *ui;
    AccountLicenseManager   *m_accountManager        = nullptr;
    bool                     m_isPaidPlan             = false;
    bool                     m_canUpgradePaidPlan     = false;
    bool                     m_isEvaluationActive     = false;
    int                      m_evaluationDaysRemaining = -1;
};

