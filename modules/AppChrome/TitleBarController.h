#ifndef APPCHROME_TITLEBARCONTROLLER_H
#define APPCHROME_TITLEBARCONTROLLER_H

#include <QObject>
#include <Qt>

#include "WindowChrome.h"

class QWidget;
class QLabel;
class QPoint;
class WinControlButton;

// ── TitleBarController ────────────────────────────────────────────────────────
//
// Runtime engine behind WindowChrome::install(). Builds the title bar strip
// (mounted via QMainWindow::setMenuWidget) and the event filters implementing
// window move / resize / state. Created by WindowChrome::install and parented to
// the window — not meant to be constructed directly.
class TitleBarController : public QObject {
  Q_OBJECT
public:
  TitleBarController(QWidget *window, const WindowChrome::Config &cfg);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private:
  void buildStrip();
  // Resize the edge overlay to the window and hide it while maximized/fullscreen
  // (the window edges are flush with the screen, so a border there is pointless).
  void updateBorder();
  void toggleMaximize();
  void syncMaximizeButton();
  void setTitle(const QString &title);

  // Resize hit-testing (global coordinates).
  Qt::Edges edgesAt(const QPoint &globalPos) const;
  void updateResizeCursor(Qt::Edges edges);
  void clearResizeCursor();

  QWidget *m_window = nullptr;
  WindowChrome::Config m_cfg;

  QWidget *m_strip = nullptr;
  QWidget *m_border = nullptr; // hairline edge overlay (optional)
  QLabel *m_titleLabel = nullptr;
  QLabel *m_iconLabel = nullptr;
  WinControlButton *m_minBtn = nullptr;
  WinControlButton *m_maxBtn = nullptr;
  WinControlButton *m_closeBtn = nullptr;

  bool m_hasOverrideCursor = false;
};

#endif // APPCHROME_TITLEBARCONTROLLER_H
