# Desktop simulator

Runs the cube's firmware on a Linux desktop, with LVGL drawing into an SDL
window instead of into the GC9A01 over SPI. The point is to be able to work on
the UI and the timer behaviour without the hardware.

The firmware is **not** forked for this. `main.ino`, `display.cpp`, `util.cpp`,
`battery.cpp` and `beeper.cpp` are compiled exactly as they ship, against the
shims in `shim/`. The one exception is `qmi.cpp`, which needs the vendor
QMI8658 driver; `src/sim_qmi.cpp` stands in for it and synthesises an
accelerometer reading for whichever face the sim says the cube is resting on.

## Running

```bash
./run.sh
```

First run fetches LVGL v8.3.11 (matching what SquareLine Studio generated
against) and configures CMake; later runs just rebuild. Needs `cmake`, a C++17
compiler and `libsdl2-dev`:

```bash
sudo apt install cmake build-essential libsdl2-dev
```

## Controls

| Key | Effect |
| --- | --- |
| `1` `2` `3` `4` | Rest the cube on a face — 0°, 90°, 180°, 270° |
| `0` or `s` | Lay it face down, which puts the firmware into deep sleep |
| `b` | Cycle the low-battery warning: forced on, forced off, voltage-driven |
| `[` `]` | Lower / raise the simulated pack voltage |
| `v` | Toggle between the upright view and the raw panel |
| `m` | Toggle the round-panel mask |
| `r` | Reboot, as a wake-from-deep-sleep would |
| `q` or `Esc` | Quit |

`b` exists because nudging the voltage is a slow way to see the warning: the
firmware averages ten readings taken five seconds apart, so a change takes most
of a minute to land — and the window between `LOW_BATTERY_VOLTAGE` (3.6 V) and
`BAT_EMPTY_VOLTAGE` (3.5 V, where the firmware deep-sleeps) is narrow enough to
overshoot. The forced states still go through the firmware's own threshold,
just with the voltage model skipped. `[` or `]` hands control back.

By default the window shows the panel **upright**, the way someone holding the
cube on the current face sees it. `v` switches to the physical panel, where the
content appears rotated — useful when checking what `tft.setRotation()` is
actually doing.

Deep sleep parks the sim on a dark panel; any key re-executes the process,
which reproduces the cold boot the IMU interrupt causes on hardware.

## Scripted screenshots

Environment variables let a script capture the panel without a display:

```bash
SDL_VIDEODRIVER=dummy SIM_ORIENTATION=180 \
  SIM_SCREENSHOT=/tmp/face.bmp SIM_SCREENSHOT_MS=1500 ./build/sim
```

| Variable | Effect |
| --- | --- |
| `SIM_ORIENTATION` | Boot on a face: `0`, `90`, `180`, `270` or `sleep` |
| `SIM_BATTERY` | Starting pack voltage in volts, e.g. `3.65` |
| `SIM_SCREENSHOT` | Where to write the frame |
| `SIM_SCREENSHOT_MS` | When to grab it, in ms since boot |
| `SIM_KEYS` | Keys to press at startup, e.g. `b` or `mv` |

`SIM_SCREENSHOT_MS` also takes a comma-separated list, which captures a
countdown at several points in one run; each file then gets its elapsed time
appended to the name. The sim exits after the last capture.

```bash
SDL_VIDEODRIVER=dummy SIM_ORIENTATION=90 SIM_SCREENSHOT=/tmp/arc.bmp \
  SIM_SCREENSHOT_MS=1000,76000,151000,226000 ./build/sim
```

## Tests

`tests/` holds host tests for the firmware's pure logic — the face-to-timer
mapping in `util.cpp`, the accelerometer vectors the faces correspond to,
battery percentage, and the arc fill, arc colour and low-battery threshold in
`indicators.cpp`. They build as part of the same project:

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

## What is and isn't simulated

Faithful: the LVGL render path and the SquareLine UI, panel rotation, the
countdown and its debounce, battery percentage and bar colours, the deep-sleep
transitions.

Stubbed: the beeper is silent — `tone()` only sets a flag that shows up in the
window title as `BEEP`, so the sequence timing is visible but not audible. I2C,
RTC GPIO holds and the CPU frequency change are no-ops. Timing comes from the
host clock, so this says nothing about how the real ESP32 performs at 80 MHz.

## Layout

```
shim/     Arduino.h, TFT_eSPI, Wire, driver/rtc_io -- the hardware APIs faked
src/      the simulated panel, input state, IMU stand-in and SDL entry point
tests/    host tests for util.cpp, plus link stubs for its hardware calls
```

`shim/` is deliberately minimal: only the calls this firmware actually makes are
provided, so reaching for a new hardware API shows up as a compile error rather
than silently doing nothing.
