# Third-party components

No new third-party dependency is added without an ADR in `DOCS/DECISIONS.md` and a row here.

| Component | Version | Licence | How it is obtained | Used by |
|---|---|---|---|---|
| Qt | 6.11 (Linux snap), 6.10 (Windows) | LGPL-3.0 | `kde-qt6-core24-sdk` snap (dev build), `kde-neon-6` extension (snap), official installer (Windows CI) | app |
| Qt DBus | with Qt (Linux only) | LGPL-3.0 | as Qt | `modules/GlobalShortcut` (desktop portal, ADR-009) |
| QtWaitingSpinner | vendored as `src/waitingspinnerwidget.*` | MIT | copied into the tree | app (busy spinner) |
| Remix Icon | 2020 snapshot, 2.5.0 and 4.6.0 (`sparkling-line`), `src/icons/*-line.png`, `*-fill.png`, `text.png` | Apache-2.0 | copied into the tree | app (toolbar glyphs) |
| X11, xcb | as shipped by the SDK / core24 | MIT | SDK snap (build), staged packages (snap) | `modules/GlobalShortcut` (X11 key grab) |
| Mozilla translations inference engine | pinned commit in `cmake/FetchTranslationsEngine.cmake` | MPL-2.0 | fetched at configure time, built from source (ADR-003) | `modules/OfflineTranslation` |
| marian (Mozilla fork) | in the engine tree | MIT | fetched with the engine | engine |
| sentencepiece (browsermt fork) | engine submodule | Apache-2.0 | fetched with the engine | engine |
| intgemm | engine submodule | MIT | fetched with the engine | engine (x86 int8 products) |
| ruy, cpuinfo | engine submodule | Apache-2.0, BSD-2-Clause | fetched with the engine | engine (float products, ARM int8) |
| ssplit-cpp | engine submodule | Apache-2.0 (code) | fetched with the engine | engine (sentence splitting) |
| onnxjs sgemm, simd_utils | engine submodules | MIT, BSD-2-Clause | fetched with the engine | engine |
| PCRE2 | system library (core24 base, Flatpak runtime); bundled build on Windows | BSD-3-Clause | system library; downloaded by ssplit-cpp's CMake on Windows | ssplit-cpp |
| Firefox translation models | as listed in the `models` release catalogue | MPL-2.0 | downloaded by the user at runtime from the glate-ng `models` release (ADR-004) | offline translation |
