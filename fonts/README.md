# Fonts

Source faces for the countdown label and the switch-on prompt, kept here so
the conversion is reproducible. `tools/convert-font.sh` reads from here and
writes `ArduinoIDE_V2/main/src/ui_font_Countdown_54.c` (or wherever `OUT`
says).

## Oswald-SemiBold.ttf

The countdown face. Oswald is a variable font upstream; this is the wght=600
instance, pinned with:

```
fonttools varLib.instancer Oswald[wght].ttf wght=600 -o Oswald-SemiBold.ttf
```

The instance matters — `lv_font_conv` takes a variable font's default instance,
which for Oswald is Regular, and Regular is too light for the panel.

Regenerate the countdown font with:

```
RANGE=0x30-0x3A,0x68 tools/convert-font.sh fonts/Oswald-SemiBold.ttf 62
```

The range adds `h` to the digits and colon, for the hours past an hour (1h25).

62px rather than the script's default 54: Oswald is condensed, so it can run
taller before the digits reach the arc.

The switch-on prompt's count is the same face, much larger, and only the two
digits it ever shows:

```
OUT=$PWD/ArduinoIDE_V2/main/src/ui_font_FlipCount.c RANGE=0x31-0x32 TABULAR=0 \
  tools/convert-font.sh fonts/Oswald-SemiBold.ttf 150
```

Licensed under the SIL Open Font License 1.1, in `OFL.txt`.
Upstream: https://github.com/google/fonts/tree/main/ofl/oswald
