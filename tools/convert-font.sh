#!/usr/bin/env bash
# Regenerate the countdown font from a TTF/OTF.
#
# The countdown label renders exactly eleven characters -- the digits and a
# colon -- so the default range is subset to those. That is both much smaller
# than a full ASCII range and cheap enough to afford 4bpp antialiasing, which
# is what stops the digits looking stair-stepped on the round panel.
#
#   tools/convert-font.sh <font.ttf> [size]
#
# Always writes src/ui_font_Countdown_54.c, defining lv_font_t
# ui_font_Countdown_54. The slot is named for its role rather than for the
# typeface in it, so changing fonts needs no source edits -- just rerun this.
#
# Override RANGE or BPP in the environment to reach beyond the digits.
set -euo pipefail

if [[ $# -lt 1 ]]; then
  sed -n '2,15p' "$0" | sed 's/^# \?//'
  exit 1
fi

FONT_PATH=$1
SIZE=${2:-54}
RANGE=${RANGE:-0x30-0x3A}
BPP=${BPP:-4}

REPO_ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT="$REPO_ROOT/ArduinoIDE_V2/main/src/ui_font_Countdown_54.c"

if [[ ! -f "$FONT_PATH" ]]; then
  echo "no such font file: $FONT_PATH" >&2
  exit 1
fi

# --no-compress is required: LVGL is built here without LV_USE_FONT_COMPRESSED.
npx --yes lv_font_conv@1.5.3 \
  --font "$FONT_PATH" \
  --size "$SIZE" \
  --bpp "$BPP" \
  --range "$RANGE" \
  --format lvgl \
  --no-compress \
  --no-prefilter \
  --lv-include lvgl.h \
  -o "$OUT"

# A ticking countdown on a centred label jumps sideways if "1" is narrower
# than the other digits, so equalise them unless asked not to.
if [[ ${TABULAR:-1} == 1 ]]; then
  "$REPO_ROOT/tools/tabular-digits.py" "$OUT"
fi

echo "wrote $OUT ($(wc -c < "$OUT") bytes, ${BPP}bpp, range $RANGE)"
