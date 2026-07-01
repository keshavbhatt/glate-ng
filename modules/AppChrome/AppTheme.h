#ifndef APPCHROME_APPTHEME_H
#define APPCHROME_APPTHEME_H

#include <QColor>
#include <QPalette>
#include <QString>

class QApplication;

// ── AppTheme ──────────────────────────────────────────────────────────────────
//
// Self-contained application theming for Qt Widgets apps. Forces the built-in
// Fusion style and applies our own light/dark palette + a thin, palette-driven
// stylesheet, so the app looks identical regardless of the host platform theme
// (Breeze/Adwaita/snap themes). No app-specific symbols, no bundled assets —
// reusable across projects. See ThemeController for live OS-scheme tracking.
namespace AppTheme {

enum class Mode { System, Light, Dark };

// Selectable look:
//  - Fusion : Qt's Fusion + our palette + a minimal stylesheet (checkboxes drawn
//             by FusionChromeStyle). Clean and native-feeling. The default.
//  - Fusion2: a full, comprehensive stylesheet skin (pill buttons, rounded
//             inputs, custom scrollbars/sliders/menus) over Fusion, with a
//             matching palette. More distinctive/branded.
enum class Skin { Fusion, Fusion2 };

// Optional consumer overrides; defaults give a neutral, modern look. `highlight`
// is the user-selectable accent colour, applied to both skins.
struct Accents {
  QColor highlight = QColor(0xff, 0x7a, 0x1a); // selection / accent colour
  int radiusPx = 6;                            // control corner radius
  int spacingPx = 6;                           // toolbar / layout spacing
};

// Force Qt's built-in Fusion style on the application (wrapped in the checkbox
// proxy). Call once, early.
void forceFusionStyle();

// Full-coverage Fusion-friendly palettes (incl. Disabled group + PlaceholderText).
QPalette lightPalette(const Accents &accents = {});
QPalette darkPalette(const Accents &accents = {});

// Thin, palette-driven stylesheet for the Fusion skin. Also styles the CSD
// title-bar object names used by WindowChrome/TitleBarController.
QString styleSheet(const Accents &accents = {});

// The line / divider colour used for separators throughout the skin (the same
// value the Fusion 2 stylesheet uses for borders and dividers). Single source of
// truth so the CSD window edge (TitleBarController) stays in sync with the
// in-window separators.
QColor separatorColor(bool dark);

// The colour for a frameless window's outer edge hairline (CSD). A slightly
// stronger variant of separatorColor so the window boundary reads clearly
// against a neighbouring window without becoming a heavy frame.
QColor windowBorderColor(bool dark);

// Resolve a requested mode to a concrete Light/Dark. System is resolved from
// QStyleHints::colorScheme() (Qt 6.5+), falling back to a window-lightness
// heuristic on older Qt.
Mode resolveEffective(Mode requested);

// Apply skin + mode + accent to the application. System is resolved first. Safe
// to call repeatedly for live re-theming.
void apply(QApplication &app, Skin skin, Mode requested,
           const Accents &accents = {});

// String <-> enum helpers (for persisting in settings).
Mode modeFromString(const QString &s);
QString modeToString(Mode mode);
Skin skinFromString(const QString &s);
QString skinToString(Skin skin);

} // namespace AppTheme

#endif // APPCHROME_APPTHEME_H
