#include "AppTheme.h"

#include <QApplication>
#include <QFile>
#include <QPainter>
#include <QPainterPath>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QStyleOption>

// Register the bundled themes/icons (appchrome.qrc). In a STATIC library the
// resource initialiser must be pulled in explicitly. Defined in the global
// namespace as Q_INIT_RESOURCE requires.
static void appchromeInitResources() {
  static bool done = false;
  if (!done) {
    Q_INIT_RESOURCE(appchrome);
    done = true;
  }
}

namespace {

// Proxy over Fusion that custom-draws check boxes and radio buttons so they have
// a clear, palette-driven border (Fusion's is too faint on a near-white window)
// and a crisp tick/dot, identically in light and dark. Everything else is left
// to Fusion. Asset-free — no QSS on the indicators (which would break the tick).
class FusionChromeStyle : public QProxyStyle {
public:
  // switchCheckboxes: draw check boxes as iOS-style toggle switches (used by the
  // Fusion 2 skin to match its switch look) instead of a bordered box.
  explicit FusionChromeStyle(QStyle *base, bool switchCheckboxes = false)
      : QProxyStyle(base), m_switch(switchCheckboxes) {}

  int pixelMetric(PixelMetric metric, const QStyleOption *opt,
                  const QWidget *w) const override {
    if (m_switch && metric == PM_IndicatorWidth)
      return 38;
    if (m_switch && metric == PM_IndicatorHeight)
      return 20;
    // NOTE: we used to override PM_SubMenuOverlap here (Fusion 2) to keep a
    // child menu from covering the parent's rounded right border. A positive
    // overlap broke submenu opening on GNOME/Mutter Wayland (the xdg-popup is
    // positioned with a gap from its parent and never appears), so we leave the
    // metric at the style default — a few px of cosmetic overlap is well worth
    // submenus that actually open.
    return QProxyStyle::pixelMetric(metric, opt, w);
  }

  void drawPrimitive(PrimitiveElement pe, const QStyleOption *opt,
                     QPainter *p, const QWidget *w) const override {
    if (pe == PE_IndicatorCheckBox) {
      if (m_switch)
        drawSwitch(opt, p);
      else
        drawIndicator(false, opt, p);
      return;
    }
    if (pe == PE_IndicatorRadioButton) {
      drawIndicator(true, opt, p);
      return;
    }
    QProxyStyle::drawPrimitive(pe, opt, p, w);
  }

private:
  bool m_switch = false;

  // iOS-style toggle: rounded track (accent when on, muted when off) + white knob.
  static void drawSwitch(const QStyleOption *opt, QPainter *p) {
    const QPalette &pal = opt->palette;
    const bool enabled = opt->state & State_Enabled;
    const bool on = opt->state & State_On;

    QRectF track = QRectF(opt->rect).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal r = track.height() / 2.0;
    QColor trackColor = on ? pal.color(QPalette::Highlight)
                           : pal.color(QPalette::Mid);
    if (!enabled)
      trackColor = pal.color(QPalette::Mid);

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(Qt::NoPen);
    p->setBrush(trackColor);
    p->drawRoundedRect(track, r, r);

    const qreal d = track.height() - 4.0;
    const qreal x = on ? track.right() - d - 2.0 : track.left() + 2.0;
    QRectF knob(x, track.top() + 2.0, d, d);
    p->setBrush(QColor(Qt::white));
    p->drawEllipse(knob);
    p->restore();
  }

