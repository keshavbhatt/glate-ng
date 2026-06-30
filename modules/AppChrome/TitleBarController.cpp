#include "TitleBarController.h"

#include "AppTheme.h"

#include <QAbstractButton>
#include <QApplication>
#include <QEnterEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QStyle>
#include <QVBoxLayout>
#include <QWidget>
#include <QWindow>

// ── WinControlButton ──────────────────────────────────────────────────────────
//
// A toolkit-independent minimize / maximize-restore / close button. Paints its
// own glyph from the active palette (so it follows light/dark themes and looks
// crisp at any DPI) with a rounded hover highlight — red for close. No Q_OBJECT
// needed: it only reuses QAbstractButton's clicked() signal.
class WinControlButton : public QAbstractButton {
public:
  enum Kind { Minimize, MaximizeRestore, Close };

  explicit WinControlButton(Kind kind, QWidget *parent = nullptr)
      : QAbstractButton(parent), m_kind(kind) {
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::ArrowCursor);
    setAttribute(Qt::WA_Hover, true);
  }

  void setMaximized(bool maximized) {
    if (m_maximized != maximized) {
      m_maximized = maximized;
      update();
    }
  }

  QSize sizeHint() const override { return QSize(44, 30); }

protected:
  void enterEvent(QEnterEvent *) override { m_hover = true; update(); }
  void leaveEvent(QEvent *) override { m_hover = false; update(); }

  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QColor fg = palette().color(QPalette::WindowText);

    // Hover / pressed background.
    if (m_hover || isDown()) {
      QColor bg = (m_kind == Close) ? QColor(0xe0, 0x46, 0x46)
                                    : palette().color(QPalette::Midlight);
      if (isDown())
        bg = bg.darker(115);
      QPainterPath path;
      path.addRoundedRect(QRectF(rect()).adjusted(2.5, 2.5, -2.5, -2.5), 5, 5);
      p.fillPath(path, bg);
      if (m_kind == Close)
        fg = Qt::white;
    }

    QPen pen(fg);
    pen.setWidthF(1.3);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::MiterJoin);
    p.setPen(pen);

    const QPointF c = QRectF(rect()).center();
    const qreal s = 4.5; // half glyph size

    switch (m_kind) {
    case Minimize:
      p.drawLine(QPointF(c.x() - s, c.y()), QPointF(c.x() + s, c.y()));
      break;
    case MaximizeRestore:
      if (m_maximized) {
        // Two overlapped squares = "restore".
        const qreal o = 2.0;
        p.drawRect(QRectF(c.x() - s + o, c.y() - s - o, 2 * s, 2 * s));
        p.fillRect(QRectF(c.x() - s - o, c.y() - s + o, 2 * s, 2 * s),
                   palette().color(m_hover || isDown() ? QPalette::Midlight
                                                       : QPalette::Window));
        p.drawRect(QRectF(c.x() - s - o, c.y() - s + o, 2 * s, 2 * s));
      } else {
        p.drawRect(QRectF(c.x() - s, c.y() - s, 2 * s, 2 * s));
      }
      break;
    case Close:
      p.drawLine(QPointF(c.x() - s, c.y() - s), QPointF(c.x() + s, c.y() + s));
      p.drawLine(QPointF(c.x() - s, c.y() + s), QPointF(c.x() + s, c.y() - s));
      break;
    }
  }

private:
  Kind m_kind;
  bool m_hover = false;
  bool m_maximized = false;
};

