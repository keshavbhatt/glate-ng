#pragma once

#include <QSettings>
#include <QString>

/**
 * @brief All app-specific configuration for the AccountAndLicense module.
 *
 * Construct one instance per application and pass it to AccountLicenseManager.
 * All policy fields default to the values originally used by WonderWall.
 *
 * @par WonderWall backward-compatible example
 * @code
 * AccountLicenseConfig cfg;
 * cfg.appCode              = QStringLiteral("WW_NG");
 * cfg.appName              = QStringLiteral("WonderWall");
 * cfg.settingsOrgName      = QStringLiteral("ktechpit");
 * cfg.settingsAppName      = QStringLiteral("WonderWall");
 * cfg.encryptionKeySuffix  = QStringLiteral("WW_LOCAL_LICENSE_KEY_v1");
 * cfg.keyAliasPrefix       = QStringLiteral("WW_KEY_ALIAS_v1|");
 * cfg.activationKey        = QStringLiteral("wonderwall");
 * cfg.serialKey            = QStringLiteral("conf");
 * cfg.checkStatusEndpoint  = QStringLiteral("https://ktechpit.com/USS/public/check/status.php");
 * // ...etc.
 * @endcode
 *
 * @par New project (glate) example
 * @code
 * AccountLicenseConfig cfg;
 * cfg.appCode             = QStringLiteral("GLATE");
 * cfg.appName             = QStringLiteral("Glate");
 * cfg.settingsOrgName     = QStringLiteral("ktechpit");
 * cfg.settingsAppName     = QStringLiteral("glate");
 * cfg.checkStatusEndpoint = QStringLiteral("https://ktechpit.com/USS/public/check/status.php");
 * // ...etc.
 * @endcode
 */
struct AccountLicenseConfig
{
    // ── App identity ──────────────────────────────────────────────────────────
    QString appCode;  ///< Short code sent to the API, e.g. "WW_NG"
    QString appName;  ///< Human-readable name, e.g. "WonderWall"

    // ── QSettings parameters ──────────────────────────────────────────────────
    QString settingsOrgName;   ///< QSettings organisation, e.g. "ktechpit"
    QString settingsAppName;   ///< QSettings application,  e.g. "WonderWall"
    QSettings::Format settingsFormat = QSettings::NativeFormat;

    // ── Encryption key derivation ─────────────────────────────────────────────
    /// Suffix mixed into the machine-bound XOR protection key.
    /// Set to "WW_LOCAL_LICENSE_KEY_v1" when migrating existing WonderWall data.
    QString encryptionKeySuffix = QStringLiteral("AL_LOCAL_LICENSE_KEY_v1");

    /// Prefix used when hashing sensitive QSettings key names.
    /// Set to "WW_KEY_ALIAS_v1|" when migrating existing WonderWall data.
    QString keyAliasPrefix = QStringLiteral("AL_KEY_ALIAS_v1|");

    // ── Settings key names ────────────────────────────────────────────────────
    /// Key that stores the activation flag.  Was "wonderwall" in WonderWall.
    QString activationKey = QStringLiteral("activated");
    /// Key that stores the serial.  Was "conf" in WonderWall.
    QString serialKey = QStringLiteral("serial");

    // ── API endpoints ─────────────────────────────────────────────────────────
    QString checkStatusEndpoint;
    /// Checkout URL template; %1 is replaced with the accountId.
    QString checkoutUrlTemplate;
    QString selfServicePortalUrl;
    /// Upgrade-dialog URL template; %1 = appName, %2 = accountId.
    QString upgradeDialogUrlTemplate;

    // ── License policy ────────────────────────────────────────────────────────
    int    freeDownloadCap            = 20;
    int    evaluationDailyDownloadCap = 8;
    qint64 evaluationDurationSecs     = 14LL * 24LL * 60LL * 60LL;
    qint64 weeklyCheckSecs            =  7LL * 24LL * 60LL * 60LL;
    qint64 lifetimeRecheckSecs        = 90LL * 24LL * 60LL * 60LL;
    qint64 clockSkewToleranceSecs     = 10LL * 60LL;

    // ── Obfuscated storage leaf names ─────────────────────────────────────────
    /// Leaf directory under QStandardPaths::AppDataLocation for the serial file.
    QString serialStoreDirLeaf  = QStringLiteral("._t6p2h");
    QString serialStoreFileName = QStringLiteral("._i3n.dat");
    /// Leaf directory/file for the encrypted settings-backup store.
    QString backupStoreDirLeaf  = QStringLiteral("._a9v1k");
    QString backupStoreFileName = QStringLiteral("._s4d.dat");
};

