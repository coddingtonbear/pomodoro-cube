# Fonts

Source faces for the countdown label, kept here so the conversion is
reproducible. `tools/convert-font.sh` reads from here and writes
`ArduinoIDE_V2/main/src/ui_font_Countdown_54.c`.

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
tools/convert-font.sh fonts/Oswald-SemiBold.ttf 62
```

62px rather than the script's default 54: Oswald is condensed, so it can run
taller before the digits reach the arc.

Licensed under the SIL Open Font License 1.1, in `OFL.txt`.
Upstream: https://github.com/google/fonts/tree/main/ofl/oswald
