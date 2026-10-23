# Decisions

Architecture decisions for glate, in the format used by omnidict: context, decision,
consequences. Newest last. A decision changes only through a new ADR that supersedes it.

## ADR-001: Linux dev builds use the KDE Qt snap SDK (2026-10-22)

**Context.** The snap is built with the `kde-neon-6` extension on core24, which brings Qt 6.11
from the KDE content snaps. Building on the host against whatever Qt the distribution ships
hides breakage that only shows up in the snap.

**Decision.** `scripts/dev-build.sh` builds against `kde-qt6-core24-sdk` and `scripts/dev-run.sh`
runs against `kf6-core24`, as in omnidict. `cmake/SnapSdkWorkaround.cmake` keeps the SDK's
generic `/usr/include` from shadowing the host libc headers. The scripts prefer the cmake snap
because the SDK's library path breaks the host cmake on rolling distributions (libarchive).
Windows has no CI build (owner, 2026-10-23); the code still builds there with the official Qt
installer, by hand.

**Consequences.** Developers need three snaps: `kde-qt6-core24-sdk`, `kf6-core24` and `cmake`.

The Flathub package (github.com/flathub/com.ktechpit.glate) builds from source on the KDE 6.11
runtime, like omnidict's: the glate-ng git source at a tag, plus Mozilla's engine and the
submodules it needs as pinned source archives, configured with `-DGLATE_ENGINE_PREFETCHED=ON`
because Flathub builds have no network. Those archives must move together with
`GLATE_ENGINE_COMMIT`; `tools/flathub/engine_sources.py` prints them for the current pin.
(Up to 4.2 the Flathub package repackaged the snap binary.) The local
`com.ktechpit.glate.yml` builds this source tree with network access instead, for testing.

## ADR-002: Coding standards apply to new code first (2026-10-22)

**Context.** omnidict's `.clang-format`, `.clang-tidy` and warning set are the standard. The
existing glate sources use a different style (2-space LLVM) and would need about 2,400 changed
lines to conform, which would bury real history under a formatting commit.

**Decision.** The standards are enforced, with warnings as errors in CI, on `tests/` and on
new modules starting with `modules/OfflineTranslation`. Legacy `src/` and the older modules
build with `glate_set_legacy_warnings()` and are not linted. A file moves under the strict set
when it is substantially rewritten.

**Consequences.** `make lint` and CI check only the strict set (`STRICT_DIRS` in the Makefile).

## ADR-003: Offline translation engine is Mozilla's bergamot inference (2026-10-22)

**Context.** Offline translation needs a local engine. Options compared:

- kotki + marian-lite (what LocalTranslate uses): Linux only, depends on OpenBLAS, protobuf
  and SQLite, and trails Mozilla's current model format. LocalTranslate itself is GPL-3.0,
  so none of its code can come into MIT glate.
- Mozilla's inference engine (`mozilla/translations`, `inference/`, formerly
  bergamot-translator): MPL-2.0, builds with GCC, Clang and MSVC, and is the engine the
  Firefox models are made for.
- An LLM: far larger downloads and slower on CPU for the same quality on short text.

**Decision.** Use Mozilla's inference engine, fetched at a pinned commit with only the
submodules it needs (`cmake/FetchTranslationsEngine.cmake`), built from source as a static
library. Float matrix products use ruy (`USE_RUY_SGEMM`) so no system BLAS is needed;
int8 products use intgemm on x86 and ruy on ARM. No model ships with the app.

**Consequences.** MPL-2.0 is file-level copyleft: the engine's files stay MPL and their
source stays available, which the pinned upstream commit satisfies. Build time grows by a few
minutes; the engine is behind `GLATE_OFFLINE_ENGINE` so it can be turned off.

## ADR-004: Translation models are mirrored on a rolling GitHub release (2026-10-22)

**Context.** Mozilla publishes the models through Firefox Remote Settings, an endpoint built
for Firefox that can change without notice. Models are 15 to 55 MB per direction (median
about 35 MB), English to and from about 60 languages, about 4 GB for every direction.

**Decision.** As in omnidict ADR-016, a manually started workflow
(`.github/workflows/models.yml`) copies a pinned list of Mozilla models into the rolling
`models` release of the public glate-ng repository together with a `catalog.json` (SHA-256
per file).
The app reads only that catalogue and verifies every file it downloads. Models install into
the app data directory, one folder per direction. Pairs without English pivot through English.

**Consequences.** glate controls which model versions users get. The workflow runs only when
the owner starts it.

## ADR-005: Offline translation is a Pro feature (2026-10-22)

**Decision.** Downloading models and translating offline both go through the existing
`LicenseService` Pro check. Online translation is unchanged.

## ADR-006: GPL-3.0-or-later with a source-available licensing module (2026-10-22)

**Context.** The repository becomes public. Glate was MIT up to 4.3.0. The owner's other
apps (omnidict, playlist-dl) use one model: the program under the GPL, the account and licence
code source-available under the Ktechpit Licensing Module License, joined by a GPL section 7
additional permission for official builds.

**Decision.** Adopt that model (`LICENSING.md`, `REUSE.toml`, `LICENSES/`). The module is
`modules/AccountAndLicense/`, `src/GlateLicenseConfig.h`, `src/licenseservice.*`,
`src/accountdialog.*` and `src/planscomparisiondialog.*`. Vendored third-party files keep their
licences (QHotkey BSD-3-Clause, QtWaitingSpinner MIT, Remix Icon Apache-2.0). Releases up to
4.3.0 stay MIT. `reuse lint` runs in `make lint` and CI.

