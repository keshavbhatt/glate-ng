#ifndef APPCHROME_WINDOWCHROME_H
#define APPCHROME_WINDOWCHROME_H

#include <QIcon>
#include <QString>

class QWidget;
class QApplication;
class TitleBarController;

// ── WindowChrome ──────────────────────────────────────────────────────────────
//
// Client-side decoration (CSD) for a top-level QWidget: makes the window
// frameless and adds its own title bar strip at the very top — app icon/title on
// the left, minimize/maximize/close on the right — plus drag-to-move, edge/corner
// resize and double-click-to-maximize. On a QMainWindow the strip is mounted via
// setMenuWidget (above any toolbar); on any other widget (e.g. a QDialog) the
// existing layout is wrapped beneath the strip. The window controls are painted
// by the toolkit-independent WinControlButton (no bundled assets) and follow the
// active palette. Uses QWindow::startSystemMove/startSystemResize so it works on
// both X11 and Wayland. Reusable across projects.
//
// Call install() ONLY when CSD is enabled, from the window constructor (before
// show()), so the frameless flag is set before the native window is created.
namespace WindowChrome {

struct Config {
  QString title;          // strip title (defaults to the window title)
  QIcon icon;             // strip app icon (defaults to the window icon)
  int resizeMarginPx = 6; // edge hit-test thickness
  bool showTitle = true;
  bool showIcon = true;
  bool showMinimize = true;  // minimize button
  bool showMaximize = true;  // maximize/restore button (hidden if non-resizable)
  bool drawBorder = true;    // hairline themed edge around the frameless window so
                             // overlapping windows with the same background stay
                             // visually distinct
};

// Installs CSD on `w`. Returns a controller parented to `w` (you may ignore it).
// Returns nullptr if `w` is null.
TitleBarController *install(QWidget *w, const Config &cfg);

// Auto-apply CSD to every plain QDialog the application opens, using `cfg` as the
// template (a fresh title/icon is taken from each dialog). System/native dialogs
// — file picker (XDG portal), message boxes, colour/font/input/progress dialogs,
// wizards and popups — are deliberately skipped so they keep their native
// behaviour. Call once at startup when CSD is enabled.
void enableAutoForDialogs(QApplication &app, const Config &cfg);

} // namespace WindowChrome

#endif // APPCHROME_WINDOWCHROME_H
