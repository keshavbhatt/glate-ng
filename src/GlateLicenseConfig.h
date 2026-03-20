#pragma once


/**
 * @brief Returns the AccountLicenseConfig for Glate.
 *
 * Used to construct the AccountLicenseManager in MainWindow:
 * @code
 *   m_licenseManager = new AccountLicenseManager(glateAccountLicenseConfig(), this);
 * @endcode
 */
inline AccountLicenseConfig glateAccountLicenseConfig()
{
    AccountLicenseConfig cfg;

    // ── App identity ──────────────────────────────────────────────────────────
    cfg.appCode  = QStringLiteral("GLATE");
    cfg.appName  = QStringLiteral("Glate");

    // ── QSettings ─────────────────────────────────────────────────────────────
    // Must match QApplication::setOrganizationName / setApplicationName in main.cpp
    cfg.settingsOrgName = QApplication::organizationName();
    cfg.settingsAppName = QApplication::applicationName();

    // ── API endpoints ─────────────────────────────────────────────────────────
    cfg.checkStatusEndpoint =
        QStringLiteral("https://ktechpit.com/USS/public/check/status.php");
    // %1 is replaced with the accountId at runtime
    cfg.checkoutUrlTemplate =
        QStringLiteral("https://ktechpit.com/USS/public/checkout.php?slug=glate&accountId=%1");
    cfg.selfServicePortalUrl =
        QStringLiteral("https://ktechpit.com/USS/public/user/");
    // %1 = appName, %2 = accountId
    cfg.upgradeDialogUrlTemplate =
        QStringLiteral("https://ktechpit.com/USS/WW/accounts/html/index.php?itemName=%1&accountId=%2");

    // ── Policy ────────────────────────────────────────────────────────────────
    cfg.evaluationDurationSecs = 30 * 24 * 3600; // 30 days
    return cfg;
}

