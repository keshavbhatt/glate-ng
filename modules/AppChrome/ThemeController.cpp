#include "ThemeController.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QPushButton>
#include <QSize>
#include <QStyle>
#include <QStyleHints>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>

namespace {
// Item delegate that drops the focus state before painting. Without it the
// Fusion base style draws a square focus rectangle around the current popup
// item — it ignores the QSS item border-radius, so it reads as an ugly square
// box on top of the rounded hover/selection pill.
class NoFocusDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;

protected:
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    QStyleOptionViewItem opt(option);
    opt.state &= ~QStyle::State_HasFocus;
    QStyledItemDelegate::paint(painter, opt, index);
  }

  // Enforce a compact-but-safe row height so popup item padding can stay tight
  // without clipping descenders (the reason the QSS used a big min-height).
  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override {
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    if (s.height() < 26)
      s.setHeight(26);
    return s;
  }
};

// Stylesheet painted directly on the combobox popup's view so the rounded card
// survives a transparent popup container (see eventFilter). Mirrors the Fusion 2
// sheet's combobox-popup colours.
QString popupViewQss(bool dark) {
  if (dark)
    return QStringLiteral(
        "QAbstractItemView, QListView {"
        "  background-color:#282828; border:1px solid #3e3e3e;"
        "  border-radius:10px; padding:6px; outline:0; font-size:13px; }"
        "QListView::item {"
        "  padding:2px 10px; margin:1px 4px;"
        "  border-radius:6px; color:#b3b3b3; background:transparent; }"
        "QListView::item:hover    { background-color:#3e3e3e; color:#ffffff; }"
        "QListView::item:selected { background-color:#454545; color:#ffffff; }");
  return QStringLiteral(
      "QAbstractItemView, QListView {"
      "  background-color:#ffffff; border:1px solid #dcdfe6;"
      "  border-radius:10px; padding:6px; outline:0; font-size:13px; }"
      "QListView::item {"
      "  padding:2px 10px; margin:1px 4px;"
      "  border-radius:6px; color:#5b6472; background:transparent; }"
      "QListView::item:hover    { background-color:#eceef1; color:#1a1d23; }"
      "QListView::item:selected { background-color:#e2e5ea; color:#1a1d23; }");
}
} // namespace

ThemeController::ThemeController(QApplication &app, AppTheme::Accents accents,
                                QObject *parent)
    : QObject(parent), m_app(app), m_accents(accents) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  // Track the OS light/dark preference. Only acts while mode == System; an
  // explicit override pins the look and ignores this.
  connect(m_app.styleHints(), &QStyleHints::colorSchemeChanged, this,
          [this](Qt::ColorScheme) {
            if (m_mode == AppTheme::Mode::System)
              reapply();
          });
#endif
  // Application-wide polish filter for the chrome fix-ups (see eventFilter).
  m_app.installEventFilter(this);
  reapply();
}

bool ThemeController::eventFilter(QObject *obj, QEvent *event) {
  const QEvent::Type type = event->type();

  if (type == QEvent::Polish) {
    if (auto *btn = qobject_cast<QPushButton *>(obj)) {
      // Tag icon-only push buttons so the skins can style them flat (the QSS
      // pill radius doesn't apply to a content-sized icon-only button).
      const bool iconOnly = btn->text().isEmpty() && !btn->icon().isNull();
      if (btn->property("iconOnly").toBool() != iconOnly) {
        btn->setProperty("iconOnly", iconOnly);
        if (btn->style()) {
          btn->style()->unpolish(btn);
          btn->style()->polish(btn);
        }
      }
    }
  }

  // Round the combobox popup (Fusion 2). The popup is a separate top-level
  // window (QComboBoxPrivateContainer). The square box behind the rounded card
  // comes from (a) the container's own QFrame border and (b) the blanket
  // `QWidget { background-color }` rule, which *explicitly* paints the container
  // — WA_TranslucentBackground can't beat a painted QSS background, so we make
  // the container transparent. But a transparent container also strips the
  // descendant view's background, so we paint the rounded card on the VIEW
  // itself with its own stylesheet (instead of relying on the cascaded rule).
  // Qt resets the frame shape on each show, so re-strip it on Show too.
  if (m_skin == AppTheme::Skin::Fusion2 &&
      (type == QEvent::Polish || type == QEvent::Show) &&
      obj->inherits("QComboBoxPrivateContainer")) {
    auto *popup = static_cast<QWidget *>(obj);
    if (type == QEvent::Polish) {
      popup->setWindowFlag(Qt::FramelessWindowHint, true);
      popup->setWindowFlag(Qt::NoDropShadowWindowHint, true);
      popup->setAttribute(Qt::WA_TranslucentBackground, true);
      popup->setAutoFillBackground(false);
      popup->setStyleSheet(
          QStringLiteral("background: transparent; border: none;"));
      if (auto *view = popup->findChild<QAbstractItemView *>()) {
        view->setFrameShape(QFrame::NoFrame);
        view->setStyleSheet(popupViewQss(m_effective == AppTheme::Mode::Dark));
        // Kill the square focus rectangle the base style draws on the current
        // item (it ignores the rounded item radius). Set once per view.
        if (!view->property("noFocusDelegate").toBool()) {
          view->setItemDelegate(new NoFocusDelegate(view));
          view->setProperty("noFocusDelegate", true);
        }
      }
    }
    if (auto *frame = qobject_cast<QFrame *>(popup)) {
      frame->setFrameShape(QFrame::NoFrame);
      frame->setLineWidth(0);
    }
  }

  return QObject::eventFilter(obj, event);
}

void ThemeController::setMode(AppTheme::Mode requested) {
  m_mode = requested;
  reapply();
}

void ThemeController::setSkin(AppTheme::Skin skin) {
  m_skin = skin;
  reapply();
}

void ThemeController::setAccent(const QColor &accent) {
  if (accent.isValid())
    m_accents.highlight = accent;
  reapply();
}

void ThemeController::configure(AppTheme::Skin skin, AppTheme::Mode mode,
                                const QColor &accent) {
  m_skin = skin;
  m_mode = mode;
  if (accent.isValid())
    m_accents.highlight = accent;
  reapply();
}

void ThemeController::reapply() {
  AppTheme::apply(m_app, m_skin, m_mode, m_accents);
  const AppTheme::Mode effective = AppTheme::resolveEffective(m_mode);
  if (effective != m_effective) {
    m_effective = effective;
    emit effectiveModeChanged(m_effective);
  }
}