// ── BorderOverlay ─────────────────────────────────────────────────────────────
//
// A frameless window shares the desktop's compositor with no native frame, so two
// of the app's windows stacked over each other (same palette background) blur into
// one shape. This mouse-transparent overlay sits on top of the whole window and
// paints a single hairline (1px) rectangle in a theme-derived colour, giving the
// window a crisp edge so a child window stays visually distinct. It never fills its
// background, so it doesn't obscure anything beneath the 1px line.
class BorderOverlay : public QWidget {
public:
  explicit BorderOverlay(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setFocusPolicy(Qt::NoFocus);
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    // Match the inside line/divider colour so the window edge reads as part of
    // the same visual language. Single source of truth: AppTheme::separatorColor.
    // Picked by the window lightness so it tracks the active theme.
    const bool dark = palette().color(QPalette::Window).lightnessF() < 0.5;
    QPen pen(AppTheme::separatorColor(dark));
    pen.setWidth(1); // hairline
    pen.setCosmetic(true);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    // Inset by half the device pixel so the 1px stroke lands fully inside the rect.
    p.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
  }
};

// ── TitleStrip ────────────────────────────────────────────────────────────────
//
// The title bar container. Icon (left) and window controls (right) live in a
// normal layout; the title label is positioned across the FULL width so it
// centres on the window centre regardless of the asymmetric sides. The label is
// transparent to mouse events, so it never blocks the controls or window drag.
class TitleStrip : public QWidget {
public:
  explicit TitleStrip(QWidget *parent = nullptr) : QWidget(parent) {}

  void setCenteredLabel(QLabel *label) {
    m_label = label;
    if (m_label) {
      m_label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
      m_label->setAlignment(Qt::AlignCenter);
      relayout();
    }
  }

protected:
  void resizeEvent(QResizeEvent *) override { relayout(); }

private:
  void relayout() {
    if (m_label)
      m_label->setGeometry(rect());
  }
  QLabel *m_label = nullptr;
};

// ── TitleBarController ────────────────────────────────────────────────────────

TitleBarController::TitleBarController(QWidget *window,
                                      const WindowChrome::Config &cfg)
    : QObject(window), m_window(window), m_cfg(cfg) {
  buildStrip();

  // Resize hit-testing needs to see mouse movement anywhere over the window
  // (including over child widgets) — filter at the application level, scoped to
  // this window. Move/double-click are handled via the strip filter.
  qApp->installEventFilter(this);
  m_strip->installEventFilter(this);
  m_window->installEventFilter(this);

  connect(m_window, &QWidget::windowTitleChanged, this,
          [this](const QString &t) { setTitle(t); });

  // Hairline edge so overlapping same-coloured windows stay distinct. Kept sized
  // and raised in the event filter (Resize/Show).
  if (m_cfg.drawBorder) {
    m_border = new BorderOverlay(m_window);
    updateBorder();
  }

  syncMaximizeButton();
}

