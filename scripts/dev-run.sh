#!/usr/bin/env bash
# Runs the dev build against the kf6-core24 runtime content snap, the same
# libraries the shipped snap uses. Host fonts, display and D-Bus work directly.
#
#   scripts/dev-run.sh                 # launch the app
#   scripts/dev-run.sh --ctest [args]  # run the test suite (env baked in by CMake)
#   scripts/dev-run.sh -- --some-flag  # pass arguments to glate
set -euo pipefail

DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${GLATE_BUILD_DIR:-$DIR/build-sdk}"

# shellcheck source-path=SCRIPTDIR source=snap-runtime-env.sh
source "$DIR/scripts/snap-runtime-env.sh"

[ -d "$GLATE_RT/usr/lib/x86_64-linux-gnu" ] || {
    echo "kf6-core24 runtime snap missing: sudo snap install kf6-core24" >&2; exit 1; }

if [ "${1:-}" = "--ctest" ]; then
    # ctest is a host binary: keep the host environment. The tests get the
    # runtime env through their CTest ENVIRONMENT property (tests/CMakeLists.txt).
    cd "$BUILD"
    exec "$GLATE_CTEST" --output-on-failure "${@:2}"
fi

glate_prepare_runtime_farm "$BUILD"
glate_export_runtime_env "$BUILD"
export QT_QPA_PLATFORMTHEME="${QT_QPA_PLATFORMTHEME_OVERRIDE:-xdgdesktopportal}"

[ "${1:-}" = "--" ] && shift
cd "$DIR"
exec "$BUILD/src/glate" "$@"
