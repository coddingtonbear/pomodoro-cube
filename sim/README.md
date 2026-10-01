# Desktop simulator

Runs the cube's firmware on a Linux desktop, with LVGL drawing into an SDL
window instead of into the GC9A01 over SPI. The point is to be able to work on
the UI and the timer behaviour without the hardware.

The firmware is **not** forked for this. `main.ino`, `display.cpp`, `util.cpp`,
`battery.cpp` and `haptic.cpp` are compiled exactly as they ship, against the
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
| | `3` is flow's work face, which counts up, and `4` spends the break it banks |
| `z` `x` | Lean it 5° anticlockwise / clockwise off that face, up to 40° |
| `0` or `s` | Lay it face down |
| `u` | Lay it face up. On a switched-off cube, `u` `0` `u` `0` `u` is the three flips that switch it on |
| `b` | Cycle the low-battery warning: forced on, forced off, voltage-driven |
| `[` `]` | Lower / raise the simulated pack voltage |
| `t` | Tap the cube, which brightens the panel for ten seconds |
| `d` | Double-tap it |
| `v` | Toggle between the viewer's view and the raw panel |
| `m` | Toggle the round-panel mask |
| `a` | Print the BLE advertisement the firmware last published |
| `r` | Reboot, as a wake-from-deep-sleep would |
| `q` or `Esc` | Quit |

`b` exists because nudging the voltage is a slow way to see the warning: the
firmware averages ten readings taken five seconds apart, so a change takes most
of a minute to land. The forced states still go through the firmware's own
threshold, just with the voltage model skipped. `[` or `]` hands control back.

By default the window shows the panel as someone looking at the cube sees it:
turned to the angle the cube is held at, which is the face it is on plus any
lean. A face that has caught up with the cube is therefore upright, and one
that has not is as crooked as it would be in the hand — which is the only way
to see a turn being eased. `v` switches to the physical panel, where the
content appears rotated — useful when checking what `tft.setRotation()` is
actually doing.

The simulated cube changes face in no time at all, which no hand does, so a
turn here is the worst case the easing has to cope with rather than what one
looks like.

Face down switches off and blanks the panel; face up pauses, leaving the frozen
countdown lit. Either way any key re-executes the process, reproducing the cold
boot the IMU interrupt causes on hardware. A face key wakes onto that face and
any other key onto 90°. To switch a face-down cube back on, wake it with `u`
and press `k` or `d` within five seconds.

## Scripted screenshots

Environment variables let a script capture the panel without a display:

```bash
SDL_VIDEODRIVER=dummy SIM_ORIENTATION=180 \
  SIM_SCREENSHOT=/tmp/face.bmp SIM_SCREENSHOT_MS=1500 ./build/sim
```

| Variable | Effect |
| --- | --- |
| `SIM_ORIENTATION` | Boot on a face: `0`, `90`, `180`, `270`, `up` or `down` |
| `SIM_BATTERY` | Starting pack voltage in volts, e.g. `3.65` |
| `SIM_SCREENSHOT` | Where to write the frame |
| `SIM_SCREENSHOT_MS` | When to grab it, in ms since boot |
| `SIM_KEYS` | Keys to press, e.g. `b`, `mv`, or `u@12000` for 12s in |
| `SIM_BLE_TRACE` | Print every BLE advertisement as it goes out |
| `SIM_TIME_SCALE` | Run the firmware's clock this many times faster than real time, e.g. `600` |

`SIM_KEYS` entries are comma-separated, and `key@ms` presses one part-way
through a run — which is how pausing gets exercised, since it needs a face
change while a countdown is already going.

`SIM_BLE_TRACE` with `SIM_KEYS` is how the advertising sequence gets checked
without a keyboard — including the farewell, which only goes out on the way into
deep sleep:

```bash
SDL_VIDEODRIVER=dummy SIM_BLE_TRACE=1 SIM_ORIENTATION=0 SIM_KEYS=u@6000 ./build/sim
```

`SIM_SCREENSHOT_MS` also takes a comma-separated list, which captures a
countdown at several points in one run; each file then gets its elapsed time
appended to the name. The sim exits after the last capture.

```bash
SDL_VIDEODRIVER=dummy SIM_ORIENTATION=90 SIM_SCREENSHOT=/tmp/arc.bmp \
  SIM_SCREENSHOT_MS=1000,76000,151000,226000 ./build/sim
```

`SIM_TIME_SCALE` speeds up `millis()`, `delay()`, and with them the times in
`SIM_KEYS` and `SIM_SCREENSHOT_MS`, which are all in simulated milliseconds.
A flow stint past its first hour is a minute away at `100`:

```bash
SDL_VIDEODRIVER=dummy SIM_TIME_SCALE=100 SIM_ORIENTATION=180 \
  SIM_KEYS=t@5995000 SIM_SCREENSHOT=/tmp/hour.bmp SIM_SCREENSHOT_MS=6000000 ./build/sim
```

The timer on screen falls behind the clock, though. The firmware advances it
by at most one second per pass of `loop()`, and a pass still takes as long on
the wall clock as its drawing does — a few milliseconds, which at `100` is
most of a simulated second. The run above shows about 1:18, not 1:40, and at
`600` a pass is several seconds and an hour's run shows about fifteen minutes.
Aim a screenshot by what is on screen rather than by the arithmetic, and keep a
tap no more than a few simulated seconds before the capture it is meant to
brighten.

## Tests

`tests/` holds host tests for the firmware's pure logic — the face-to-timer
mapping and the flow bank's arithmetic in `util.cpp`, the
accelerometer vectors the faces correspond to, and the arc fill, arc colour,
flow lap indicator, MM:SS-to-HH:MM switch and low-battery threshold in
`indicators.cpp`. They build as part of the same project:

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

## What is and isn't simulated

Faithful: the LVGL render path and the SquareLine UI, panel rotation, the
countdown and its debounce, the arc and battery indicator, the deep-sleep
transitions, and RTC memory — the block survives a simulated deep sleep and is
filled with junk on a cold boot, exactly as the real thing would be, so the
magic-word guard is genuinely exercised rather than getting away with a benign
block of zeroes. A paused panel stays lit because the sim decides from the
panel's own state (backlight on, no Sleep In command) rather than from knowing
the firmware is asleep.

One gap around pausing: the device holds `TFT_BL_PIN` through deep sleep, so the
frame stays lit across the wake until `Display::setup()` releases the hold. The
sim's wake is a fresh process, so its panel goes dark for the moment the boot
takes. Nothing depends on it, but the real cube will look slightly smoother.

There is no radio: `a` prints the BTHome payload `bthome.cpp` builds rather than
transmitting it, which is enough to read a payload back against the spec or paste
it into a decoder.

Stubbed: the vibration motor is a flag that follows its pin and shows up in the
window title as `BUZZ`, so the pattern timing is visible. It does not shake the
simulated accelerometer, so how the motor disturbs the IMU is a question for the
board. I2C,
RTC GPIO holds and the CPU frequency change are no-ops. Timing comes from the
host clock, so this says nothing about how the real ESP32 performs at 80 MHz.

## Layout

```
shim/     Arduino.h, TFT_eSPI, Wire, driver/rtc_io -- the hardware APIs faked
src/      the simulated panel, input state, IMU stand-in and SDL entry point
tests/    host tests for the firmware's pure logic, plus link stubs
```

`shim/` is deliberately minimal: only the calls this firmware actually makes are
provided, so reaching for a new hardware API shows up as a compile error rather
than silently doing nothing.
