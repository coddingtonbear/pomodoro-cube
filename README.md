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

- **Four intervals, chosen by rotation** — 25 minutes on the default face and
  5 on the next, then flow mode's pair. Nothing to configure and nothing to
  press: the accelerometer reads which face is down and the timer starts.
- **Flow mode, on the third and fourth faces** — the work face counts *up* for
  as long as the cube stands on it, and the break face then counts down a fifth
  of what it counted. Fifty minutes of work buys ten of break, which is the
  fixed pair flow mode replaced; an hour and a half buys eighteen. See
  [Flow mode](#flow-mode).
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

Standing the cube on a face starts that face's timer, unless there's a paused
one belonging to that same face, in which case it picks up where it left off.
Three of the faces count down and the arc drains as they run; the fourth is
[flow mode](#flow-mode), which counts up:

![The arc draining over a five-minute timer](https://coddingtonbear-public.s3.amazonaws.com/github/pomodoro-cube/countdown.png)

When the timer reaches zero the beeper plays a repeating pattern, the arc
refills and pulses red, and after thirty seconds the cube puts itself to sleep.
Work counts towards a pomodoro total — both work faces, the fixed one when it
reaches zero and flow's when the stint ends — and breaks do not.

![Paused, and the low battery warning](https://coddingtonbear-public.s3.amazonaws.com/github/pomodoro-cube/states.png)

A paused timer and the pomodoro count live in RTC memory, which survives deep
sleep but not a flat battery. Because a cold boot leaves arbitrary bits there,
the block carries a magic word; anything that doesn't match it is discarded
rather than resumed as a plausible-looking timer.

## Flow mode

The third face counts up instead of down, for a stretch of work that doesn't
fit a fixed interval. Turning the cube onto the fourth face ends the stint and
starts a break a fifth as long.

The panel inverts to black on white while a stint runs, because a number that
is going up looks exactly like one going down:

![Flow mode counting up, and the break it earned](https://coddingtonbear-public.s3.amazonaws.com/github/pomodoro-cube/flow.png)

Counting up has no total for the arc to drain against, so the arc becomes a lap
indicator instead: it fills over 25 minutes, shading green to red as the lap
ages, then starts again. Past an hour the label runs out of digits for MM:SS and
switches to HH:MM, with a small `h:m` marker underneath saying so — the two are
otherwise indistinguishable.

A stint's time is banked when it ends, and only the break face spends it. The
rules that fall out of that:

| Situation | What happens |
| --- | --- |
| Flow work → flow break | The break is a fifth of the stint, and the stint counts as a pomodoro |
| Flow work → face up | Parked, not ended: standing the cube back on that face carries on counting up |
| Flow work → any other timer face | The stint ends and counts, but the break is forfeited — choosing a face is choosing an interval |
| Flow break with nothing banked | Falls back to ten minutes, rather than a zero-second break that would beep the moment it started |
| A stint under 90 seconds | Earns nothing and counts nothing: turning the cube through the face on the way somewhere else shouldn't score |
| A stint reaching four hours | Ends itself and beeps. Left standing, the cube would otherwise hold the backlight on until the pack went flat |

## Bluetooth

The cube advertises its state as [BTHome v2](https://bthome.io/format/) —
service data under UUID `0xFCD2` — which Home Assistant discovers with no
custom component, no MQTT and no ESPHome. It is one-way and connectionless, so
it costs almost nothing next to the backlight.

Objects, in the ascending order the spec requires, totalling 30 of the 31 bytes
a legacy advertisement allows:

| Object | ID | Type | Meaning |
| --- | --- | --- | --- |
| Packet ID | `0x00` | uint8 | Increments on change, so receivers can dedupe |
| Voltage | `0x0C` | uint16, ×0.001 V | The smoothed pack voltage, uncorrected |
| Work | `0x0F` | binary | 1 for a work interval, 0 for a break |
| Connectivity | `0x19` | binary | 1 while awake, 0 in the advert sent before sleeping |
| Running | `0x27` | binary | Is the countdown advancing |
| Count | `0x3D` | uint16 | Completed work intervals since the last power cycle |
| Duration | `0x42` | uint24, ×0.001 s | Remaining, or elapsed while counting up |
| Duration 2 | `0x42` | uint24, ×0.001 s | What the timer started at, or 0 while counting up |

`Work` is a generic boolean because BTHome has no object that means "this
interval is work". It exists because flow's two faces have no fixed length to be
classified by, and it is the same value the firmware counts pomodoros from, so
the two can't disagree about which faces are work.

The device info byte sets the trigger-based flag, which tells Home Assistant
the cube advertises irregularly — without it, every normal sleep would look
like a fault.

**There are deliberately no event objects.** A BTHome event carries an event
id and no payload, so a "timer started" event couldn't say how long the
interval was; you'd read that from the duration objects anyway, and then risk
acting on the previous value if the sensors hadn't settled first. Automations
should key off `running` changing state instead, and read `work` for what kind
of interval it is. State also self-heals where an event can't: an advertisement
is an unacknowledged broadcast, so a missed event is gone for good while a
missed state is re-advertised a second later.

**`Duration 2 == 0` means the timer is counting up**, and `Duration` is then the
elapsed time rather than the remaining. Test it *before* the state table below,
which assumes a timer with a length: a flow stint at nought seconds would
otherwise read as both armed and finished, and a paused one as neither. The
sentinel is free — it needs no object of its own, and no fixed timer ever
advertises a zero length.

Five states come out of four fields, with one extra byte pair on the wire:

| State | Condition |
| --- | --- |
| Counting up | `started == 0` (test this first) |
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
02 01 06 1A 16 D2 FC 44 00 00 0C 3C 0F 0F 01 19 01 27 01 3D 00 00 42 90 DB 16 42 60 E3 16
```

## Hardware

A [Waveshare ESP32-S3-Touch-LCD-1.28](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28),
which carries everything except the beeper: the 240×240 round GC9A01 over SPI,
a QMI8658 accelerometer over I²C, a LiPo connector with an ETA6096 charger and
the 200k/100k sense divider, and USB-C for flashing. Every pin in
`ArduinoIDE_V2/main/consts.h` matches that board — including the three that
distinguish it from the otherwise similar non-touch `ESP32-S3-LCD-1.28`
(backlight on GPIO2, LCD reset on GPIO14, IMU INT1 on GPIO4).

The one added component is a **piezo beeper on GPIO15**. The board has no
sounder of its own.

The board also has a **CST816S capacitive touch controller**, on the same I²C
bus as the accelerometer, which this firmware does not use at all. It's the
obvious place to start if the cube ever needs configuring on-device rather than
by reflashing.

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
fifth a flow stint earns, the arc's fill and colour ramp, the lap indicator, the
MM:SS to HH:MM switch, the low-battery threshold, the RTC guard, and the BTHome
encoder's exact output bytes:

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
  `Util::getTimerSpec()`.
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
