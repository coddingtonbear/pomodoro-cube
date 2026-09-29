#!/usr/bin/env bash
# Compile the firmware and flash it to the cube over USB-C.
#
#   tools/flash.sh [--build-only] [--monitor] [--clean] [--port /dev/ttyACM0]
#
# Does the whole job the README describes by hand: finds the arduino-cli
# bundled inside the Arduino IDE, checks the two configuration symlinks the
# build silently needs, brings the serial port into existence, compiles with
# the board settings the Waveshare module wants, uploads, and resets the board
# a second time so the accelerometer comes up.
#
#   --build-only  compile and report the size; don't touch the device
#   --monitor     tail UART0 at 115200 after flashing (ctrl-c to stop)
#   --clean       throw away the build cache first
#   --port P      flash this port instead of the first /dev/ttyACM*
#
# Override ARDUINO_CLI, FQBN or BUILD_DIR in the environment.
set -euo pipefail

REPO_ROOT=$(cd "$(dirname "$0")/.." && pwd)
SKETCH="$REPO_ROOT/ArduinoIDE_V2/main"

# ESP32S3 Dev Module as the README's board settings spell it out: 16MB flash,
# no PSRAM on this module, and USB CDC off because the port is a CH343 UART
# bridge rather than the S3's native USB.
FQBN=${FQBN:-esp32:esp32:esp32s3:FlashSize=16M,PSRAM=disabled,CDCOnBoot=default,PartitionScheme=default}

# Out of the repo, so the tree stays clean, but stable across runs so the
# second build only recompiles what changed.
BUILD_DIR=${BUILD_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/pomodoro-cube/build}

BUILD_ONLY=0
MONITOR=0
CLEAN=0
PORT=${PORT:-}

while [[ $# -gt 0 ]]; do
  case $1 in
    --build-only|-n) BUILD_ONLY=1 ;;
    --monitor|-m)    MONITOR=1 ;;
    --clean)         CLEAN=1 ;;
    --port|-p)       PORT=${2:?--port needs a device}; shift ;;
    -h|--help)       sed -n '2,18p' "$0" | sed 's/^# \?//'; exit 0 ;;
    *)               echo "unknown argument: $1" >&2; exit 1 ;;
  esac
  shift
done

say() { printf '\n== %s\n' "$*"; }

# arduino-cli is not packaged on this machine; the copy that exists is the one
# the IDE ships with, inside its unpacked download.
find_cli() {
  if [[ -n ${ARDUINO_CLI:-} ]]; then
    echo "$ARDUINO_CLI"; return
  fi
  if command -v arduino-cli >/dev/null 2>&1; then
    command -v arduino-cli; return
  fi
  local candidate
  candidate=$(ls -d "$HOME"/Downloads/arduino-ide_*_Linux_64bit/resources/app/lib/backend/resources/arduino-cli 2>/dev/null | tail -1 || true)
  if [[ -x ${candidate:-} ]]; then
    echo "$candidate"; return
  fi
  echo "no arduino-cli found: install it, or set ARDUINO_CLI to the one inside the Arduino IDE" >&2
  exit 1
}

# TFT_eSPI and LVGL both keep their configuration outside the sketch. Without
# these links the panel is configured as a different display on different pins,
# which the build now rejects rather than leaving a black screen to puzzle over.
check_links() {
  local libs="$HOME/Arduino/libraries"
  local missing=0
  local -a wanted=(
    "$libs/lv_conf.h:$REPO_ROOT/ArduinoIDE_V2/lv_conf.h"
    "$libs/TFT_eSPI/User_Setup.h:$REPO_ROOT/ArduinoIDE_V2/User_Setup.h"
  )
  for pair in "${wanted[@]}"; do
    local link=${pair%%:*} target=${pair#*:}
    if [[ ! -e $link ]]; then
      echo "missing $link -- link it with:" >&2
      echo "  ln -s \"$target\" \"$link\"" >&2
      missing=1
    elif [[ -L $link && $(readlink -f "$link") != "$(readlink -f "$target")" ]]; then
      echo "$link points at $(readlink "$link"), not $target" >&2
      missing=1
    fi
  done
  [[ $missing == 0 ]] || exit 1
}

# The CH343 bridge needs cdc_acm, which is blacklisted on this machine, so the
# port is absent after every reboot until the module is loaded.
ensure_port() {
  if [[ -n $PORT ]]; then
    [[ -e $PORT ]] || { echo "no such port: $PORT" >&2; exit 1; }
    return
  fi
  PORT=$(ls /dev/ttyACM* 2>/dev/null | head -1 || true)
  if [[ -z $PORT ]]; then
    say "no /dev/ttyACM*; loading cdc_acm (it is blacklisted on this machine)"
    sudo modprobe cdc_acm
    for _ in {1..10}; do
      PORT=$(ls /dev/ttyACM* 2>/dev/null | head -1 || true)
      [[ -n $PORT ]] && break
      sleep 0.5
    done
  fi
  if [[ -z $PORT ]]; then
    echo "still no serial port -- is the cube plugged in?" >&2
    exit 1
  fi
  if [[ ! -w $PORT ]]; then
    echo "$PORT is not writable; add yourself to the group owning it (usually dialout)" >&2
    exit 1
  fi
}

# Straight after an upload the board has come up with the QMI8658 silent --
# every read returns zero and the loop crawls. A second reset over the control
# lines fixes it, so do it unprompted rather than leaving a cube that looks
# broken.
reset_board() {
  python3 - "$PORT" <<'PY'
import sys, time
try:
    import serial
except ImportError:
    sys.exit("pyserial not installed; reset the cube by hand (unplug it)")
with serial.Serial(sys.argv[1], 115200) as port:
    port.dtr = False   # hold GPIO0 high so it boots the sketch, not the ROM loader
    port.rts = True    # assert reset
    time.sleep(0.1)
    port.rts = False
PY
}

CLI=$(find_cli)
check_links

if [[ $CLEAN == 1 ]]; then
  say "clearing $BUILD_DIR"
  rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"

# -O2 rather than the ESP32 core's -Os. Drawing the face is CPU-bound, and on
# the board a face redrawn at an angle took 36.5 ms at -Os and takes 32 at -O2,
# 25 frames a second to 28, for about 60 kB more flash. Only settable on the
# command line, so an Arduino IDE build is still -Os: slower, and otherwise the
# same firmware.
say "compiling $SKETCH"
"$CLI" compile --fqbn "$FQBN" --build-path "$BUILD_DIR" \
  --build-property "compiler.optimization_flags=-O2" "$SKETCH"

if [[ $BUILD_ONLY == 1 ]]; then
  say "built only, as asked; binary in $BUILD_DIR"
  exit 0
fi

ensure_port
say "uploading to $PORT"
"$CLI" upload --fqbn "$FQBN" --input-dir "$BUILD_DIR" --port "$PORT" "$SKETCH"

say "resetting $PORT so the accelerometer comes up"
reset_board

if [[ $MONITOR == 1 ]]; then
  say "serial on $PORT at 115200 -- ctrl-c to stop"
  stty -F "$PORT" 115200 raw -echo
  cat "$PORT"
else
  echo
  echo "flashed. Stand the cube on an edge to see a timer -- lying flat it reads"
  echo "as a resting face and deep-sleeps within a second."
  echo "Watch it with: tools/flash.sh --monitor, or"
  echo "  stty -F $PORT 115200 raw -echo && cat $PORT"
fi
