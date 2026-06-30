# AppChrome

Reusable application chrome for Qt 6 Widgets apps — **self-contained theming** and
**client-side decoration (CSD)** — packaged as a single static library (`app_chrome`).
No app-specific symbols and no bundled assets, so it drops into any project.

The two parts are independent: use either alone.

## Features

- **AppTheme / ThemeController** — self-contained theming over Qt's built-in **Fusion** style,
  so the app looks identical regardless of the host platform theme (Breeze/Adwaita/snap/flatpak).
  Follows the OS light/dark preference live (`QStyleHints::colorScheme`, Qt 6.5+), with a user
  override. Two selectable **skins**:
  - `Skin::Fusion` — our light/dark `QPalette` + a thin, palette-driven stylesheet; check
    boxes/radios are custom-drawn by a `QProxyStyle` (`FusionChromeStyle`).
  - `Skin::Fusion2` — a full, comprehensive stylesheet skin (pill buttons, rounded inputs,
    custom scrollbars/sliders/menus) bundled in `appchrome.qrc` (`themes/fusion2_<mode>.qss` +
    chevron icons), with a matching palette.

  A user **accent colour** (`Accents::highlight`) is applied to both skins (in Fusion 2 via an
  appended override stylesheet).
- **WindowChrome** — frameless window with its own title bar strip mounted at the very top
  (via `QMainWindow::setMenuWidget`, so it sits above any toolbar): app icon + title on the
  left, minimize/maximize/close on the right, plus drag-to-move, edge/corner resize and
  double-click-to-maximize. The window controls are painted from the active palette (no bundled
  images), so they follow light/dark themes. Uses `QWindow::startSystemMove` /
  `startSystemResize`, so it works on **both X11 and Wayland**.

## Dependencies

| Component | Minimum |
|-----------|---------|
| Qt6::Core / Gui / Widgets | **6.5** (for `QStyleHints::colorScheme`) |
| CMake | 3.21 |

## Integration

```cmake
add_subdirectory(modules/AppChrome)
target_link_libraries(my_app PRIVATE app_chrome)
```

The library exposes its headers via a PUBLIC include dir, so `#include <AppTheme.h>` etc.
work directly.

## Usage

### Theming (follow system, with override)

```cpp
#include <ThemeController.h>

// In main(), before creating the main window:
auto *theme = new ThemeController(app);
theme->configure(AppTheme::skinFromString(savedSkin),   // "fusion" | "fusion2"
                 AppTheme::modeFromString(savedMode),   // "system" | "light" | "dark"
                 QColor(savedAccentHex));                // e.g. "#3574f0"

// Later, from a settings dialog (live, no restart):
theme->setSkin(AppTheme::Skin::Fusion2);
theme->setMode(AppTheme::Mode::Dark);
theme->setAccent(QColor("#1db954"));
```

`ThemeController` re-applies automatically when the OS scheme changes **while** the mode is
`System`; an explicit Light/Dark override pins the look. It emits `effectiveModeChanged` so the
app can refresh any widgets that bake palette colours at construction time.

### Client-side decoration (opt-in)

```cpp
#include <WindowChrome.h>

// In the QMainWindow constructor, BEFORE show():
WindowChrome::Config cfg;
cfg.title = QApplication::applicationName();
cfg.icon  = QApplication::windowIcon();
WindowChrome::install(this, cfg);          // frameless + own title bar strip

// To decorate every plain dialog the app opens (close + maximize, no minimize):
WindowChrome::Config dlg;
dlg.showMinimize = false;
WindowChrome::enableAutoForDialogs(*qApp, dlg);
```

Call `install()` only when CSD is enabled. It sets `Qt::FramelessWindowHint`, so it must run
before the native window exists (i.e. before `show()`); changing it back to native decorations
takes a restart. On a `QMainWindow` the strip is mounted via `setMenuWidget` (above any
toolbar); on any other widget its existing layout is wrapped beneath the strip. When CSD is
off, simply don't call it — the native title bar is used.

## Notes & limitations

- The bundled stylesheet is intentionally **minimal**: the look comes from Fusion + the
  palette, not from QSS. Avoid partially restyling complex widgets (`QComboBox`, `QSpinBox`,
  `QLineEdit`, …) via QSS — doing so makes Qt stop drawing their sub-controls (dropdown arrows,
  spin buttons) and yields broken borders. Style via the palette instead. Check boxes and radio
  buttons are an exception that QSS *also* breaks (the tick disappears), so `forceFusionStyle()`
  wraps Fusion in a small `QProxyStyle` (`FusionChromeStyle`) that custom-draws those two
  indicators with a clear palette-driven border — everything else stays Fusion.
- A `QPalette` swap updates most widgets live. Widgets that **bake** palette colours into a
  per-widget stylesheet string at construction won't update until rebuilt — re-read the palette
  on `QEvent::PaletteChange`, or use `palette(...)` references inside the stylesheet (which
  resolve live).
- Inside Flatpak/Snap, OS scheme detection relies on the `xdg-desktop-portal`
  `org.freedesktop.appearance` key; without it, `System` falls back to a window-lightness
  heuristic.
