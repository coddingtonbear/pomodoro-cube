# pomodoro-cube

A desk cube that runs a pomodoro timer. Stand it on a face to pick the
interval, lay it face up to pause, lay it face down to switch it off. There are
no buttons — the only control is which way up it is.

![The four timer faces](https://coddingtonbear-public.s3.amazonaws.com/github/pomodoro-cube/faces.png)

Forked from [fly-robin-fly/coffee_timer](https://github.com/fly-robin-fly/coffee_timer),
which does the same trick for coffee brew times.

> [!NOTE]
> This firmware has never run on real hardware — I haven't bought the parts
> yet. Everything here was developed against the [simulator](#developing-without-the-hardware),
> which runs the real firmware on a desktop. A handful of things can only be
> settled with a board in hand; they're listed under [Unverified](#unverified).

## Features

- **Four intervals, chosen by rotation** — 25 minutes on the default face,
  then 5, 50 and 10 going clockwise. Nothing to configure and nothing to
  press: the accelerometer reads which face is down and the timer starts.
- **An arc that drains rather than fills** — it starts full and empties as the
  time goes, shading green through amber to red so you can read roughly how
  long is left from across the room without reading the digits.
- **Pause by laying it face up** — the remaining time is kept and the panel is
  left showing the frozen countdown in muted colours. Stand it back on the
  same face to carry on. A *different* face is taken as choosing a different
  interval, so the pause is abandoned.
- **Face down means off** — the timer is discarded and the panel blanks.
- **A pause costs the same power as being off.** The GC9A01 refreshes itself
  from its own memory, so the frozen frame survives with the CPU stopped; only
  the backlight draws current.
- **A battery indicator that stays out of the way** — nothing on screen at all
  above 3.6 V, and below it a red empty-battery outline with the measured pack
  voltage inside, so the divider and ADC can be checked against a multimeter.
- **Home Assistant over BLE** — the cube's state is encoded as a
  [BTHome v2](https://bthome.io/format/) advertisement, which Home Assistant
  discovers natively. See [Bluetooth](#bluetooth) — the encoder is written and
  tested, but nothing transmits it yet.

## How it works

Orientation is the whole interface. A QMI8658 accelerometer reports which way
gravity points, `Util::calcOrientation()` turns that into one of six states —
four upright faces plus face up and face down — and a 300 ms debounce keeps a
cube mid-flip from starting a timer it doesn't mean.

Standing the cube on a face starts that face's countdown, unless there's a
paused timer belonging to that same face, in which case it picks up where it
left off. The arc drains as the countdown runs:

![The arc draining over a five-minute timer](https://coddingtonbear-public.s3.amazonaws.com/github/pomodoro-cube/countdown.png)

When the timer reaches zero the beeper plays a repeating pattern, the arc
refills and pulses red, and after thirty seconds the cube puts itself to sleep.
Completed 25-minute timers are counted; breaks are not.

![Paused, and the low battery warning](https://coddingtonbear-public.s3.amazonaws.com/github/pomodoro-cube/states.png)

A paused timer and the pomodoro count live in RTC memory, which survives deep
sleep but not a flat battery. Because a cold boot leaves arbitrary bits there,
the block carries a magic word; anything that doesn't match it is discarded
rather than resumed as a plausible-looking timer.

## Bluetooth

The cube advertises its state as [BTHome v2](https://bthome.io/format/) —
service data under UUID `0xFCD2` — which Home Assistant discovers with no
custom component, no MQTT and no ESPHome. It is one-way and connectionless, so
it costs almost nothing next to the backlight.

Objects, in the ascending order the spec requires, totalling 28 of the 31 bytes
a legacy advertisement allows:

| Object | ID | Type | Meaning |
| --- | --- | --- | --- |
| Packet ID | `0x00` | uint8 | Increments on change, so receivers can dedupe |
| Voltage | `0x0C` | uint16, ×0.001 V | The smoothed pack voltage, uncorrected |
| Connectivity | `0x19` | binary | 1 while awake, 0 in the advert sent before sleeping |
| Running | `0x27` | binary | Is the countdown advancing |
| Count | `0x3D` | uint16 | Completed 25-minute timers since the last power cycle |
| Duration | `0x42` | uint24, ×0.001 s | Remaining |
| Duration 2 | `0x42` | uint24, ×0.001 s | What the timer started at |

The device info byte sets the trigger-based flag, which tells Home Assistant
the cube advertises irregularly — without it, every normal sleep would look
like a fault.

**There are deliberately no event objects.** A BTHome event carries an event
id and no payload, so a "timer started" event couldn't say how long the
interval was; you'd read that from the duration objects anyway, and then risk
acting on the previous value if the sensors hadn't settled first. Automations
should key off `running` changing state instead, and classify the interval from
`Duration 2` — 1500 is work, 300 or 600 a break, 3000 the long face. State also
self-heals where an event can't: an advertisement is an unacknowledged
broadcast, so a missed event is gone for good while a missed state is
re-advertised a second later.

Four states come out of three fields, with no extra object on the wire:

| State | Condition |
| --- | --- |
| Armed | `not running`, `remaining == started` |
| Running | `running` |
| Paused | `not running`, `0 < remaining < started` |
| Finished | `remaining == 0` |

**A farewell advert goes out before every sleep**, with `connectivity` dropped
to 0, because Home Assistant holds the last state it heard — otherwise a busy
light keyed on `running` would stay lit until the cube was next picked up. Why
it slept is inferable from the rest of the payload: `remaining == 0` is a
normal finish, a low voltage is a flat pack, and anything else is the cube
being put away.

`bthome.cpp` builds this payload and is tested against exact bytes. Nothing
transmits it yet — that needs NimBLE and a board. The simulator prints what
would go out when you press `a`:

```
02 01 06 18 16 D2 FC 44 00 00 0C EC 0E 19 01 27 01 3D 00 00 42 78 DF 16 42 60 E3 16
```

## Hardware

An ESP32 driving a 240×240 round GC9A01 over SPI, a QMI8658 accelerometer over
I²C, a piezo beeper, and a LiPo read through a 200k/100k divider. The pin map
in `ArduinoIDE_V2/main/consts.h` and the use of `ext0` deep-sleep wake point at
a Waveshare 1.28" round-display board — an S3 rather than a C3, since the C3
has neither `ext0` nor the RTC GPIO functions the firmware calls.

## Building

Open `ArduinoIDE_V2/main` in the Arduino IDE. Dependencies, all from the
Library Manager:

- TFT_eSPI 2.5.43 (Bodmer) — configured by `tft_setup.h`
- lvgl 8.3.11
- SensorQMI8658 0.4.1 (Lewis He)

`ArduinoIDE_V2/main/src/` began life as SquareLine Studio output and is now
maintained by hand. **Re-exporting from `SquareLine/coffee_timer.spj` would
overwrite it**, countdown font included.

## Developing without the hardware

```bash
./sim/run.sh
```

This compiles the real firmware for a Linux desktop and draws LVGL into an SDL
window instead of a GC9A01 over SPI. It isn't a reimplementation: `main.ino`,
`display.cpp`, `util.cpp`, `indicators.cpp`, `rtc_state.cpp`, `bthome.cpp`,
`battery.cpp` and `beeper.cpp` all compile exactly as they ship, against fake
`Arduino.h`, `TFT_eSPI`, `Wire` and `driver/rtc_io` headers. Only `qmi.cpp` is
replaced, because it needs the vendor IMU driver.

RTC memory is modelled rather than faked — the block survives a simulated deep
sleep, and a cold boot fills it with junk rather than zeroes, so the magic-word
guard is genuinely exercised.

See [sim/README.md](sim/README.md) for the controls, the scripted-screenshot
environment variables, and what the simulator can't tell you.

Host tests cover the parts that are pure logic — the face-to-timer mapping, the
arc's fill and colour ramp, the low-battery threshold, the RTC guard, and the
BTHome encoder's exact output bytes:

```bash
cmake --build sim/build && ctest --test-dir sim/build --output-on-failure
```

## Changing the countdown font

```bash
tools/convert-font.sh fonts/Oswald-SemiBold.ttf 62
```

The font slot is named for its role rather than the typeface, so nothing else
needs editing. The script subsets to the digits and colon, renders at 4 bpp for
antialiasing, and equalises the digit widths so the countdown doesn't shift
sideways as it ticks — most faces draw a much narrower `1` than `0`, and on a
centred label that slides the whole time every second.

The current face is [Oswald](https://github.com/google/fonts/tree/main/ofl/oswald)
SemiBold at 62px, chosen because it's condensed: on a 240px round panel with an
arc eating the edges, height is the scarce resource. See
[fonts/README.md](fonts/README.md) for why the weight is pinned to a static
instance.

## Unverified

Things that need a board, collected so they can be checked in one sitting:

- **Which rotation direction is clockwise.** `DEG_0` → `DEG_90` → `DEG_180` →
  `DEG_270` is one consistent direction, but whether it's clockwise depends on
  the accelerometer's sign convention. If it runs backwards, swap two cases in
  `Util::getTimerByOrientation()`.
- **Which sign of `az` is face up.** Same class of unknown; two lines in
  `Util::calcOrientation()` exchange if it's the other way round.
- **`BAT_FULL_VOLTAGE` was removed, but `BAT_EMPTY_VOLTAGE` and
  `LOW_BATTERY_VOLTAGE` are still guesses** inherited from upstream. They
  calibrate the whole measurement chain — divider, ADC, cell — not just the
  cell, so they should only be retuned against real readings.
- **Whether the display stays powered in deep sleep**, which the lit-while-
  paused behaviour depends on.
- **Everything about the radio** — NimBLE plumbing, a static MAC, and whether
  adverts actually escape during the ~1 s window before the CPU stops.

## Layout

```
ArduinoIDE_V2/main/       the firmware, built with the Arduino IDE
ArduinoIDE_V2/main/src/   the LVGL UI, originally SquareLine Studio output
SquareLine/               the SquareLine Studio project the UI came from
fonts/                    source faces for the countdown label
sim/                      desktop simulator and host tests
tools/                    font conversion
```
