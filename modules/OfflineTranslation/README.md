# OfflineTranslation

Offline translation for glate with Mozilla's Firefox translation models
(DOCS/DECISIONS.md ADR-003 to ADR-005). Qt Core and Qt Network only, no widgets. Sources are C++20; public headers stay C++17 because
the app still builds as C++17.

| File | Role |
|---|---|
| `Languages` | glate (Google) codes to model codes, and routes through English |
| `ModelCatalog` | parses and validates the release's `catalog.json` |
| `ModelStore` | installed models on disk, one directory per direction |
| `ModelManager` | fetches the catalogue, downloads, verifies (SHA-256) and installs |
| `EngineConfig` | the engine's YAML settings for one direction |
| `TranslationEngine` | blocking wrapper around Mozilla's engine (pimpl, no engine headers leak) |
| `OfflineTranslator` | runs the engine on a worker thread, results as signals |

No model ships with the app. The catalogue comes from the `models` release of this
repository, filled by `.github/workflows/models.yml` from
`tools/models/models.lock.json`. `GLATE_MODELS_CATALOG_URL` overrides the address for testing.

Build without the engine: `-DGLATE_OFFLINE_ENGINE=OFF`. The module still builds, and
`TranslationEngine::isAvailable()` returns false so the UI can hide offline mode.
