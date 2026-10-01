https://github.com/user-attachments/assets/7382e785-1dbb-4c98-959f-1ba87e019f36

# pomodoro-cube

A desk cube that runs a pomodoro timer. Stand it on a face to pick the
interval, lay it face up to pause, lay it face down to switch it off. There are
no buttons — the only control is which way up it is.

![The four timer faces](https://coddingtonbear-public.s3.amazonaws.com/github/pomodoro-cube/faces.png?v=2)

Forked from [fly-robin-fly/coffee_timer](https://github.com/fly-robin-fly/coffee_timer),
which does the same trick for coffee brew times.

The enclosure is on Printables as
[Pomodoro Cube](https://www.printables.com/model/1860676-pomodoro-cube); its
source is in [cad/](cad/README.md).

## Features

- **The faces are the interface** — each face of the cube means something
  different: a 25-minute work timer, a 5-minute break, flow mode's work and its
  break, pause, and off. Nothing to configure and nothing to press: the
  accelerometer reads which face is down, and that is the whole API.
- **Flow mode, for work that doesn't fit a fixed interval** — its work face
  counts *up* for as long as the cube stands on it, banking a fifth of what it
  counts as break time. The break face spends that bank. Fifty minutes of work
  buys ten of break, which is the fixed pair flow mode replaced; an hour and a
  half buys eighteen. Every 25 minutes it counts scores a pomodoro, which is
  what the arc's lap is showing. See [Flow mode](#flow-mode).
- **Home Assistant over BLE** — the cube broadcasts its state as a
  [BTHome v2](https://bthome.io/format/) advertisement, which Home Assistant
  discovers natively: no custom component, no MQTT, no ESPHome. See
  [Bluetooth](#bluetooth).

## How it works

Which way up the cube is sitting is the whole interface. There's an
accelerometer on the board, and the firmware turns what it reports into one of
six states: the four timer faces, face up, or face down.

Standing the cube on a timer face starts that face's timer — or, if you'd
paused one on that same face, picks it back up where you left off. The
countdown faces show an arc that drains as the time runs out:

![The arc draining over a five-minute timer](https://coddingtonbear-public.s3.amazonaws.com/github/pomodoro-cube/countdown.png?v=2)

When a countdown reaches zero, the vibration motor buzzes and the whole face
flashes red until you turn the cube to another face; if you don't, it gives up
and goes to sleep after thirty seconds:

![A finished timer flashing](https://coddingtonbear-public.s3.amazonaws.com/github/pomodoro-cube/finished.gif)

Work timers count towards a running pomodoro total (the one Home Assistant
sees), and breaks don't.

Laying the cube face up pauses whatever's running, and leaves the frozen
countdown on screen in muted colours:

![Paused, and the low battery warning](https://coddingtonbear-public.s3.amazonaws.com/github/pomodoro-cube/states.png?v=2)

The cube spends most of its life in deep sleep, and any movement wakes it back
up. The paused timer, the pomodoro count and the flow bank are all kept in
memory that survives deep sleep — but not a flat battery.

### Switching off

Face down is off. Every bump wakes a sleeping cube, so "off" has to mean
something a bit stronger than "asleep"; otherwise a cube in a bag would light
up and start a timer whenever it landed on an edge. Once it's face down, the
cube stays dark however much it's jostled, and whatever was running is parked
just as if you'd paused it.

To switch it back on, flip it three times: turn it face up, then over onto
its face and back up again, twice. The screen counts the flips down — a big
**2**, then **1**, "to go" — and answers the third with a buzz and **Let's
go**. Take more than five seconds over any one flip and it goes back to sleep,
still off; the next flip starts the count again from three.

## Flow mode

Sometimes 25 minutes just isn't the right length for a stretch of work. Flow
mode's work face counts _up_ instead of down, for however long you leave the
cube on it, and every five minutes you work banks a minute of break. The flow
break face then counts that bank back down: fifty minutes of work buys you a
ten-minute break, and an hour and a half buys eighteen.

The bank is a running balance rather than a one-time handoff, so several
stretches of work add up, and if you're interrupted partway through a break,
whatever's left stays banked for next time.

Both flow faces are drawn black on white so you can tell at a glance that
you're not on a fixed timer, and the work face shows what your bank would be
if you stopped right now:

![Flow mode counting up with its bank, and the break it earned](https://coddingtonbear-public.s3.amazonaws.com/github/pomodoro-cube/flow.png)

The arc on the work face fills over 25 minutes and then starts over, counting a
pomodoro each time it comes back around.

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

`Duration 2 == 0` means the timer is counting up, and `Duration` is then the
elapsed time.

Before it goes to sleep, the cube sends one last advertisement with `running`
off, so Home Assistant doesn't go on thinking a timer is running while the cube
is asleep.

### How it reaches the air

The work is split in two, because the halves fail in different ways and only one
of them can be tested without a radio:

- `bthome.cpp` assembles the payload, and is tested against exact bytes. It also
  holds `BTHome::Sequencer`, which decides when there is anything new to say:
  the firmware offers its state on every pass of the loop and the sequencer only
  produces an advertisement when the bytes a receiver would see have changed.
  That is what keeps the packet id meaning *"something changed"* rather than
  *"another second went by"* — and it is why a pack voltage wobbling below a
  millivolt doesn't churn it, since the comparison is on encoded bytes rather
  than the float behind them.
- `ble.cpp` hands the result to NimBLE. Non-connectable, no scan response, no
  GATT server and nothing to connect to: a BTHome device is a beacon. The radio
  goes on the air in two-second bursts at 100 ms, and is quiet in between.
  `BTHome::Scheduler` decides when a burst is due: at once when anything but the
  seconds remaining or the pack voltage has changed, and otherwise every 30
  seconds as a heartbeat, carrying wherever the countdown and the pack have got
  to. Home Assistant's countdown is therefore up to 30 seconds stale, and face
  changes, pauses, finishes and pomodoros arrive at once. The farewell goes out
  at the same 100 ms and stays on the air for the whole of the shutdown sequence — the panel
  being put away, the parting buzz, a second's pause — with the controller
  shut down last, immediately before the CPU stops, and never after less than a
  second on the air. That is a dozen-odd copies of the one advertisement that
  cannot be repeated later, where a fixed 400 ms used to send four, which a
  receiver scanning at a low duty cycle (an ESPHome Bluetooth proxy listens for
  30 ms in every 320 by default) could miss altogether.

The cube appears in Home Assistant under its BLE MAC — one above the WiFi MAC
printed on the board — because the payload already uses 30 of the 31 bytes a
legacy advertisement holds and there is nowhere left to put a name.

> [!NOTE]
> Home Assistant only offers a BTHome device its Bluetooth integration has
> actually *seen*, and the cube is silent while asleep. Stand it on a face before
> adding the integration, or there will be nothing to find.

The simulator prints the payload the firmware last published when you press `a`,
and `SIM_BLE_TRACE=1` prints every one as it goes out:

```
[sim] advertisement 3 (30 bytes): 02 01 06 1A 16 D2 FC 44 00 02 0C 3C 0F 0F 01 19 01 27 01 3D 00 00 42 90 DB 16 42 60 E3 16
[sim] farewell advertisement 4 (30 bytes): 02 01 06 1A 16 D2 FC 44 00 03 0C 3C 0F 0F 01 19 00 27 00 3D 00 00 42 90 DB 16 42 60 E3 16
```

## Hardware

### Parts

- **Board**: [Waveshare ESP32-S3-Touch-LCD-1.28](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28)
- **Battery**: a 102535 Li-Po cell ([the one used here](https://www.amazon.com/dp/B0GQB1DCTW))
- **Vibration motor**: [the one used here](https://www.amazon.com/dp/B0FQTHK9LH)
- **Screws**: 4× M2.6 × 6 mm self-tapping: two inside hold the display to the
  center, two outside hold the back on
- **Enclosure**: four printed parts; see [cad/](cad/README.md) or
  [Printables](https://www.printables.com/model/1860676-pomodoro-cube)

### The board

A [Waveshare ESP32-S3-Touch-LCD-1.28](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28),
which carries everything except the vibration motor: the 240×240 round GC9A01 over SPI,
a QMI8658 accelerometer over I²C, a LiPo connector with an ETA6096 charger and
the 200k/100k sense divider, and USB-C for flashing. Every pin in
`ArduinoIDE_V2/main/consts.h` matches that board — including the three that
distinguish it from the otherwise similar non-touch `ESP32-S3-LCD-1.28`
(backlight on GPIO2, LCD reset on GPIO14, IMU INT1 on GPIO4).

The one added component is a **vibration motor on GPIO15**, driven high to run
it. That is one of the six GPIOs the board brings out on its SH1.0 connector
(15, 16, 17, 18, 21 and 33), and the pin is held low through deep sleep. A GPIO
cannot supply a motor directly, so this wants a module with its own driver
transistor. There is no sounder: everything the cube has to say, it says by
buzzing.

| When | Pattern |
| --- | --- |
| Waking onto a timer face | Two short pulses |
| Set on a different face, resting faces included | One pulse |
| A timer running out | 400 ms on, 600 ms off, until the cube is turned to another face or sleeps |

A wake that finds the cube still resting goes back to sleep without a buzz,
because a buzz is movement and movement is what wakes it. For the same reason
the motor is stopped before the wake-on-motion detector is armed, taps are
ignored while it runs, and the alarm's silences are longer than the face
debounce, so the accelerometer always gets a quiet stretch in which to notice
the cube being turned. The patterns are in `consts.h`.

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
- NimBLE-Arduino 1.4.3 (h2zero) — **not** 2.x, which needs an ESP32 core of 3.x
  and up; this project is on 2.0.14. NimBLE rather than the core's bundled
  `BLEDevice.h` because that one is Bluedroid, which costs tens of kilobytes of
  RAM the cube would rather spend on LVGL

Two of them keep their configuration outside the sketch, and both have to be
linked into place before anything will work:

```bash
ln -s "$PWD/ArduinoIDE_V2/lv_conf.h"    ~/Arduino/libraries/lv_conf.h
ln -s "$PWD/ArduinoIDE_V2/User_Setup.h" ~/Arduino/libraries/TFT_eSPI/User_Setup.h
```

`lv_conf.h` sets only the five options that differ from LVGL's defaults, and
`sim/CMakeLists.txt` sets the same five, so the simulator and the board render
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
kept despite the 16MB flash: the sketch is about 880 kB against that scheme's 1.3 MB
app slot, and nothing here uses the filesystem.

The IDE isn't needed to flash it, though — `tools/flash.sh` compiles and
uploads in one go with the `arduino-cli` the IDE ships with, passing those board
settings itself. It also builds at `-O2` rather than the core's `-Os`, which
draws the face about 14% faster; the IDE has no setting for that, so an IDE
build works the same and just draws a little slower:

```bash
tools/flash.sh              # compile, upload, reset
tools/flash.sh --build-only # just check it compiles
tools/flash.sh --monitor    # ...and then tail UART0 at 115200
```

It checks the two symlinks above before compiling, loads `cdc_acm` if
`/dev/ttyACM0` is missing (the module is blacklisted on some machines, so the
port doesn't appear on its own), and resets the board a second time after
uploading — straight out of an upload the QMI8658 has come up silent, with
every read returning zero, and a second reset clears it.

`ArduinoIDE_V2/main/src/` began life as SquareLine Studio output and is now
maintained by hand. **Re-exporting from `SquareLine/coffee_timer.spj` would
overwrite it**, countdown font included.

## Developing without the hardware

```bash
./sim/run.sh
```

This compiles the real firmware for a Linux desktop and draws LVGL into an SDL
window instead of a GC9A01 over SPI. It isn't a reimplementation: `main.ino`,
`display.cpp`, `util.cpp`, `indicators.cpp`, `rtc_state.cpp`, `tilt.cpp`,
`bthome.cpp`, `battery.cpp`, `haptic.cpp` and `haptic_pattern.cpp` all compile
exactly as they ship, against fake `Arduino.h`, `TFT_eSPI`, `Wire` and
`driver/rtc_io` headers. Only `qmi.cpp` and `ble.cpp` are replaced, because they
need the vendor IMU driver and NimBLE.

RTC memory is modelled rather than faked — the block survives a simulated deep
sleep, and a cold boot fills it with junk rather than zeroes, so the magic-word
guard is genuinely exercised.

See [sim/README.md](sim/README.md) for the controls, the scripted-screenshot
environment variables, and what the simulator can't tell you.

Host tests cover the parts that are pure logic — the face-to-timer mapping, the
flow bank's arithmetic, the arc's fill and colour ramp, the lap indicator
and what scores off it, the MM:SS to HH:MM switch, the low-battery threshold,
the angle the face is drawn at, the RTC guard, the vibration patterns, the
order the sleep path shuts things down in, and the BTHome encoder's exact output
bytes:

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

## Layout

```
ArduinoIDE_V2/lv_conf.h   LVGL settings, linked into ~/Arduino/libraries
ArduinoIDE_V2/User_Setup.h TFT_eSPI panel settings, linked into the library
ArduinoIDE_V2/main/       the firmware, built with the Arduino IDE
ArduinoIDE_V2/main/src/   the LVGL UI, originally SquareLine Studio output
SquareLine/               the SquareLine Studio project the UI came from
cad/                      the enclosure, exported from Fusion 360; see cad/README.md
fonts/                    source faces for the countdown label
sim/                      desktop simulator and host tests
tools/                    flashing the board, and font conversion
```
