#include "mainwindow.h"
#include <QApplication>
#include <QGuiApplication>

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
  QApplication::setApplicationVersion("4.0");
  MainWindow w;

  w.show();

  return a.exec();
}
