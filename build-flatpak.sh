#!/usr/bin/env bash
set -euo pipefail

APP_VERSION="$(grep -E '^project\(glate VERSION ' CMakeLists.txt | sed -E 's/.*VERSION ([0-9]+\.[0-9]+(\.[0-9]+)?).*/\1/')"
BUNDLE_NAME="glate-${APP_VERSION}.flatpak"
if [[ -z "${APP_VERSION}" ]]; then
  echo "Failed to parse version from CMakeLists.txt" >&2
  exit 1
fi

echo "Building Glate Flatpak..."

rm -rf .flatpak-builder build-flatpak repo glate.flatpak "${BUNDLE_NAME}"

flatpak-builder --force-clean --ccache --repo=repo build-flatpak com.ktechpit.glate.yml

flatpak build-bundle repo "${BUNDLE_NAME}" com.ktechpit.glate

echo ""
echo "Build complete"
echo ""
echo "Install local bundle:"
echo "  flatpak install --user ${BUNDLE_NAME}"
echo ""
echo "Or install from local repo:"
echo "  flatpak remote-add --user --no-gpg-verify glate-repo repo"
echo "  flatpak install --user glate-repo com.ktechpit.glate"

