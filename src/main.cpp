#include "mainwindow.h"
#include <QApplication>
#include <QColor>
#include <QGuiApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSettings>
#include <QString>

#include <AppTheme.h>
#include <ThemeController.h>
#include <WindowChrome.h>

// Unique name for the local socket used as a single-instance lock.
static constexpr auto kInstanceKey = "com.ktechpit.glate.SingleInstance";

int main(int argc, char *argv[]) {
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

  // ── Single-instance guard ─────────────────────────────────────────────────
  // Try to contact an already-running instance first.
  {
    QLocalSocket probe;
    probe.connectToServer(kInstanceKey);
    if (probe.waitForConnected(500)) {
      // Another instance is running — ask it to raise itself and exit.
      qInfo() << "Another instance is already running; asking it to raise itself.";
      probe.write("raise");
      probe.flush();
      probe.waitForBytesWritten(500);
      return 0;
    }
  }

  // No existing instance: claim the server name (remove any stale socket file
  // left over from a previous crash) and start listening.
  QLocalServer::removeServer(kInstanceKey);
  QLocalServer instanceServer;
  instanceServer.listen(kInstanceKey);
  // ─────────────────────────────────────────────────────────────────────────

  // ── AppChrome: self-contained theming + client-side decoration ────────────
  // Apply the skin/appearance/accent the user picked in Settings → Appearance
  // (Fusion/Fusion 2, follow-system/light/dark) and track the OS scheme live.
  // Replaces glate's bundled qbreeze stylesheet.
  QSettings appSettings; // org/app name already set above
  const QString skin = appSettings.value("appearance/skin", "fusion").toString();
  const QString mode = appSettings.value("appearance/mode", "system").toString();
  QColor accent(appSettings.value("appearance/accent", "#4a78ff").toString());
  if (!accent.isValid())
    accent = QColor(0x4a, 0x78, 0xff);
  auto *themeController = new ThemeController(a, AppTheme::Accents{}, &a);
  themeController->configure(AppTheme::skinFromString(skin),
                             AppTheme::modeFromString(mode), accent);

  // Client-side decoration (opt-in via Settings). When on, give the app's own
  // dialogs the same custom title bar (close + maximize).
  if (appSettings.value("appearance/csd", true).toBool()) {
    WindowChrome::Config dialogCfg;
    dialogCfg.showMinimize = false;
    dialogCfg.showMaximize = true;
    WindowChrome::enableAutoForDialogs(a, dialogCfg);
  }

  MainWindow w(themeController);

  // When a second instance connects, bring this window to the front.
  QObject::connect(&instanceServer, &QLocalServer::newConnection, &w,
                   [&instanceServer, &w]() {
    if (QLocalSocket *conn = instanceServer.nextPendingConnection()) {
      conn->deleteLater();
    }
    w.showNormal();
    w.raise();
    w.activateWindow();
  });

  w.show();

  return a.exec();
}
