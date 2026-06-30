#include "WindowChrome.h"

#include "TitleBarController.h"

#include <QApplication>
#include <QColorDialog>
#include <QDialog>
#include <QEvent>
#include <QFileDialog>
#include <QFontDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QWidget>

namespace WindowChrome {

TitleBarController *install(QWidget *w, const Config &cfg) {
  if (!w)
    return nullptr;

  // Frameless: must be set before the native window is realised (i.e. before
  // show()), so call install() from the window constructor.
  w->setWindowFlag(Qt::FramelessWindowHint, true);

  // The controller builds the title strip + wires move/resize handling; parented
  // to the window so its lifetime matches.
  return new TitleBarController(w, cfg);
}

namespace {

// True for dialogs we should NOT decorate — native/system dialogs that must keep
// their own behaviour (the file picker routes through the XDG portal in a
// sandbox, message boxes auto-size, etc.).
bool isSystemDialog(QWidget *w) {
  return qobject_cast<QFileDialog *>(w) || qobject_cast<QMessageBox *>(w) ||
         qobject_cast<QColorDialog *>(w) || qobject_cast<QFontDialog *>(w) ||
         qobject_cast<QInputDialog *>(w) || qobject_cast<QProgressDialog *>(w) ||
         w->inherits("QWizard");
}

// Application-wide filter that decorates plain QDialogs on first show.
class DialogChromeInstaller : public QObject {
public:
  DialogChromeInstaller(Config cfg, QObject *parent)
      : QObject(parent), m_cfg(cfg) {}

protected:
  bool eventFilter(QObject *obj, QEvent *event) override {
    if (event->type() != QEvent::Show)
      return false;
    auto *w = qobject_cast<QWidget *>(obj);
    if (!w || !w->isWindow())
      return false;
    // The app's own top-level windows: QDialogs, dialog-flagged QWidgets (e.g.
    // an "About" window) and plain Qt::Window widgets shown as independent
    // windows (e.g. a "Media Information" viewer). Popups, tooltips and combo
    // drop-downs have other window types and are left alone.
    const Qt::WindowType wt = w->windowType();
    if (wt != Qt::Dialog && wt != Qt::Window)
      return false;
    // Main windows manage their own chrome (WindowChrome::install from their
    // constructor); never auto-decorate one.
    if (w->inherits("QMainWindow"))
      return false;
    if (w->property(kMarker).toBool())
      return false;
    if (w->windowFlags() & Qt::FramelessWindowHint || isSystemDialog(w))
      return false;

    w->setProperty(kMarker, true);

    // Each dialog gets its own title/icon; the rest of the template is shared.
    Config cfg = m_cfg;
    cfg.title = w->windowTitle();
    cfg.icon = w->windowIcon();
    install(w, cfg);

    // Toggling the frameless flag on an already-created window hides it — re-show
    // (the marker stops this re-entering). One brief flicker on first open.
    w->setVisible(true);
    return false;
  }

private:
  static constexpr const char *kMarker = "appchrome_decorated";
  Config m_cfg;
};

} // namespace

void enableAutoForDialogs(QApplication &app, const Config &cfg) {
  app.installEventFilter(new DialogChromeInstaller(cfg, &app));
}

} // namespace WindowChrome
