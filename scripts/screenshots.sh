#!/usr/bin/env bash
# Captures the store screenshots headlessly and composes them, as in playlist-dl:
# raw window grabs from the GLATE_DEBUG_* hooks (MainWindow::debugRun), then
# scripts/screenshots.py frames them with a headline into screenshots/.
#
#   scripts/screenshots.sh            # needs a dev build (make build) and network
#
# Runs in a scratch profile under build-sdk/, never the owner's own data. The
# hero shot translates online; the offline shots use the en-de / de-en test
# models (python3 tools/models/models.py install third_party/test-models en-de de-en).
# Quit any running Glate first: a second instance hands over to it and exits.
set -euo pipefail

DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${GLATE_BUILD_DIR:-$DIR/build-sdk}"
PROFILE="$BUILD/screenshot-profile"
RAW="$BUILD/screenshots-raw"
CONF="$PROFILE/config/org.keshavnrj.ubuntu/Glate.conf"
MODELS="$PROFILE/data/org.keshavnrj.ubuntu/Glate/models"

[ -x "$BUILD/src/glate" ] || { echo "no dev build in $BUILD: run make build" >&2; exit 1; }
[ -f "$DIR/third_party/test-models/en-de/model.json" ] || {
    echo "test models missing: python3 tools/models/models.py install third_party/test-models en-de de-en" >&2
    exit 1
}

# A fresh profile each time, so history holds only what these runs translate.
rm -rf "$PROFILE" "$RAW"
mkdir -p "$(dirname "$CONF")" "$MODELS" "$RAW"
cat > "$CONF" <<CONF
[General]
firstRun=false
closeToTray=false
voiceGender=0
appearance/mode=dark
appearance/skin=fusion2
appearance/csd=true
CONF
cp -r "$DIR/third_party/test-models/en-de" "$DIR/third_party/test-models/de-en" "$MODELS/"
# The languages list with real sizes, read from a local file.
python3 "$DIR/tools/models/models.py" catalog "$PROFILE/catalog.json" \
    --base-url "https://github.com/keshavbhatt/glate-ng/releases/download/models/"

HERO_TEXT="Good morning! The train to the old town leaves from platform 4 at half past nine.
Tickets are cheaper online, and children under twelve travel free.

Please keep your luggage with you at all times."
OFFLINE_TEXT="Our meeting has moved to Thursday afternoon.
Could you send me the updated report before then?

Thank you, and have a nice weekend."

# shot <name> <seconds> [VAR=value ...]: one app run, one picture.
shot() {
    local name="$1" seconds="$2"
    shift 2
    env XDG_CONFIG_HOME="$PROFILE/config" XDG_DATA_HOME="$PROFILE/data" \
        QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME_OVERRIDE=none \
        GLATE_MODELS_CATALOG_URL="file://$PROFILE/catalog.json" \
        GLATE_DEBUG_WINDOW_SIZE=1100x680 GLATE_DEBUG_GRAB="$RAW/$name.png,$seconds" \
        "$@" "$DIR/scripts/dev-run.sh" >"$RAW/$name.log" 2>&1 || true
    [ -s "$RAW/$name.png" ] || { echo "no picture for $name, see $RAW/$name.log" >&2; exit 1; }
    echo "grabbed $name"
}

shot main 8 GLATE_DEBUG_OFFLINE=0 GLATE_DEBUG_LANGS=en:es GLATE_DEBUG_TEXT="$HERO_TEXT"
shot linebyline 8 GLATE_DEBUG_OFFLINE=0 GLATE_DEBUG_LANGS=en:ja GLATE_DEBUG_TEXT="$HERO_TEXT" \
    GLATE_DEBUG_OPEN=linebyline
shot offline 8 GLATE_DEBUG_OFFLINE=1 GLATE_DEBUG_LANGS=en:de GLATE_DEBUG_TEXT="$OFFLINE_TEXT"
shot languages 6 GLATE_DEBUG_OFFLINE=1 GLATE_DEBUG_OPEN=offline
shot history 6 GLATE_DEBUG_OPEN=history
shot settings 6 GLATE_DEBUG_OPEN=settings
shot share 8 GLATE_DEBUG_OFFLINE=0 GLATE_DEBUG_LANGS=en:fr GLATE_DEBUG_TEXT="$OFFLINE_TEXT" \
    GLATE_DEBUG_OPEN=share

uv run --quiet --with pillow python3 "$DIR/scripts/screenshots.py" "$RAW"
