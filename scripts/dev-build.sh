#!/usr/bin/env bash
# Builds glate on the host against the KDE Qt 6.11 snap SDK
# (kde-qt6-core24-sdk), the same Qt the shipped snap gets at runtime from the
# kde-neon-6 extension. What compiles here is what ships (DOCS/DECISIONS.md ADR-001).
#
#   scripts/dev-build.sh            # RelWithDebInfo into ./build-sdk
#   GLATE_BUILD_TYPE=Debug scripts/dev-build.sh
#   GLATE_CMAKE_ARGS="-DGLATE_WERROR=ON" scripts/dev-build.sh
#   scripts/dev-build.sh --tests    # also run ctest afterwards
set -euo pipefail

DIR="$(cd "$(dirname "$0")/.." && pwd)"
SDK=/snap/kde-qt6-core24-sdk/current
BUILD="${GLATE_BUILD_DIR:-$DIR/build-sdk}"

# shellcheck source-path=SCRIPTDIR source=snap-runtime-env.sh
source "$DIR/scripts/snap-runtime-env.sh"

if [ ! -d "$SDK/usr/lib/x86_64-linux-gnu/cmake/Qt6" ]; then
    echo "Qt SDK snap missing. Install with:" >&2
    echo "  sudo snap install kde-qt6-core24-sdk" >&2
    exit 1
fi
if [ ! -d "$GLATE_RT/usr/lib/x86_64-linux-gnu" ]; then
    echo "kf6-core24 runtime snap missing (needed to run/test). Install with:" >&2
    echo "  sudo snap install kf6-core24" >&2
    exit 1
fi

# The SDK's own build tools (moc, rcc, ...) need the SDK's libraries, some of
# which sit in the libproxy subdirectory.
export LD_LIBRARY_PATH="$SDK/usr/lib/x86_64-linux-gnu:$SDK/usr/lib/x86_64-linux-gnu/libproxy${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# Extra CMake arguments: GLATE_CMAKE_ARGS="-DGLATE_WERROR=ON -DFOO=bar"
read -r -a EXTRA_ARGS <<< "${GLATE_CMAKE_ARGS:-}"

glate_prepare_runtime_farm "$BUILD"

# PCRE2 for the offline engine's sentence splitter. The SDK has the header but
# only a dangling libpcre2-8.so; the library itself is in the core24 base snap,
# as it is for the shipped snap. The header is linked on its own so the SDK's
# generic include directory never reaches the compiler.
mkdir -p "$BUILD/sdk-pcre2"
ln -sf "$SDK/usr/include/pcre2.h" "$BUILD/sdk-pcre2/pcre2.h"

# X11 and xcb come from the SDK too (the global shortcut), so a host compiler without
# multiarch paths does not pick up the host's copies.
"$GLATE_CMAKE" -S "$DIR" -B "$BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE="${GLATE_BUILD_TYPE:-RelWithDebInfo}" \
    -DCMAKE_PREFIX_PATH="$SDK/usr" \
    -DCMAKE_LIBRARY_PATH="$SDK/usr/lib/x86_64-linux-gnu" \
    -DQt6_DIR="$SDK/usr/lib/x86_64-linux-gnu/cmake/Qt6" \
    -DCMAKE_EXE_LINKER_FLAGS="-Wl,-rpath-link,$SDK/usr/lib/x86_64-linux-gnu -Wl,--allow-shlib-undefined" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DPCRE2_INCLUDE_DIRS="$BUILD/sdk-pcre2" \
    -DPCRE2_LIBRARIES="$BUILD/core24-libs/libpcre2-8.so.0" \
    -DGLATE_SNAP_RUNTIME="$GLATE_RT" \
    -DGLATE_SNAP_RUNTIME_LIB_PATH="$(glate_runtime_lib_path "$BUILD")" \
    "${EXTRA_ARGS[@]}"
"$GLATE_CMAKE" --build "$BUILD" -j"$(nproc)"

# clangd / IDEs look for compile_commands.json at the repo root.
ln -sf "$BUILD/compile_commands.json" "$DIR/compile_commands.json"

if [ "${1:-}" = "--tests" ]; then
    "$DIR/scripts/dev-run.sh" --ctest
fi