  static void drawIndicator(bool radio, const QStyleOption *opt, QPainter *p) {
    const QPalette &pal = opt->palette;
    const bool enabled = opt->state & State_Enabled;
    const bool on = opt->state & State_On;
    const bool tri = opt->state & State_NoChange;
    const bool hover = (opt->state & State_MouseOver) && enabled;

    QRectF box = QRectF(opt->rect).adjusted(1.0, 1.0, -1.0, -1.0);
    const qreal radius = radio ? box.width() / 2.0 : 3.0;

    QColor border = enabled ? pal.color(QPalette::Dark) : pal.color(QPalette::Mid);
    if (hover)
      border = pal.color(QPalette::Highlight);
    QColor fill = (on || tri) ? pal.color(QPalette::Highlight)
                              : pal.color(QPalette::Base);
    if (!enabled)
      fill = pal.color(QPalette::Window);

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(QPen(border, 1.2));
    p->setBrush(fill);
    if (radio)
      p->drawEllipse(box);
    else {
      QPainterPath path;
      path.addRoundedRect(box, radius, radius);
      p->drawPath(path);
    }

    const QColor mark = pal.color(QPalette::HighlightedText);
    if (radio) {
      if (on) {
        p->setPen(Qt::NoPen);
        p->setBrush(mark);
        const qreal d = box.width() * 0.42;
        QRectF dot(0, 0, d, d);
        dot.moveCenter(box.center());
        p->drawEllipse(dot);
      }
    } else if (tri) {
      QPen pen(mark, 1.6);
      pen.setCapStyle(Qt::RoundCap);
      p->setPen(pen);
      p->drawLine(QPointF(box.left() + box.width() * 0.27, box.center().y()),
                  QPointF(box.right() - box.width() * 0.27, box.center().y()));
    } else if (on) {
      QPen pen(mark, 1.7);
      pen.setCapStyle(Qt::RoundCap);
      pen.setJoinStyle(Qt::RoundJoin);
      p->setPen(pen);
      p->setBrush(Qt::NoBrush);
      QPainterPath chk;
      chk.moveTo(box.left() + box.width() * 0.24, box.top() + box.height() * 0.52);
      chk.lineTo(box.left() + box.width() * 0.43, box.top() + box.height() * 0.70);
      chk.lineTo(box.left() + box.width() * 0.77, box.top() + box.height() * 0.30);
      p->drawPath(chk);
    }
    p->restore();
  }
};

} // namespace

namespace AppTheme {

void forceFusionStyle() {
  // Fusion is built into Qt and renders identically everywhere — the core of
  // our platform-theme independence (snap/flatpak can't override it). Wrapped in
  // FusionChromeStyle so check boxes / radios get clear, palette-driven borders.
  if (QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion")))
    qApp->setStyle(new FusionChromeStyle(fusion));
}

QPalette lightPalette(const Accents &accents) {
  QPalette p;
  const QColor window(0xf4, 0xf5, 0xf7);
  const QColor base(0xff, 0xff, 0xff);
  const QColor alt(0xec, 0xee, 0xf1);
  const QColor text(0x1c, 0x1e, 0x21);
  const QColor button(0xe6, 0xe8, 0xeb);
  const QColor disabledText(0x9a, 0x9d, 0xa3);

  p.setColor(QPalette::Window, window);
  p.setColor(QPalette::WindowText, text);
  p.setColor(QPalette::Base, base);
  p.setColor(QPalette::AlternateBase, alt);
  p.setColor(QPalette::ToolTipBase, QColor(0x2b, 0x2e, 0x33));
  p.setColor(QPalette::ToolTipText, QColor(0xf4, 0xf5, 0xf7));
  p.setColor(QPalette::PlaceholderText, disabledText);
  p.setColor(QPalette::Text, text);
  p.setColor(QPalette::Button, button);
  p.setColor(QPalette::ButtonText, text);
  p.setColor(QPalette::BrightText, Qt::red);
  p.setColor(QPalette::Light, QColor(0xff, 0xff, 0xff));
  p.setColor(QPalette::Midlight, QColor(0xe0, 0xe2, 0xe6));
  p.setColor(QPalette::Mid, QColor(0xc4, 0xc7, 0xcc));
  p.setColor(QPalette::Dark, QColor(0x9a, 0x9d, 0xa3));
  p.setColor(QPalette::Shadow, QColor(0x70, 0x73, 0x78));
  p.setColor(QPalette::Highlight, accents.highlight);
  p.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
  p.setColor(QPalette::Link, accents.highlight);
  p.setColor(QPalette::LinkVisited, accents.highlight.darker(120));

  p.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
  p.setColor(QPalette::Disabled, QPalette::Text, disabledText);
  p.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
  p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0xc4, 0xc7, 0xcc));
  p.setColor(QPalette::Disabled, QPalette::HighlightedText, disabledText);
  return p;
}