**Consequences.** Mozilla's engine (MPL-2.0) and Qt (LGPL-3.0) are compatible with the GPL.
New files are covered by the GPL annotation in `REUSE.toml` unless they belong to the module,
whose paths must then be added to `REUSE.toml` and `LICENSING.md` together.

The repository was checked before it went public: no credentials, tokens or private keys in
the working tree or in any of its 167 commits; the licence server endpoints are public by
nature (they ship in every build), and history before 2026-03-19 was already public in
keshavbhatt/glate.

## ADR-007: Tag-based releases; main goes to edge, tags to stable (2026-10-22)

**Context.** Releases were cut by hand: the tag workflow built artifacts, and the snap was
uploaded separately. omnidict publishes from CI.

**Decision.** As in omnidict, `.github/workflows/snap.yml` builds the snap for amd64 and
arm64 on every push to `main` that touches the app and publishes it to `edge` with the
version `X.Y.Z+git<hash>`; a `vX.Y.Z` tag publishes `X.Y.Z` straight to `stable` (owner,
2026-10-22: edge is the testing channel, so a tag is the decision to ship; omnidict stops at
candidate). The tag must equal `project(VERSION)`, checked by both workflows.
The tag workflow (`release.yml`) makes the GitHub release (no Windows build since
2026-10-23, owner: CI minutes are limited);
the snap and the Flatpak no longer go there (owner, 2026-10-22: Flathub already publishes
the Flatpak). Flathub follows tags through the manifest's `x-checker-data`. Publishing is gated by the `SNAPCRAFT_STORE_CREDENTIALS` secret
and the `PUBLISH_TO_STORE` repository variable. The steps are in `.github/RELEASE.md`.

## ADR-008: App shell module, as in omnidict (2026-10-23)

**Context.** Glate had a popup slide show on first start, an About text box, no log, no way
to report a bug from the app, and nothing that showed when offline mode was on. omnidict
has a shell for all of this.

**Decision.** A strict, reusable `modules/AppShell` (namespace `glate::shell`) ports
omnidict's pieces: `LogSink` (Qt message handler, `<AppData>/logs/glate.log` rotated at 2 MB,
the last 1000 lines in memory), logging categories `glate.*`, diagnostics text, the About,
What's new, bug report and shortcuts windows, a first-run `WelcomeCard` shown in place in the
empty result pane (never a popup), and a `StatusPill` for the "Offline" indicator.
`src/glateshell.*` holds what is specific to Glate. Every screen had an approved HTML mock
first (`DOCS/mocks/`). Differences from omnidict:

- What's new reads the `<releases>` notes of the metainfo file (bundled as `:/meta/`), so the
  release notes are written once; `tst_app_shell` fails when the newest release does not
  match `project(VERSION)`.
- The bug report can include the last 200 log lines (ticked by default). The log never
  holds the text being translated or its translation: request URLs are logged without their
  query (`redactUrlQueries`), and the home directory is replaced with `~`.
- An existing install without a recorded version (no `shell/whatsNewSeenVersion`, but a
  `firstRun` key) counts as an update and sees What's new once.

## ADR-009: Global shortcut module; Wayland through the desktop portal, QHotkey removed (2026-10-23)

**Context.** Quick Translate used the vendored QHotkey: an X11 key grab, so on Wayland the
option was greyed out, and the selected text came from the `xclip` program, which the snap
does not ship. The owner asked for Wayland support as in Omnidict (ADR-019 there) and
Paraphrase.

**Decision.** A strict `modules/GlobalShortcut` (namespace `glate::shortcut`) replaces QHotkey:

- X11 (not Xwayland): the app grabs the keys with xcb, as QHotkey did. Windows:
  `RegisterHotKey`. The key is the one set in Settings (default Ctrl+Shift+Space).
- Wayland, and X11 through Xwayland: the xdg-desktop-portal `GlobalShortcuts` portal,
  through Qt DBus (adapted from Omnidict's client, on a D-Bus connection of its own that
  registers the app id first). The desktop owns the key: the Settings key is only suggested
  on the first bind, the field shows the key the desktop bound, and "Change..." opens the
  desktop's own dialog. KDE Plasma 5.27+, GNOME 48+ and Hyprland have the portal; it works
  from the snap (`desktop` plug) and the Flatpak with no extra permission.
- Without a portal (sway, GNOME 46 and 47) Settings explains a custom desktop shortcut
  running `glate --quick-translate`, which the running instance handles, with the launcher's
  activation token passed on.
- The text comes from Qt's clipboard: the selection (X11, Wayland), else the clipboard
  (Windows, or copy then shortcut). On Wayland it is read once the window is active, since
  the compositor only offers it to the focused window. `xclip` is no longer used.

**Consequences.** Qt DBus joins the Linux build (`THIRD_PARTY.md`). QHotkey (BSD-3-Clause)
leaves the tree, its licence entries and the About window. macOS has no global shortcut.
Tests run the portal flow against a fake portal on a private D-Bus daemon
(`tests/fake_portal.h`), never the session's; the real desktop dialogs are checked by hand.
