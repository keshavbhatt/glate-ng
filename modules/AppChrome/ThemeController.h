#ifndef APPCHROME_THEMECONTROLLER_H
#define APPCHROME_THEMECONTROLLER_H

#include <QColor>
#include <QObject>

#include "AppTheme.h"

class QApplication;

// ── ThemeController ───────────────────────────────────────────────────────────
//
// Live theme brain. Applies AppTheme to the application and, while the requested
// mode is System, re-applies automatically when the OS light/dark preference
// changes (QStyleHints::colorSchemeChanged, Qt 6.5+). A user override
// (Light/Dark) pins the look and ignores OS changes. Reusable; holds no
// app-specific settings — the consumer drives it via setMode().
class ThemeController : public QObject {
  Q_OBJECT
public:
  explicit ThemeController(QApplication &app, AppTheme::Accents accents = {},
                           QObject *parent = nullptr);

  // Requested mode (may be System); persists nothing — that's the caller's job.
  AppTheme::Mode mode() const { return m_mode; }
  // Concrete Light/Dark currently in effect.
  AppTheme::Mode effectiveMode() const { return m_effective; }
  AppTheme::Skin skin() const { return m_skin; }
  QColor accent() const { return m_accents.highlight; }

  // Set the user-requested mode and re-apply immediately.
  void setMode(AppTheme::Mode requested);
  // Set the skin (Fusion / Fusion 2) and re-apply.
  void setSkin(AppTheme::Skin skin);
  // Set the accent colour and re-apply.
  void setAccent(const QColor &accent);
  // Set everything at once and re-apply only once (use at startup).
  void configure(AppTheme::Skin skin, AppTheme::Mode mode, const QColor &accent);

signals:
  // Emitted whenever the effective (resolved) mode changes — either from a
  // setMode() override or a tracked OS scheme change. Lets the app refresh
  // widgets that bake palette colours at construction.
  void effectiveModeChanged(AppTheme::Mode effective);

protected:
  // Application-wide filter that, on widget polish: (1) tags icon-only
  // QPushButtons with iconOnly=true so the skins can style them flat+rounded
  // instead of a hard square box, and (2) under the Fusion 2 stylesheet skin,
  // makes QComboBox popups frameless + translucent so the popup's container
  // window no longer paints a square box behind the rounded list card.
  bool eventFilter(QObject *obj, QEvent *event) override;

private:
  void reapply();

  QApplication &m_app;
  AppTheme::Accents m_accents;
  AppTheme::Skin m_skin = AppTheme::Skin::Fusion;
  AppTheme::Mode m_mode = AppTheme::Mode::System;
  AppTheme::Mode m_effective = AppTheme::Mode::Light;
};

#endif // APPCHROME_THEMECONTROLLER_H