QPalette darkPalette(const Accents &accents) {
  QPalette p;
  const QColor window(0x23, 0x25, 0x29);
  const QColor base(0x1b, 0x1d, 0x20);
  const QColor alt(0x2a, 0x2d, 0x32);
  const QColor text(0xe6, 0xe8, 0xeb);
  const QColor button(0x32, 0x35, 0x3b);
  const QColor disabledText(0x6c, 0x70, 0x77);

  p.setColor(QPalette::Window, window);
  p.setColor(QPalette::WindowText, text);
  p.setColor(QPalette::Base, base);
  p.setColor(QPalette::AlternateBase, alt);
  p.setColor(QPalette::ToolTipBase, QColor(0x32, 0x35, 0x3b));
  p.setColor(QPalette::ToolTipText, text);
  p.setColor(QPalette::PlaceholderText, disabledText);
  p.setColor(QPalette::Text, text);
  p.setColor(QPalette::Button, button);
  p.setColor(QPalette::ButtonText, text);
  p.setColor(QPalette::BrightText, QColor(0xff, 0x6b, 0x6b));
  p.setColor(QPalette::Light, QColor(0x3b, 0x3e, 0x45));
  p.setColor(QPalette::Midlight, QColor(0x30, 0x33, 0x39));
  p.setColor(QPalette::Mid, QColor(0x3f, 0x43, 0x4a));
  p.setColor(QPalette::Dark, QColor(0x16, 0x17, 0x1a));
  p.setColor(QPalette::Shadow, QColor(0x0d, 0x0e, 0x10));
  p.setColor(QPalette::Highlight, accents.highlight);
  p.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
  p.setColor(QPalette::Link, accents.highlight.lighter(120));
  p.setColor(QPalette::LinkVisited, accents.highlight);

  p.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
  p.setColor(QPalette::Disabled, QPalette::Text, disabledText);
  p.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
  p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0x3f, 0x43, 0x4a));
  p.setColor(QPalette::Disabled, QPalette::HighlightedText, disabledText);
  return p;
}

QString styleSheet(const Accents &accents) {
  const int r = accents.radiusPx;
  const int s = accents.spacingPx;
  // Palette-driven: palette(...) resolves live against the active palette, so a
  // single sheet serves both light and dark. Kept thin on purpose — Fusion
  // already does the heavy lifting; this only tunes spacing, radius and the
  // CSD title-bar widgets.
  //
  // Deliberately MINIMAL. The professional look comes from Fusion + the palette,
  // not from a heavy stylesheet. Partially restyling complex widgets via QSS
  // (QComboBox/QSpinBox/QLineEdit/QPushButton borders, etc.) makes Qt stop
  // drawing their sub-controls (dropdown arrows, spin buttons) and produces the
  // "broken borders / missing arrows" artefacts — so we don't. We only touch
  // things that have no sub-controls to break: flat toolbar chrome (with a
  // bottom separator), the tab pane border (consistent in both modes) and the
  // CSD title strip. Check boxes / radios are handled by FusionChromeStyle (a
  // QProxyStyle), NOT QSS, so their ticks/dots keep rendering.
  Q_UNUSED(r);
  Q_UNUSED(s);
  return QString::fromLatin1(R"(
    QToolBar { border: none; border-bottom: 1px solid palette(mid); }
    QToolBar::separator { background: palette(mid); width: 1px; margin: 5px 6px; }

    QComboBox QAbstractItemView { max-height: 320px; }

    /* Tabs: consistent palette-driven pane border in both light and dark. */
    QTabWidget::pane { border: 1px solid palette(mid); top: -1px; }

    /* Rule lines (QFrame HLine/VLine): palette-driven so they read correctly in
       both light and dark (Fusion's native etched line looks white on dark). */
    QFrame[frameShape="4"] { background: palette(mid); border: none; max-height: 1px; }
    QFrame[frameShape="5"] { background: palette(mid); border: none; max-width: 1px; }

    /* Menu-button chevron + separated preset-selector panel. */
    QPushButton::menu-indicator {
      image: url(:/appchrome/icons/chevron-down.svg);
      subcontrol-origin: padding; subcontrol-position: center right;
      width: 10px; height: 10px; right: 8px;
    }
    #presetStackedWidget {
      background: palette(base); border: 1px solid palette(mid); border-radius: 6px;
    }

    /* Compact, flat tooltip (no radius — a rounded top-level tooltip would show
       square corners behind the border). */
    QToolTip {
      background: palette(base); color: palette(text);
      border: 1px solid palette(mid); border-radius: 0px; padding: 3px 7px;
    }

    /* ── Client-side decoration title bar strip (WindowChrome) ── */
    /* No bottom border: the strip merges seamlessly into the window below it. */
    #AppChromeTitleBar { background: palette(window); }
    #AppChromeTitleLabel { font-weight: 600; }
  )");
}

