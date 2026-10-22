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
Windows keeps its own CI build with the official Qt installer.

**Consequences.** Developers need three snaps: `kde-qt6-core24-sdk`, `kf6-core24` and `cmake`.

The Flathub package (github.com/flathub/com.ktechpit.glate) does not build from source: it
unpacks the published snap and installs its binary on the KDE runtime. So everything in the
snap binary, the offline engine included (statically linked, plus the runtime's own
libpcre2-8), reaches Flathub without changes to its build. A binary built against Qt 6.11
needs the `Qt_6.11` symbol version (`qt_version_tag`), so the Flathub manifest's
`runtime-version` must be at least the Qt of the snap it repackages: 6.11 from the first snap
built with this SDK. The local `com.ktechpit.glate.yml` builds from source for testing and
uses the same runtime.

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
