# Releasing Glate

Glate uses a tag-based flow (DOCS/DECISIONS.md ADR-007).

| Event | Snap Store | GitHub | Flathub |
|---|---|---|---|
| Push to `main` (app code changed) | `edge`, version `X.Y.Z+git<hash>` | CI only | nothing |
| Tag `vX.Y.Z` | `stable`, version `X.Y.Z` | release with Windows zip, Flatpak bundle, checksums | the update bot opens a pull request for the tag |
| By hand | | | merge the Flathub pull request |

A tag reaches every stable user directly, so check the edge build of the commit before
tagging it (`snap refresh glate --edge`).

## Cutting a release

1. Bump `project(glate VERSION X.Y.Z)` in `CMakeLists.txt`.
2. Add a `<release version="X.Y.Z" date="YYYY-MM-DD">` entry at the top of
   `src/com.ktechpit.glate.metainfo.xml`.
3. Commit and push to `main`; that build lands in edge. Check it
   (`snap refresh glate --edge`), then tag the same commit and push the tag:

   ```bash
   git tag -a vX.Y.Z -m "Glate X.Y.Z"
   git push origin vX.Y.Z
   ```

   Both workflows refuse a tag that does not match `project(VERSION)`.
   The tag's snaps (amd64 and arm64) are published straight to stable. To roll back, release
   the previous revision to stable: `snapcraft release glate <revision> stable`.
4. Flathub: the update bot opens a pull request in flathub/com.ktechpit.glate pointing at
   the new tag; check that its build passes, then merge. If the engine pin
   (`GLATE_ENGINE_COMMIT`) changed, update the engine archives in that pull request with
   `python3 tools/flathub/engine_sources.py`.

## One-time setup for publishing

- Secret `SNAPCRAFT_STORE_CREDENTIALS`: the output of
  `snapcraft export-login --snaps=glate --channels=edge,stable --acls=package_access,package_push,package_update,package_release -`.
- Repository variable `PUBLISH_TO_STORE=true`. Until it is set, snap builds run and keep
  the snaps as workflow artifacts, but publish nothing.

## Local package builds

```bash
snapcraft            # snap
./build-flatpak.sh   # Flatpak bundle from this source tree
```
