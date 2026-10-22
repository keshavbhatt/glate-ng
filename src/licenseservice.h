// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: LicenseRef-Ktechpit-Licensing-Module

#pragma once

#include <QObject>
#include <QString>

class AccountLicenseManager;

/**
 * @brief Application-wide Pro-feature license gate.
 *
 * Any class can call LicenseService::instance()->checkAccess("Feature Name")
 * to enforce a Pro-feature gate without knowing about AccountLicenseManager,
 * MainWindow, or AccountDialog.
 *
 * Typical call site:
 * @code
 *   if (!LicenseService::instance()->checkAccess(tr("Export as Audio")))
 *       return;
 * @endcode
 *
 * MainWindow initialises the singleton, keeps hasPaidAccess() in sync via
 * setHasPaidAccess(), and connects upgradeRequested() to the Account dialog.
 */
class LicenseService : public QObject
{
    Q_OBJECT
public:
    /// Describes where the user stands relative to the evaluation period.
    enum class EvaluationState {
        Unknown,  ///< No evaluation data yet (first launch)
        Active,   ///< Trial is currently running
        Expired,  ///< Trial ran and has since ended
    };
    Q_ENUM(EvaluationState)

    /// Returns the application-wide singleton, creating it on first call.
    /// The instance is parented to qApp and lives for the process lifetime.
    static LicenseService *instance();

    /**
     * @brief Gate a Pro feature.
     *
     * @return true  — user has paid access; caller may proceed.
     * @return false — no paid access; a polite prompt was shown.
     *                 If the user clicked "View Plans", upgradeRequested()
     *                 was emitted so MainWindow can open the Account dialog.
     */
    bool checkAccess(const QString &featureName);

    /**
     * @brief Gate one TTS playback request in the free tier.
     *
     * Free users may play TTS up to kFreeTierDailyTtsLimit times per day.
     * On the first call that exceeds the limit, an upgrade prompt is shown.
     *
     * @return true  — play is allowed (paid access, or within daily quota).
     * @return false — limit reached; caller should abort playback.
     */
    bool checkTtsAccess();

    static constexpr int kFreeTierDailyTtsLimit = 6;

    /// Called by MainWindow whenever AccountLicenseManager reports a new status.
    void setHasPaidAccess(bool hasPaidAccess);
    void setEvaluationState(EvaluationState state);
    /// Provide the license manager so TTS counters are stored in the
    /// encrypted store rather than plain QSettings.
    void setAccountManager(AccountLicenseManager *mgr);
    /// Set from the AccountLicenseConfig so the upgrade dialog can quote
    /// the actual trial length instead of a hardcoded number.
    void setEvaluationDurationDays(int days);

    bool hasPaidAccess() const;
    EvaluationState evaluationState() const;
    int evaluationDurationDays() const;

signals:
    /// Emitted when the user clicks "View Plans" inside the upgrade prompt.
    void upgradeRequested();

private:
    explicit LicenseService(QObject *parent = nullptr);
    static LicenseService *s_instance;

    // TTS daily-counter helpers
    int  ttsPlaysToday() const;
    void incrementTtsPlays() const;

    bool            m_hasPaidAccess       = false;
    EvaluationState m_evaluationState     = EvaluationState::Unknown;
    int             m_evaluationDurationDays = 14;
    AccountLicenseManager *m_accountManager = nullptr;
};
