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
  as long as the cube stands on it, banking a fifth of what it counts as break
  time. The break face spends that bank. Fifty minutes of work buys ten of
  break, which is the fixed pair flow mode replaced; an hour and a half buys
  eighteen. Every 25 minutes it counts scores a pomodoro, which is what the
  arc's lap is showing. See [Flow mode](#flow-mode).
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
Work counts towards a pomodoro total and breaks do not — the fixed work face
when it reaches zero, and flow's a lap at a time as it runs.

![Paused, and the low battery warning](https://coddingtonbear-public.s3.amazonaws.com/github/pomodoro-cube/states.png)

A paused timer and the pomodoro count live in RTC memory, which survives deep
sleep but not a flat battery. Because a cold boot leaves arbitrary bits there,
the block carries a magic word; anything that doesn't match it is discarded
rather than resumed as a plausible-looking timer.

## Flow mode

The third face counts up instead of down, for a stretch of work that doesn't
fit a fixed interval. A fifth of what it counts goes into a **bank** of break
time, and the fourth face spends that bank.

The bank is a balance rather than a handoff, which is what makes several spells
of work add up and an interrupted break keep its remainder:

| | Bank |
| --- | --- |
| 25 minutes of flow work | **5:00** — a fifth of 25 credited |
| Turn to the break face | 5:00 on the clock, counting down |
| A minute of it taken, then back to flow work | **4:00** — what was left stays banked |
| Another 25 minutes of flow work | **9:00** — 4 left over plus 5 newly earned |

A running break writes the new balance back every second, so whatever
interrupts it — another face, a pause, a flat battery — leaves the rest still
banked. Working on the 25-minute face doesn't cost you the bank either: only
the break face spends it, and only laying the cube face down clears it.

Turn to the break face with nothing banked and you get a break of no length:
`00:00`, the finish pattern, and the usual half minute before the cube sleeps.
There is nothing to fall back on, because any fallback would be break time
nobody worked for. For the same reason there is no minimum on a break either —
the bank is an account, and what it says you have is what you get, down to
twelve seconds.

**Both flow faces wear an inverted panel** — black on white — because a number
going up looks exactly like one going down, and because the break face is
spending the bank just as much as the work face is filling it. The two flow
faces belong together and the two fixed ones belong together:

![Flow mode counting up with its bank, and the break it earned](https://coddingtonbear-public.s3.amazonaws.com/github/pomodoro-cube/flow.png)

The work face carries the bank above the counter in small type — `BANK 4:40`.
It shows what the bank would be worth **if the stint ended now**, so it climbs a
second for every five worked rather than sitting at the last committed figure:
the question you are asking when you glance at it is what turning the cube over
would give you. The break face has no such label, because there the big number
*is* the bank, counting down.

Counting up has no total for the arc to drain against, so the arc becomes a lap
indicator instead: it fills over 25 minutes, shading green to red as the lap
ages, then starts again. Flow's arc runs a step darker at every stop than the
fixed faces' — those colours were picked against black, and amber on white is
all but invisible. **Each lap that closes scores a pomodoro** — the arc
coming back round is the cube saying so — which is why the lap is 25 minutes and
not some other number. Two hours of flow is four pomodoros, counted as they
happen rather than totted up when the stint ends, so the count reaches Home
Assistant while you are still working.

A stint's last partial lap scores nothing, the same way abandoning a 25-minute
timer at 24:00 scores nothing. It still credits the bank, though: a fifth of the
whole stint goes in, partial lap included. How much break you have banked and
how many pomodoros you did are different questions.

Past an hour the label runs out of digits for MM:SS and switches to HH:MM, with
a small `h:m` marker underneath saying so — the two are otherwise
indistinguishable.

The rest of the rules:

| Situation | What happens |
| --- | --- |
| Flow work → face up | Parked, not ended: standing the cube back on that face carries on counting up, and nothing is credited until it does end |
| Flow work → any other timer face | The stint ends and credits the bank. Laps already scored are kept |
| Flow break → face up | Parked. The bank already holds the remainder, so abandoning the pause loses nothing |
| A stint reaching four hours | Ends itself and beeps, nine laps scored and 48 minutes credited. Left standing, the cube would otherwise hold the backlight on until the pack went flat |
| The bank reaching four hours | Capped there, for the same reason a stint is |
| Flow break with an empty bank | Finishes on the spot: `00:00` and the finish pattern |
| Face down | Off, and the bank is cleared with everything else |

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
| Count | `0x3D` | uint16 | Pomodoros since the last power cycle: completed work timers, plus a flow lap each 25 minutes |
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

**`Duration 2 == 0` together with `work` means the timer is counting up**, and
`Duration` is then the elapsed time rather than the remaining. The `work` half
of that matters: a *break* of zero length is a real state too — it is what the
flow break face shows when the bank is empty — and the work flag is the only
thing telling the two apart, since flow's work face is the one face that ever
counts up. The sentinel is free either way: it needs no object of its own, and
no timer with a length ever advertises a zero one.

Five states come out of four fields, with one extra byte pair on the wire.
**First match wins**, and the order matters: a stint at nought seconds would
otherwise read as armed, and an empty-bank break as both armed and finished.

| | State | Condition |
| --- | --- | --- |
| 1 | Counting up | `started == 0` and `work` |
| 2 | Finished | `remaining == 0` |
| 3 | Running | `running` |
| 4 | Armed | `not running`, `remaining == started` |
| 5 | Paused | `not running`, `0 < remaining < started` |

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

- TFT_eSPI 2.5.43 (Bodmer)
- lvgl 8.3.11
- SensorLib 0.4.1 (Lewis He) — the library that provides `SensorQMI8658.hpp`.
  Searching the Library Manager for the header's name finds other people's
  QMI8658 libraries instead, none of which are this one

Two of them keep their configuration outside the sketch, and both have to be
linked into place before anything will work:

```bash
ln -s "$PWD/ArduinoIDE_V2/lv_conf.h"    ~/Arduino/libraries/lv_conf.h
ln -s "$PWD/ArduinoIDE_V2/User_Setup.h" ~/Arduino/libraries/TFT_eSPI/User_Setup.h
```

`lv_conf.h` sets only the three options that differ from LVGL's defaults, and
`sim/CMakeLists.txt` sets the same three, so the simulator and the board render
from identical settings. LVGL looks for it one directory *above* itself, which
is why it cannot live in the sketch folder.

`User_Setup.h` is the panel: GC9A01, 240×240, and the SPI pins. TFT_eSPI does
support a `tft_setup.h` in the sketch folder and this project used to rely on
it, but **that mechanism cannot work under the Arduino build**: TFT_eSPI.h
looks for `<tft_setup.h>` on the include path, and the sketch folder is not on
the path when the build compiles the library itself. The sketch's own files
find it and the library's do not, so the library compiles against its default
— an ILI9341 on entirely different pins — and the panel stays black with
nothing anywhere to say why. Overriding `User_Setup.h` configures every
translation unit alike. `display.cpp` now refuses to compile if `GC9A01_DRIVER`
is undefined, so a missing symlink is a build error rather than a dead screen.

Board settings, matching the ESP32-S3-WROOM-1 the Waveshare board carries:
**ESP32S3 Dev Module**, flash size **16MB**, PSRAM **disabled** (this module
has none). USB CDC on boot stays **disabled** — the USB-C port is a CH343
UART bridge rather than the S3's native USB, so `Serial` is UART0 either way
and the port appears as `/dev/ttyACM0`. The default 4MB partition scheme is
kept despite the 16MB flash: the sketch is 586 kB against that scheme's 1.3 MB
app slot, and nothing here uses the filesystem.

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
flow bank's arithmetic, the arc's fill and colour ramp, the lap indicator
and what scores off it, the MM:SS to HH:MM switch, the low-battery threshold,
the RTC guard, and the BTHome encoder's exact output bytes:

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
ArduinoIDE_V2/lv_conf.h   LVGL settings, linked into ~/Arduino/libraries
ArduinoIDE_V2/User_Setup.h TFT_eSPI panel settings, linked into the library
ArduinoIDE_V2/main/       the firmware, built with the Arduino IDE
ArduinoIDE_V2/main/src/   the LVGL UI, originally SquareLine Studio output
SquareLine/               the SquareLine Studio project the UI came from
fonts/                    source faces for the countdown label
sim/                      desktop simulator and host tests
tools/                    font conversion
```
