#pragma once

#include "AccountLicenseConfig.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

class AccountLicenseStore;

/**
 * @brief App-agnostic manager for serial IDs and remote licence verification.
 *
 * Handles:
 *  - Serial / account-ID generation and obfuscated persistence
 *  - Asynchronous remote licence-status checks via a configurable API endpoint
 *  - Local anomaly detection (clock skew, account-ID drift)
 *  - Evaluation period tracking
 *  - Weekly / lifetime re-check scheduling
 *
 * Configure it once via AccountLicenseConfig and reuse across projects.
 *
 * @par Typical usage
 * @code
 * AccountLicenseConfig cfg = makeMyAppConfig();
 * auto *mgr = new AccountLicenseManager(cfg, this);
 * connect(mgr, &AccountLicenseManager::licenseStatusUpdated,
 *         this, &MyWindow::onLicenseStatus);
 * mgr->ensureSerial();
 * mgr->checkPurchase(mgr->serial());
 * @endcode
 */
class AccountLicenseManager : public QObject
{
    Q_OBJECT
public:
    /// Full description of the current licence state.
    struct LicenseStatus {
        bool    active                  = false;
        bool    evaluationActive        = false;
        bool    hasPaidAccess           = false;
        QString message;
        QString expiryDate;
        qint64  expiryTimestamp         = 0;
        qint64  evaluationEndTimestamp  = 0;
        qint64  nextCheckTimestamp      = 0;
        QString licenseType;
        int     daysRemaining           = -1;
        int     evaluationDaysRemaining = -1;
        bool    hasExpiryDate           = false;
        bool    hasExpiryTimestamp      = false;
        bool    hasDaysRemaining        = false;
        bool    fromNetworkError        = false;
        bool    anomalyDetected         = false;
    };

    explicit AccountLicenseManager(const AccountLicenseConfig &config,
                                   QObject *parent = nullptr);
    ~AccountLicenseManager() override;

    // ── Core operations ───────────────────────────────────────────────────────

    /// The current serial / account ID (empty until ensureSerial() is called).
    QString serial() const;

    /// Load serial from persistent storage or generate a new one, then emit
    /// serialReady() and initialise the local licence state.
    void ensureSerial();

    /// Asynchronously verify @p serial against the remote licence server.
    /// @p force bypasses weekly re-check scheduling.
    void checkPurchase(const QString &serial, bool force = false);

    // ── State queries ─────────────────────────────────────────────────────────
    bool hasPremiumAccess()   const;
    bool isEvaluationActive() const;

    // ── Config-driven URL builders ────────────────────────────────────────────
    QString checkoutUrl()          const;
    QString selfServicePortalUrl() const;
    QString upgradeDialogUrl()     const;

    // ── Policy accessors ──────────────────────────────────────────────────────
    int freeDownloadCap()            const;
    int evaluationDailyDownloadCap() const;

    const AccountLicenseConfig &config() const;

signals:
    void serialReady(const QString &serial);
    void licenseCheckInProgressChanged(bool inProgress);
    void licenseStatusUpdated(const AccountLicenseManager::LicenseStatus &status);

private slots:
    void onCheckRequestFinished();

private:
    // Internal helpers
    static qint64  nowUtcSecs();
    static QString normalizeLicenseType(const QString &type);
    qint64  defaultNextCheckUtc(const QString &licenseType,
                                qint64 nowUtc, bool anomaly) const;
    bool    detectLocalAnomaly(const QString &accountId, qint64 nowUtc);
    LicenseStatus buildCachedStatus() const;
    bool shouldPerformNetworkCheck(const LicenseStatus &cached,
                                   bool forceCheck) const;

    // Serial file storage
    QString serialFilePath()                       const;
    void    saveSerialToFile(const QString &serial) const;
    QString loadSerialFromFile()                    const;

    // ID generation
    static QString generateId(int length = 24);

    AccountLicenseConfig  m_config;
    AccountLicenseStore  *m_store           = nullptr;
    bool                  m_hasPremiumAccess = false;
    bool                  m_evaluationActive = false;
    QNetworkAccessManager m_nam;
    QString               m_serial;
    int                   m_pendingChecks    = 0;
};