Mode resolveEffective(Mode requested) {
  if (requested != Mode::System)
    return requested;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  if (qApp) {
    const Qt::ColorScheme scheme = qApp->styleHints()->colorScheme();
    if (scheme == Qt::ColorScheme::Dark)
      return Mode::Dark;
    if (scheme == Qt::ColorScheme::Light)
      return Mode::Light;
  }
#endif
  // Fallback heuristic: judge by the current window colour lightness.
  if (qApp) {
    const QColor win = qApp->palette().color(QPalette::Window);
    if (win.lightness() < 128)
      return Mode::Dark;
  }
  return Mode::Light;
}

namespace {

// Palettes aligned to the Fusion 2 stylesheet's hard-coded colours, so widgets
// that read QPalette (selection tints, dark-detection, palette(base) previews)
// stay cohesive with the skin.
QPalette fusion2DarkPalette(const QColor &accent) {
  QPalette p;
  const QColor window(0x12, 0x12, 0x12);
  const QColor base(0x2a, 0x2a, 0x2a);
  const QColor text(0xf5, 0xf5, 0xf5);
  const QColor disabled(0x53, 0x53, 0x53);
  p.setColor(QPalette::Window, window);
  p.setColor(QPalette::WindowText, text);
  p.setColor(QPalette::Base, base);
  p.setColor(QPalette::AlternateBase, QColor(0x18, 0x18, 0x18));
  p.setColor(QPalette::ToolTipBase, QColor(0x28, 0x28, 0x28));
  p.setColor(QPalette::ToolTipText, text);
  p.setColor(QPalette::PlaceholderText, QColor(0x6a, 0x6a, 0x6a));
  p.setColor(QPalette::Text, text);
  p.setColor(QPalette::Button, QColor(0x26, 0x26, 0x2c));
  p.setColor(QPalette::ButtonText, text);
  p.setColor(QPalette::Midlight, QColor(0x2e, 0x2e, 0x34)); // control hover
  p.setColor(QPalette::Mid, QColor(0x3e, 0x3e, 0x3e));
  p.setColor(QPalette::Dark, QColor(0x28, 0x28, 0x28));
  p.setColor(QPalette::Highlight, accent);
  p.setColor(QPalette::HighlightedText,
             accent.lightnessF() >= 0.62 ? QColor(Qt::black) : QColor(Qt::white));
  p.setColor(QPalette::Link, accent);
  p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
  p.setColor(QPalette::Disabled, QPalette::Text, disabled);
  p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
  return p;
}

QPalette fusion2LightPalette(const QColor &accent) {
  QPalette p;
  const QColor window(0xf5, 0xf6, 0xf8);
  const QColor base(0xff, 0xff, 0xff);
  const QColor text(0x1a, 0x1d, 0x23);
  const QColor disabled(0x8a, 0x92, 0xa0);
  p.setColor(QPalette::Window, window);
  p.setColor(QPalette::WindowText, text);
  p.setColor(QPalette::Base, base);
  p.setColor(QPalette::AlternateBase, QColor(0xf1, 0xf2, 0xf5));
  p.setColor(QPalette::ToolTipBase, QColor(0x1a, 0x1d, 0x23));
  p.setColor(QPalette::ToolTipText, QColor(0xff, 0xff, 0xff));
  p.setColor(QPalette::PlaceholderText, QColor(0x8a, 0x92, 0xa0));
  p.setColor(QPalette::Text, text);
  p.setColor(QPalette::Button, QColor(0xe9, 0xeb, 0xef));
  p.setColor(QPalette::ButtonText, text);
  p.setColor(QPalette::Midlight, QColor(0xe4, 0xe7, 0xec)); // control hover
  p.setColor(QPalette::Mid, QColor(0xd2, 0xd6, 0xdc));
  p.setColor(QPalette::Dark, QColor(0xe3, 0xe6, 0xea));
  p.setColor(QPalette::Highlight, accent);
  p.setColor(QPalette::HighlightedText,
             accent.lightnessF() >= 0.62 ? QColor(Qt::black) : QColor(Qt::white));
  p.setColor(QPalette::Link, accent);
  p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
  p.setColor(QPalette::Disabled, QPalette::Text, disabled);
  p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
  return p;
}

// Re-skins the accent-dependent parts of the Fusion 2 sheet (which ships a
// monochrome accent) with the user's chosen colour. Appended after the sheet.
QString accentOverrideQss(const QColor &accent) {
  const bool light = accent.lightnessF() >= 0.62;
  const QString on = light ? QStringLiteral("#000000") : QStringLiteral("#ffffff");
  const QString hover =
      (light ? accent.darker(108) : accent.lighter(112)).name();
  const QString pressed =
      (light ? accent.darker(118) : accent.darker(112)).name();
  const QString a = accent.name();
  return QString::fromLatin1(R"(
    QPushButton:checked { background-color: %1; color: %2; }
    QPushButton:checked:hover { background-color: %3; color: %2; }
    QPushButton[accent="true"] { background-color: %1; color: %2; }
    QPushButton[accent="true"]:hover { background-color: %3; color: %2; }
    QPushButton[accent="true"]:pressed { background-color: %4; color: %2; }
    QComboBox QAbstractItemView::item:selected { background-color: %1; color: %2; }
    QTabBar::tab:selected { border-bottom: 2px solid %1; }
    QProgressBar::chunk { background-color: %1; }
    QToolButton:checked { color: %1; }
    QSlider::sub-page:horizontal { background: %1; }
    QLineEdit { selection-background-color: %1; selection-color: %2; }
    QTextEdit:focus, QPlainTextEdit:focus { border: 1px solid palette(mid); }
  )")
      .arg(a, on, hover, pressed);
}

QString readResource(const QString &path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    return QString();
  return QString::fromUtf8(f.readAll());
}

} // namespace