void TitleBarController::buildStrip() {
  auto *strip = new TitleStrip;
  m_strip = strip;
  m_strip->setObjectName(QStringLiteral("AppChromeTitleBar"));
  // Use the application's default font (not whatever the toolbar may enlarge to).
  m_strip->setFont(QApplication::font());

  auto *lay = new QHBoxLayout(m_strip);
  lay->setContentsMargins(8, 1, 1, 1);
  lay->setSpacing(8);

  if (m_cfg.showIcon) {
    m_iconLabel = new QLabel(m_strip);
    const int sz = m_window->style()->pixelMetric(QStyle::PM_SmallIconSize);
    const QIcon icon = m_cfg.icon.isNull() ? m_window->windowIcon() : m_cfg.icon;
    m_iconLabel->setPixmap(icon.pixmap(sz, sz));
    lay->addWidget(m_iconLabel);
  }

  lay->addStretch(1);

  if (m_cfg.showMinimize) {
    m_minBtn = new WinControlButton(WinControlButton::Minimize, m_strip);
    m_minBtn->setToolTip(tr("Minimize"));
    lay->addWidget(m_minBtn);
    connect(m_minBtn, &QAbstractButton::clicked, m_window,
            &QWidget::showMinimized);
  }
  // Only offer maximize on a resizable window — a fixed-size dialog can't use it.
  const bool resizable = m_window->maximumWidth() >= QWIDGETSIZE_MAX &&
                         m_window->maximumHeight() >= QWIDGETSIZE_MAX;
  if (m_cfg.showMaximize && resizable) {
    m_maxBtn = new WinControlButton(WinControlButton::MaximizeRestore, m_strip);
    lay->addWidget(m_maxBtn);
    connect(m_maxBtn, &QAbstractButton::clicked, this,
            &TitleBarController::toggleMaximize);
  }
  m_closeBtn = new WinControlButton(WinControlButton::Close, m_strip);
  m_closeBtn->setToolTip(tr("Close"));
  lay->addWidget(m_closeBtn);
  connect(m_closeBtn, &QAbstractButton::clicked, m_window, &QWidget::close);

  // Title: centred across the full strip width (not in the layout), so it sits at
  // the window centre regardless of the icon/controls on either side.
  if (m_cfg.showTitle) {
    m_titleLabel = new QLabel(m_strip);
    m_titleLabel->setObjectName(QStringLiteral("AppChromeTitleLabel"));
    strip->setCenteredLabel(m_titleLabel);
    // Keep it under the icon/controls so a long title is overpainted by them
    // rather than spilling across — short titles still show centred.
    m_titleLabel->lower();
  }

  // Mount the strip at the top. On a QMainWindow use the menu-widget slot (above
  // any toolbar); on any other widget wrap its existing layout beneath the strip.
  if (auto *mw = qobject_cast<QMainWindow *>(m_window)) {
    mw->setMenuWidget(m_strip);
  } else {
    auto *content = new QWidget(m_window);
    if (QLayout *old = m_window->layout())
      content->setLayout(old); // re-homes the existing layout + its items
    auto *outer = new QVBoxLayout(m_window);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(m_strip);
    outer->addWidget(content, 1);
    // The strip eats client-area height the dialog's preset size didn't allow
    // for — grow the window so content isn't clipped. Idempotent (the target is
    // content+strip, so it won't creep across re-opens/saved geometry).
    const int needed =
        content->sizeHint().height() + m_strip->sizeHint().height();
    if (m_window->height() < needed)
      m_window->resize(m_window->width(), needed);
  }

  setTitle(m_cfg.title.isEmpty() ? m_window->windowTitle() : m_cfg.title);
}

void TitleBarController::updateBorder() {
  if (!m_border)
    return;
  // No edge while maximized/fullscreen — the window is flush with the screen.
  const bool framed = !m_window->isMaximized() && !m_window->isFullScreen();
  m_border->setVisible(framed);
  if (framed) {
    m_border->setGeometry(m_window->rect());
    m_border->raise();
  }
}

void TitleBarController::setTitle(const QString &title) {
  if (m_titleLabel && !title.isEmpty())
    m_titleLabel->setText(title);
}

void TitleBarController::toggleMaximize() {
  if (m_window->isMaximized())
    m_window->showNormal();
  else
    m_window->showMaximized();
}

void TitleBarController::syncMaximizeButton() {
  if (!m_maxBtn)
    return;
  const bool max = m_window->isMaximized();
  m_maxBtn->setMaximized(max);
  m_maxBtn->setToolTip(max ? tr("Restore") : tr("Maximize"));
}

// ── Resize hit-testing ──────────────────────────────────────────────────────

Qt::Edges TitleBarController::edgesAt(const QPoint &gp) const {
  if (m_window->isMaximized() || m_window->isFullScreen())
    return {};
  const QRect r = m_window->frameGeometry(); // frameless => geometry
  const int m = m_cfg.resizeMarginPx;
  Qt::Edges e;
  if (gp.x() <= r.left() + m)
    e |= Qt::LeftEdge;
  else if (gp.x() >= r.right() - m)
    e |= Qt::RightEdge;
  if (gp.y() <= r.top() + m)
    e |= Qt::TopEdge;
  else if (gp.y() >= r.bottom() - m)
    e |= Qt::BottomEdge;
  return e;
}

