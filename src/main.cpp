#include "AppCatalog.h"
#include "mainwindow.h"
#include <QApplication>
#include <QColor>
#include <QGuiApplication>
#include <QLocale>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSettings>
#include <QString>
#include <QTimer>
#include <QTranslator>

#include "glateshell.h"
#include "LogSink.h"
#include "Logging.h"

#include <AppTheme.h>
#include <ThemeController.h>
#include <WindowChrome.h>

// Unique name for the local socket used as a single-instance lock.
static constexpr auto kInstanceKey = "com.ktechpit.glate.SingleInstance";

int main(int argc, char *argv[]) {
  // First, so every message from here on reaches the log (modules/AppShell).
  glate::shell::LogSink::install();

  // Qt6 on Linux routes qDebug/qWarning to journald when the process is not
  // attached to a TTY (e.g. when launched from an IDE).  Force stderr output
  // so the IDE Run console always captures debug logs.
#ifdef QT_DEBUG
  qputenv("QT_FORCE_STDERR_LOGGING", "1");
#endif

  QApplication a(argc, argv);
  QGuiApplication::setDesktopFileName("com.ktechpit.glate");
  QApplication::setApplicationName("Glate");
  QApplication::setOrganizationName("org.keshavnrj.ubuntu");
  QApplication::setApplicationVersion(QString::fromLatin1(GLATE_APP_VERSION));

  // Load the UI translation. "system" follows the OS locale.
  QTranslator translator;
  const QString uiLang = QSettings().value("ui_language", "system").toString();
  QStringList localeCandidates;
  if (uiLang.isEmpty() || uiLang == QLatin1String("system"))
    localeCandidates = QLocale::system().uiLanguages();
  else
    localeCandidates << uiLang;
  for (const QString &locale : localeCandidates) {
    const QString baseName = QStringLiteral("Glate_") + QLocale(locale).name();
    if (translator.load(QStringLiteral(":/i18n/") + baseName)) {
      a.installTranslator(&translator);
      break;
    }
  }

  // "--quick-translate": translate the selected text, for a desktop shortcut on
  // desktops where the app cannot add one itself (no shortcut portal).
  const bool quickTranslate =
      QCoreApplication::arguments().contains(QStringLiteral("--quick-translate"));

  // ── Single-instance guard ─────────────────────────────────────────────────
  // Try to contact an already-running instance first.
  {
    QLocalSocket probe;
    probe.connectToServer(kInstanceKey);
    if (probe.waitForConnected(500)) {
      // Another instance is running, ask it to raise itself and exit.
      qCInfo(glate::shell::lcApp) << "another instance is running; passing the request on";
      // "<command>\n<activation token>": the token lets the running window
      // come to the front on Wayland.
      probe.write((quickTranslate ? QByteArray("quick") : QByteArray("raise")) +
                  '\n' + qgetenv("XDG_ACTIVATION_TOKEN"));
      probe.flush();
      probe.waitForBytesWritten(500);
      return 0;
    }
  }

  // No existing instance: claim the server name (remove any stale socket file
  // left over from a previous crash) and start listening.
  QLocalServer::removeServer(kInstanceKey);
  QLocalServer instanceServer;
  if (!instanceServer.listen(kInstanceKey))
    qCWarning(glate::shell::lcApp)
        << "single instance: cannot listen, error" << instanceServer.serverError();

  // Only the running instance writes the log file.
  glate::shell::LogSink::setLogFile(GlateShell::logFilePath());
  qCInfo(glate::shell::lcApp).noquote()
      << "Glate" << QApplication::applicationVersion() << "starting, Qt"
      << qVersion() << "on" << QGuiApplication::platformName();
  // ─────────────────────────────────────────────────────────────────────────

  // ── AppChrome: self-contained theming + client-side decoration ────────────
  // Apply the light/dark mode picked in Settings → Appearance
  // (Fusion/Fusion 2, follow-system/light/dark) and track the OS scheme live.
  // Replaces glate's bundled qbreeze stylesheet.
  QSettings appSettings; // org/app name already set above
  // Glate has one theme, Fusion 2; only light, dark or follow system is
  // stored, under a new key. A valid old choice carries over once; the old
  // keys go, so an old "fusion" skin value is never read again.
  if (!appSettings.contains("theme/mode")) {
    const QString old = appSettings.value("appearance/mode").toString();
    const bool valid = old == QLatin1String("light") ||
                       old == QLatin1String("dark") ||
                       old == QLatin1String("system");
    appSettings.setValue("theme/mode", valid ? old : QStringLiteral("system"));
  }
  appSettings.remove("appearance/skin");
  appSettings.remove("appearance/mode");
  appSettings.remove("appearance/accent"); // one fixed accent now
  const QString mode = appSettings.value("theme/mode", "system").toString();
  // Omnidict's blues, inside the range of Glate's icon gradient: #1C71D8 fills
  // selections and primary buttons in both schemes (white text reads on it);
  // on dark, text, links and glyphs in the accent use the lighter #62A0EA.
  const QColor accent(0x1c, 0x71, 0xd8);
  AppTheme::Accents accents;
  accents.highlight = accent;
  accents.accentTextDark = QColor(0x62, 0xa0, 0xea);
  auto *themeController = new ThemeController(a, accents, &a);
  themeController->configure(AppTheme::Skin::Fusion2,
                             AppTheme::modeFromString(mode), accent);

  // Client-side decoration (opt-in via Settings). When on, give the app's own
  // dialogs the same custom title bar (close + maximize).
  if (appSettings.value("appearance/csd", WindowChrome::defaultEnabled()).toBool()) {
    WindowChrome::Config dialogCfg;
    dialogCfg.showMinimize = false;
    dialogCfg.showMaximize = true;
    WindowChrome::enableAutoForDialogs(a, dialogCfg);
  }

  // "More apps from Ktechpit" cards (ADR-010): which store "Get" opens
  // depends on how this copy was installed.
  ktechpit::CatalogConfig appsConfig;
  appsConfig.hostAppId = QStringLiteral("glate");
  appsConfig.source = QStringLiteral("glate");
  if (qEnvironmentVariableIsSet("SNAP"))
    appsConfig.package = ktechpit::Package::Snap;
  else if (qEnvironmentVariableIsSet("FLATPAK_ID"))
    appsConfig.package = ktechpit::Package::Flatpak;
  else if (qEnvironmentVariableIsSet("APPIMAGE"))
    appsConfig.package = ktechpit::Package::AppImage;
#ifdef Q_OS_WIN
  else
    appsConfig.package = ktechpit::Package::Windows;
#endif
  ktechpit::AppCatalog::init(appsConfig);

  MainWindow w(themeController);

  // When a second instance connects, bring this window to the front.
  QObject::connect(&instanceServer, &QLocalServer::newConnection, &w,
                   [&instanceServer, &w]() {
    QLocalSocket *conn = instanceServer.nextPendingConnection();
    if (conn == nullptr)
      return;
    QObject::connect(conn, &QLocalSocket::disconnected, conn,
                     &QObject::deleteLater);
    const auto handle = [conn, &w]() {
      const QByteArray message = conn->readAll();
      const QList<QByteArray> parts = message.split('\n');
      const QByteArray command = parts.value(0).trimmed();
      const QByteArray token = parts.value(1).trimmed();
      if (command == "quick") {
        w.quickTranslate(token);
        return;
      }
      if (!token.isEmpty())
        qputenv("XDG_ACTIVATION_TOKEN", token);
      w.showNormal();
      w.raise();
      w.activateWindow();
    };
    if (conn->bytesAvailable() > 0)
      handle();
    else
      QObject::connect(conn, &QLocalSocket::readyRead, &w, handle,
                       Qt::SingleShotConnection);
  });

  w.show();
  w.debugRun();
  if (quickTranslate)
    QTimer::singleShot(0, &w, [&w]() { w.quickTranslate(qgetenv("XDG_ACTIVATION_TOKEN")); });

  return a.exec();
}