void apply(QApplication &app, Skin skin, Mode requested, const Accents &accents) {
  appchromeInitResources();
  const Mode effective = resolveEffective(requested);

  if (skin == Skin::Fusion2) {
    // Fusion base wrapped in the proxy with switch-style check boxes (the sheet
    // leaves check boxes unstyled so the proxy draws them), a palette matched to
    // the sheet, then the sheet + accent override.
    if (QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion")))
      app.setStyle(new FusionChromeStyle(fusion, /*switchCheckboxes=*/true));
    app.setPalette(effective == Mode::Dark ? fusion2DarkPalette(accents.highlight)
                                           : fusion2LightPalette(accents.highlight));
    QString qss = readResource(
        effective == Mode::Dark
            ? QStringLiteral(":/appchrome/themes/fusion2_dark.qss")
            : QStringLiteral(":/appchrome/themes/fusion2_light.qss"));
    qss += QLatin1Char('\n');
    qss += accentOverrideQss(accents.highlight);
    app.setStyleSheet(qss);
    return;
  }

  // Fusion (native) skin.
  forceFusionStyle();
  app.setPalette(effective == Mode::Dark ? darkPalette(accents)
                                         : lightPalette(accents));
  app.setStyleSheet(styleSheet(accents));
}

Mode modeFromString(const QString &s) {
  const QString v = s.trimmed().toLower();
  if (v == QLatin1String("light"))
    return Mode::Light;
  if (v == QLatin1String("dark"))
    return Mode::Dark;
  return Mode::System;
}

QString modeToString(Mode mode) {
  switch (mode) {
  case Mode::Light:
    return QStringLiteral("light");
  case Mode::Dark:
    return QStringLiteral("dark");
  case Mode::System:
    break;
  }
  return QStringLiteral("system");
}

Skin skinFromString(const QString &s) {
  return s.trimmed().toLower() == QLatin1String("fusion2") ? Skin::Fusion2
                                                           : Skin::Fusion;
}

QString skinToString(Skin skin) {
  return skin == Skin::Fusion2 ? QStringLiteral("fusion2")
                               : QStringLiteral("fusion");
}

QColor separatorColor(bool dark) {
  // Mirrors the "border / divider" value in the Fusion 2 stylesheets
  // (resources/themes/fusion2_{dark,light}.qss). Keep these in sync if the skin's
  // divider colour changes.
  return dark ? QColor(0x28, 0x28, 0x28) : QColor(0xe3, 0xe6, 0xea);
}

} // namespace AppTheme
