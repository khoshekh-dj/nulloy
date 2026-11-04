#!/bin/bash
# Build a self-contained <Name>.app + <Name>.dmg from an already-built app bundle
# Usage: package-mac.sh <path/to/built/TrackDeck.app> <output dir>
set -euo pipefail
SRC=${1:?path to built app bundle}
SRC=${SRC%/}
NAME=$(basename "$SRC" .app)
BIN=$(/usr/libexec/PlistBuddy -c "Print CFBundleExecutable" "$SRC/Contents/Info.plist")
OUT=${2:?output dir}
QT=$(brew --prefix qt@5)
GST=$(brew --prefix gstreamer)
GSTLIB=$(brew --prefix)/lib/gstreamer-1.0

rm -rf "$OUT"
mkdir -p "$OUT"
APP="$OUT/$NAME.app"
ditto "$SRC" "$APP"
# strip local user data / caches that live next to the binary
rm -f "$APP/Contents/MacOS/$BIN".{cfg,m3u,peaks} "$APP/Contents/MacOS/gstreamer-1.0.registry.bin"

# audio-only GStreamer plugin set
PLUGINS="coreelements typefindfunctions playback autodetect osxaudio app
 audioconvert audioresample volume audioparsers id3demux apetag icydemux
 isomp4 mpg123 flac ogg vorbis opus opusparse wavparse aiff faad musepack
 libav"
mkdir -p "$APP/Contents/PlugIns/gstreamer"
EXES=()
for p in $PLUGINS; do
    cp "$GSTLIB/libgst$p.dylib" "$APP/Contents/PlugIns/gstreamer/"
    EXES+=("-executable=$APP/Contents/PlugIns/gstreamer/libgst$p.dylib")
done
cp "$GST/libexec/gstreamer-1.0/gst-plugin-scanner" "$APP/Contents/MacOS/"
chmod u+w "$APP"/Contents/MacOS/gst-plugin-scanner "$APP"/Contents/PlugIns/gstreamer/*
EXES+=("-executable=$APP/Contents/MacOS/gst-plugin-scanner")
for p in "$APP"/Contents/MacOS/plugins/*.dylib; do
    EXES+=("-executable=$p")
done

"$QT/bin/macdeployqt" "$APP" "${EXES[@]}" -always-overwrite > "$OUT/../macdeployqt.log" 2>&1

# copy in @rpath dependencies macdeployqt missed and verify everything resolves
python3 "$(dirname "$0")/mac-fix-deps.py" "$APP" "$(brew --prefix)"

# licence texts for every bundled Homebrew library: dependency closure of the
# original (not rewritten) binaries plus everything that ended up in the bundle
python3 "$(dirname "$0")/mac-collect-licenses.py" "$APP" "$(brew --prefix)" "$(cd "$(dirname "$0")/.." && pwd)" \
    "$SRC/Contents/MacOS/$BIN" "$SRC"/Contents/MacOS/plugins/*.dylib \
    "$GST/libexec/gstreamer-1.0/gst-plugin-scanner" $(for p in $PLUGINS; do echo "$GSTLIB/libgst$p.dylib"; done)

# drop leftover rpaths that point into Homebrew
while IFS= read -r -d '' f; do
    file "$f" | grep -q Mach-O || continue
    for r in $(otool -l "$f" | grep -A2 LC_RPATH | awk '/ path /{print $2}' | grep -E '^(/opt/homebrew|/usr/local)' || true); do
        install_name_tool -delete_rpath "$r" "$f"
    done
done < <(find "$APP" -type f -print0)

# verify no binary still depends on Homebrew (ignoring each dylib's own install name)
LEFT=""
while IFS= read -r -d '' f; do
    file "$f" | grep -q Mach-O || continue
    id=$(otool -D "$f" | tail -n +2)
    deps=$(otool -L "$f" | tail -n +2 | awk '{print $1}' | grep -E '^(/opt/homebrew|/usr/local)' | grep -vxF "$id" || true)
    rpaths=$(otool -l "$f" | grep -A2 LC_RPATH | awk '/ path /{print $2}' | grep -E '^(/opt/homebrew|/usr/local)' || true)
    [ -n "$deps$rpaths" ] && LEFT+="$f: $deps $rpaths"$'\n'
done < <(find "$APP" -type f -print0)
if [ -n "$LEFT" ]; then
    echo "UNRESOLVED Homebrew references:"
    echo "$LEFT"
    exit 1
fi

# codesign requires Contents/MacOS to hold only code: move data to Resources
# and leave symlinks where the app looks for it (next to the binary)
mkdir -p "$APP/Contents/Resources"
for d in i18n skins; do
    if [ -d "$APP/Contents/MacOS/$d" ]; then
        mv "$APP/Contents/MacOS/$d" "$APP/Contents/Resources/$d"
        ln -s "../Resources/$d" "$APP/Contents/MacOS/$d"
    fi
done

# ad-hoc sign inside-out: every Mach-O file, then frameworks, then the app
while IFS= read -r -d '' f; do
    file "$f" | grep -q Mach-O || continue
    [ "$f" = "$APP/Contents/MacOS/$BIN" ] && continue
    codesign --force --sign - "$f" 2>/dev/null
done < <(find "$APP/Contents" -type f -print0)
for fw in "$APP"/Contents/Frameworks/*.framework; do
    codesign --force --sign - "$fw"
done
codesign --force --sign - "$APP"
codesign --verify --strict "$APP"

ln -s /Applications "$OUT/Applications"
hdiutil create -quiet -volname "$NAME" -srcfolder "$OUT" -ov -format UDZO "$OUT/../$NAME.dmg"
rm "$OUT/Applications"
du -sh "$APP" "$OUT/../$NAME.dmg"