void TitleBarController::updateResizeCursor(Qt::Edges edges) {
  Qt::CursorShape shape;
  if ((edges & Qt::LeftEdge && edges & Qt::TopEdge) ||
      (edges & Qt::RightEdge && edges & Qt::BottomEdge))
    shape = Qt::SizeFDiagCursor;
  else if ((edges & Qt::RightEdge && edges & Qt::TopEdge) ||
           (edges & Qt::LeftEdge && edges & Qt::BottomEdge))
    shape = Qt::SizeBDiagCursor;
  else if (edges & (Qt::LeftEdge | Qt::RightEdge))
    shape = Qt::SizeHorCursor;
  else
    shape = Qt::SizeVerCursor;

  if (m_hasOverrideCursor)
    QApplication::changeOverrideCursor(shape);
  else
    QApplication::setOverrideCursor(shape);
  m_hasOverrideCursor = true;
}

void TitleBarController::clearResizeCursor() {
  if (m_hasOverrideCursor) {
    QApplication::restoreOverrideCursor();
    m_hasOverrideCursor = false;
  }
}

// ── Event handling ──────────────────────────────────────────────────────────

bool TitleBarController::eventFilter(QObject *obj, QEvent *event) {
  const QEvent::Type t = event->type();

  // Keep the maximize button + cursor in sync with the window state.
  if (obj == m_window && t == QEvent::WindowStateChange) {
    syncMaximizeButton();
    clearResizeCursor();
    updateBorder();
    return false;
  }

  // Keep the edge overlay covering the whole window and on top of its content.
  if (m_border && obj == m_window &&
      (t == QEvent::Resize || t == QEvent::Show)) {
    updateBorder();
    return false;
  }

  // Drop any resize-cursor override when the pointer leaves the window or the
  // window loses focus (e.g. a dialog opens) — otherwise it can get stuck.
  if (obj == m_window &&
      (t == QEvent::WindowDeactivate || t == QEvent::Leave)) {
    clearResizeCursor();
    return false;
  }

  // Title strip: move-by-drag and double-click maximize. Ignore presses landing
  // on the window-control buttons.
  if (obj == m_strip &&
      (t == QEvent::MouseButtonPress || t == QEvent::MouseButtonDblClick)) {
    auto *me = static_cast<QMouseEvent *>(event);
    QWidget *child = m_strip->childAt(me->position().toPoint());
    const bool onButton = qobject_cast<QAbstractButton *>(child) != nullptr;
    if (me->button() == Qt::LeftButton && !onButton) {
      if (t == QEvent::MouseButtonDblClick) {
        toggleMaximize();
        return true;
      }
      if (QWindow *wh = m_window->windowHandle()) {
        wh->startSystemMove();
        return true;
      }
    }
    return false;
  }

  // Application-wide mouse tracking for edge resize. Scope to our window.
  if (t == QEvent::MouseMove || t == QEvent::MouseButtonPress) {
    auto *w = qobject_cast<QWidget *>(obj);
    // Event is for another window (e.g. a dialog) or ours isn't active: make
    // sure we don't leave a stuck resize cursor behind, then bow out so the
    // owning window's controller handles it.
    if (!w || w->window() != m_window || !m_window->isActiveWindow()) {
      clearResizeCursor();
      return false;
    }
    // Never hijack a button (window controls / toolbar actions) for resizing,
    // even when it sits inside the edge band.
    if (qobject_cast<QAbstractButton *>(w)) {
      clearResizeCursor();
      return false;
    }
    auto *me = static_cast<QMouseEvent *>(event);

    if (t == QEvent::MouseMove) {
      if (me->buttons() == Qt::NoButton) {
        const Qt::Edges edges = edgesAt(me->globalPosition().toPoint());
        if (edges)
          updateResizeCursor(edges);
        else
          clearResizeCursor();
      }
      return false;
    }

    // MouseButtonPress near an edge → begin a system resize.
    if (me->button() == Qt::LeftButton) {
      const Qt::Edges edges = edgesAt(me->globalPosition().toPoint());
      if (edges) {
        if (QWindow *wh = m_window->windowHandle()) {
          clearResizeCursor();
          wh->startSystemResize(edges);
          return true;
        }
      }
    }
  }

  return QObject::eventFilter(obj, event);
}
