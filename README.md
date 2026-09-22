# pomodoro-cube

A desk cube that runs a pomodoro timer. Rest it on a face to pick the interval:
25 minutes on the default face, then 5, 50 and 10 going clockwise. Lay it face
down and it sleeps.

Forked from [fly-robin-fly/coffee_timer](https://github.com/fly-robin-fly/coffee_timer),
which did the same trick for coffee brew times.

## Hardware

An ESP32 driving a 240x240 round GC9A01 over SPI, a QMI8658 accelerometer over
I2C for orientation, a piezo beeper, and a LiPo read through a 200k/100k
divider. Pin assignments are in `ArduinoIDE_V2/main/consts.h`.

## Layout

```
ArduinoIDE_V2/main/       the firmware, built with the Arduino IDE
ArduinoIDE_V2/main/src/   the LVGL UI (originally SquareLine Studio output)
SquareLine/               the SquareLine Studio project the UI came from
fonts/                    source faces for the countdown label
sim/                      desktop simulator and host tests -- see sim/README.md
tools/                    font conversion
```

`ArduinoIDE_V2/main/src/` started as SquareLine Studio generated output but is
now maintained by hand. **Re-exporting from `SquareLine/coffee_timer.spj` would
overwrite it**, including the countdown font.

## Battery

There is no charge gauge. Below `LOW_BATTERY_VOLTAGE` (3.6 V) an empty battery
outline appears in red with the measured pack voltage printed inside it; above
that, nothing is shown. The voltage is there so the 200k/100k divider and the
ESP32 ADC can be checked against a multimeter once the hardware exists — it is
the raw smoothed reading, not a derived figure.

Below `BAT_EMPTY_VOLTAGE` (3.5 V) the firmware deep-sleeps rather than running
the pack flat.

## Developing without the hardware

```bash
./sim/run.sh
```

Runs the real firmware on the desktop against faked hardware, drawing into an
SDL window. See [sim/README.md](sim/README.md) for controls, tests and limits.

## Changing the countdown font

```bash
tools/convert-font.sh fonts/Oswald-SemiBold.ttf 62
```

Rewrites `ArduinoIDE_V2/main/src/ui_font_Countdown_54.c` in place. The font slot
is named for its role rather than for the typeface, so nothing else needs
editing. The script subsets to the digits and colon, renders at 4bpp for
antialiasing, and equalises the digit widths so the countdown doesn't shift
sideways as it ticks (`TABULAR=0` to skip that, `RANGE` and `BPP` to override
the rest).

The current font is [Oswald](https://github.com/google/fonts/tree/main/ofl/oswald)
SemiBold at 62px. It is condensed, so the digits run taller than a normal-width
face can before reaching the arc. Source faces live in `fonts/` — see
[fonts/README.md](fonts/README.md) for why the weight is pinned to a static
instance.
