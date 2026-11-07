#!/bin/bash
# Format test: generate short test files in many audio formats with ffmpeg,
# play each one in a throwaway copy of the packaged app, and check that the
# expected GStreamer decoder was used. Exits non-zero if a format that should
# work doesn't.
#
# Usage: test-formats.sh <packaged app dir> <binary path inside it> [work dir]
#   macOS:   test-formats.sh dist/TrackDeck/TrackDeck.app Contents/MacOS/TrackDeck
#   Windows: test-formats.sh dist/TrackDeck-windows/TrackDeck TrackDeck.exe
# Env: EXPECTED_MISSING="file1 file2"  formats known not to be supported here
#      PLAY_TIMEOUT=30                  how long to wait for a file to start playing
set -uo pipefail
APP=${1:?packaged app dir}
BIN_REL=${2:?binary path inside the app dir}
WORK=${3:-$(mktemp -d)}
PLAY_TIMEOUT=${PLAY_TIMEOUT:-30}
EXPECTED_MISSING=" ${EXPECTED_MISSING:-} "

rm -rf "$WORK"
mkdir -p "$WORK/files" "$WORK/logs"
cp -R "$APP" "$WORK/app"
BIN="$WORK/app/$BIN_REL"
BIN_DIR=$(dirname "$BIN")
CFG="$BIN_DIR/$(basename "$BIN" .exe).cfg"

# --- generate test files ------------------------------------------------------
# name | ffmpeg codec options | expected decoder element(s) (regex)
CASES=(
  "mp3.mp3|-c:a libmp3lame -b:a 192k|mpg123audiodec|avdec_mp3"
  "mp2.mp2|-c:a mp2 -b:a 192k|mpg123audiodec|avdec_mp2float"
  "aac.m4a|-c:a aac -b:a 192k|avdec_aac|faad"
  "aac-adts.aac|-c:a aac -b:a 192k|avdec_aac|faad"
  "alac.m4a|-c:a alac|avdec_alac"
  "flac.flac|-c:a flac|flacdec"
  "vorbis.ogg|-c:a vorbis -strict -2|vorbisdec|avdec_vorbis"
  "opus.opus|-c:a libopus -b:a 128k|opusdec"
  "wav16.wav|-c:a pcm_s16le|wavparse"
  "wav24.wav|-c:a pcm_s24le|wavparse"
  "aiff.aiff|-c:a pcm_s16be|aiffparse"
  "wma.wma|-c:a wmav2 -b:a 128k|avdec_wmav2"
  "wavpack.wv|-c:a wavpack|wavpackdec|avdec_wavpack"
  "ac3.ac3|-c:a ac3 -b:a 192k|avdec_ac3|a52dec"
)
for c in "${CASES[@]}"; do
  name=${c%%|*}; rest=${c#*|}; opts=${rest%%|*}
  # shellcheck disable=SC2086
  ffmpeg -hide_banner -loglevel error -y -f lavfi -i sine=frequency=440:duration=5:sample_rate=44100 \
    -ac 2 $opts -metadata title="FormatTest $name" "$WORK/files/$name" \
    || echo "warning: ffmpeg could not create $name"
done

native_path() { command -v cygpath >/dev/null && cygpath -w "$1" || echo "$1"; }

# decoders matching $2 that the GStreamer log $1 shows were created
decoders_used() {
  grep -o 'creating element "[a-z0-9_]*"' "$1" 2>/dev/null | sed 's/creating element "//; s/"//' \
    | grep -E -x "$2" | sort -u | tr '\n' ',' | sed 's/,$//'
}

# Launch the app on a file and wait until one of the expected decoders shows up
# in the GStreamer log, or until PLAY_TIMEOUT. Polling instead of a fixed wait
# keeps a slow start (a fresh CI runner scanning newly unpacked DLLs can take
# several seconds) from failing the test, and makes passing files quick.
play_file() { # file log expect
  rm -f "$BIN_DIR"/*.cfg "$BIN_DIR"/*.m3u "$BIN_DIR"/*.peaks "$2"
  printf '[General]\nSingleInstance=false\nAutoCheckUpdates=false\n' > "$CFG"
  GST_DEBUG="GST_ELEMENT_FACTORY:4" GST_DEBUG_NO_COLOR=1 GST_DEBUG_FILE="$(native_path "$2")" \
    "$BIN" "$(native_path "$1")" >/dev/null 2>&1 &
  local pid=$! waited=0
  while [ "$waited" -lt $((PLAY_TIMEOUT * 2)) ]; do
    [ -n "$(decoders_used "$2" "$3")" ] && break
    kill -0 "$pid" 2>/dev/null || break # the app quit by itself
    sleep 0.5
    waited=$((waited + 1))
  done
  sleep 0.5 # let the log settle before the app is stopped
  kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
}

# --- warm-up: the first launch of a fresh copy builds GStreamer's plugin
# registry (kept next to the app) and the first playback loads the decoder and
# audio output libraries, both of which can be slow on a fresh machine. Play
# one file all the way to its decoder before the real tests. ------------------
warm=${CASES[0]}; warm_name=${warm%%|*}; warm_rest=${warm#*|}
if [ -f "$WORK/files/$warm_name" ]; then
  start=$(date +%s)
  PLAY_TIMEOUT=$((PLAY_TIMEOUT * 3)) play_file "$WORK/files/$warm_name" "$WORK/logs/warm-up.log" "${warm_rest#*|}"
  echo "Warm-up took $(( $(date +%s) - start ))s"
fi
[ -f "$BIN_DIR/gstreamer-1.0.registry.bin" ] && echo "GStreamer registry ready" \
  || echo "warning: GStreamer registry not found after warm-up"

# --- play each file -----------------------------------------------------------
fail=0
printf "%-14s %-8s %s\n" FILE RESULT DECODER
for c in "${CASES[@]}"; do
  name=${c%%|*}; rest=${c#*|}; expect=${rest#*|}
  file="$WORK/files/$name"
  [ -f "$file" ] || { printf "%-14s %-8s %s\n" "$name" "SKIP" "(not generated)"; continue; }
  log="$WORK/logs/$name.log"
  start=$(date +%s)
  play_file "$file" "$log" "$expect"
  secs=$(( $(date +%s) - start ))
  used=$(decoders_used "$log" "$expect")
  if [ -n "$used" ]; then
    result=PASS; used="$used (${secs}s)"
  elif [[ "$EXPECTED_MISSING" == *" $name "* ]]; then
    result=KNOWN; used="(expected: not supported on this platform)"
  else
    result=FAIL; used="(no decoder; expected $expect)"; fail=1
  fi
  printf "%-14s %-8s %s\n" "$name" "$result" "$used"
done

if [ "$fail" -ne 0 ]; then
  echo "Some formats failed to play. Logs: $WORK/logs"
  exit 1
fi
echo "All supported formats play."
