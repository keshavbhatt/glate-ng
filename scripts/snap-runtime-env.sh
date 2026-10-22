#!/usr/bin/env bash
# Sourced by dev-build.sh / dev-run.sh. Provides:
#   GLATE_RT              runtime content snap root (kf6-core24)
#   GLATE_CMAKE, GLATE_CTEST  cmake and ctest to use
#   glate_prepare_runtime_farm <build-dir>
#   glate_runtime_lib_path     <build-dir>
#   glate_export_runtime_env   <build-dir>
#
# The dev build links against the kde-qt6-core24-sdk snap and runs against the
# kf6-core24 content snap, the exact libraries the shipped snap uses. Host
# glibc stays authoritative (the content snap carries no libc).

GLATE_RT=/snap/kf6-core24/current

# The SDK's library path (needed by moc, rcc and uic) shadows libraries the
# host cmake links against, such as libarchive, so prefer the cmake snap,
# which carries its own. Ubuntu CI runners have no clash and use their cmake.
if [ -x /snap/bin/cmake ]; then
    GLATE_CMAKE=/snap/bin/cmake
    GLATE_CTEST=/snap/bin/cmake.ctest
else
    GLATE_CMAKE=cmake
    GLATE_CTEST=ctest
fi
export GLATE_CMAKE GLATE_CTEST
GLATE_CORE24_BASE=/snap/core24/current/usr/lib/x86_64-linux-gnu

# Some runtime-snap libs depend on Ubuntu base libraries the host may not
# have (libselinux, liblerc, ...). Farm them from the core24 base snap,
# EXCLUDING the glibc family: the host's newer glibc must stay
# authoritative or the loader chokes on GLIBC_PRIVATE symbols.
glate_prepare_runtime_farm() {
    local build="$1"
    local farm="$build/core24-libs"
    mkdir -p "$farm"
    local f b
    for f in "$GLATE_CORE24_BASE"/*.so*; do
        b="$(basename "$f")"
        case "$b" in
            libc.so*|libm.so*|libmvec.so*|libpthread.so*|libdl.so*|librt.so*|\
            ld-linux*|libresolv.so*|libnsl.so*|libnss_*|libanl.so*|libutil.so*|\
            libBrokenLocale.so*|libthread_db.so*) continue ;;
        esac
        ln -sf "$f" "$farm/$b"
    done
}

glate_runtime_lib_path() {
    local build="$1"
    local rt="$GLATE_RT/usr/lib/x86_64-linux-gnu"
    # libpulse (Qt Multimedia) finds libpulsecommon through a RUNPATH that
    # points at the host's /usr/lib/x86_64-linux-gnu/pulseaudio.
    echo "$rt:$rt/libproxy:$rt/pulseaudio:$build/core24-libs"
}

glate_export_runtime_env() {
    local build="$1"
    local lib_path
    lib_path="$(glate_runtime_lib_path "$build")"
    export LD_LIBRARY_PATH="$lib_path${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export QT_PLUGIN_PATH="$GLATE_RT/usr/lib/x86_64-linux-gnu/qt6/plugins"
    # Qt Multimedia's PipeWire and GStreamer backends load plugins from compiled-in
    # Ubuntu paths; point them at the runtime snap's copies.
    export SPA_PLUGIN_DIR="$GLATE_RT/usr/lib/x86_64-linux-gnu/spa-0.2"
    export PIPEWIRE_MODULE_DIR="$GLATE_RT/usr/lib/x86_64-linux-gnu/pipewire-0.3"
    export GST_PLUGIN_SYSTEM_PATH="$GLATE_RT/usr/lib/x86_64-linux-gnu/gstreamer-1.0"
    # This Qt build logs to journald when stderr is not a TTY; for dev runs we
    # want plain stderr (file-capturable).
    export QT_FORCE_STDERR_LOGGING=1
}
