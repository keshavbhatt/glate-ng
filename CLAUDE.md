# Glate: project instructions

Glate is a Qt 6 / C++ desktop translator (online through Google, offline through Mozilla's
Firefox translation models). CMake root is the repository root. `src/` is the app,
`modules/` holds reusable libraries (AccountAndLicense, AppChrome, OfflineTranslation).
Decisions live in `DOCS/DECISIONS.md`, third-party components in `THIRD_PARTY.md`.
The build tooling follows the owner's omnidict project.

## Build and test

```sh
make build     # scripts/dev-build.sh against the kde-qt6-core24-sdk snap, into build-sdk/
make test      # build, then ctest
make lint      # clang-format, clang-tidy on the strict set, shellcheck, reuse lint
scripts/dev-run.sh                      # run against the kf6-core24 runtime snap
python3 tools/models/models.py install third_party/test-models en-de de-en   # models for tst_offline_translator
python3 -m unittest discover -s tools/models
make screenshots   # store screenshots: GLATE_DEBUG_* hooks (MainWindow::debugRun) + scripts/screenshots.py
```

Headless checks use the `GLATE_DEBUG_*` hooks documented above `MainWindow::debugRun()`, with a
scratch profile (`XDG_CONFIG_HOME`/`XDG_DATA_HOME`), never the owner's own data.

Needs the snaps `kde-qt6-core24-sdk`, `kf6-core24` and `cmake`. The first configure fetches
Mozilla's engine into `third_party/translations` (ADR-003).

## Standing rules

- No em dashes or en dashes anywhere.
- New code (`modules/OfflineTranslation`, `tests/`) follows `.clang-format`, `.clang-tidy` and
  `glate_set_warnings()` with warnings as errors; legacy `src/` is not reformatted (ADR-002).
- No new third-party dependency without an ADR and a `THIRD_PARTY.md` row.
- Licensing (ADR-006, `LICENSING.md`): GPL-3.0-or-later, except the licensing module under the
  Ktechpit Licensing Module License. New module files go into `REUSE.toml` and `LICENSING.md`
  and carry its SPDX header. `reuse lint` must pass.
- The repository is public: never commit credentials, tokens, customer data or local paths.
- No translation model ships with the app; models are downloaded on demand (ADR-004).
- `.github/workflows/models.yml` publishes models publicly and runs only when the owner starts it.
- Verify headlessly before asking the owner to look. Run `make test` and `make lint` before
  declaring a task done.
