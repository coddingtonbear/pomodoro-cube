# pomodoro-cube

A desk cube that runs a pomodoro timer. Stand it on a face to pick the
interval, lay it face up to pause, lay it face down to switch it off. There are
no buttons — the only control is which way up it is.

![The four timer faces](https://coddingtonbear-public.s3.amazonaws.com/github/pomodoro-cube/faces.png)

Forked from [fly-robin-fly/coffee_timer](https://github.com/fly-robin-fly/coffee_timer),
which does the same trick for coffee brew times.

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
- **Pause by laying it face up** — the remaining time is kept and the panel is
  left showing the frozen countdown in muted colours, dimmed. The cube stays
  awake while it is paused, so standing it back on the same face carries on at
  once, with nothing to wake. A *different* face is taken as choosing a different
  interval, so the pause is abandoned.
- **Face down means off** — the panel blanks and the cube stays dark however
  much it is jostled, so it can go in a bag. Whatever was running is parked,
  exactly as face up parks it. To switch it back on, lay it face up and
  shake it within five seconds. See [Switching off](#switching-off).
- **Tap it to read it** — the backlight runs at a fifth of full brightness
  between the moments worth lighting, and a tap on the glass buys ten seconds
  at full. The QMI8658 detects the tap itself, so it works on a face that never
  brightens on its own, flow's especially. See [Brightness](#brightness).
- **A forgotten pause goes to sleep.** After half an hour the CPU stops and
  the frame is left lit: the GC9A01 refreshes itself from its own memory, so
  only the backlight draws current. Moving the cube wakes it, paused and awake
  again if it is still face up.
- **A battery indicator that stays out of the way** — nothing on screen at all
  above 3.6 V, and below it a red empty-battery outline with the measured pack
  voltage inside, so the divider and ADC can be checked against a multimeter.
- **Home Assistant over BLE** — the cube broadcasts its state as a
  [BTHome v2](https://bthome.io/format/) advertisement, which Home Assistant
  discovers natively: no custom component, no MQTT, no ESPHome. See
  [Bluetooth](#bluetooth).

## How it works

Orientation is the whole interface. A QMI8658 accelerometer reports which way
gravity points, `Util::calcOrientation()` turns that into one of six states —
four upright faces plus face up and face down — and a 300 ms debounce keeps a
cube mid-flip from starting a timer it doesn't mean.

The debounce measures how long one reading has *held still*, not how long it is
since the last one that agreed with the face being left. The difference matters
because a cube in a hand passes through faces on its way to the one it is being
put down on: picking it up off its face-up rest and standing it on a timer face
reads face-up somewhere in the middle, and a debounce timed the other way would
take that in passing as a decision to set the cube down again — leaving the
paused frame on a panel that should have gone back to work.

Waking runs the same debounce. The interrupt that wakes the cube fires because
it moved, so the first readings after one are taken in mid-air as often as not;
the firmware waits for one to settle, up to three seconds, before deciding
whether it has been set down or stood up. Guessing wrong towards sleep is the
expensive mistake — a cube that sleeps on a face it is no longer on has nothing
left to wake it.

Which way up the face is *drawn* is a separate question from which face the
cube is on, and is answered separately. `Tilt::Tracker` follows the angle of
gravity in the plane of the screen on every pass of the loop, smooths it, and
hands it to `Display::setAngle()`, so the picture turns as the cube does and a
cube held at 45° shows a face at 45°. The nearest quarter turn is done by the
GC9A01 itself, which costs nothing; only what is left over is drawn at an angle
by LVGL, in software, and without anti-aliasing — the turn samples the nearest
pixel rather than blending four, which is most of what lets a leaning face keep
up with the hand. Within 8° of a quarter turn the face is drawn exactly
square, and it takes 12° to pull it off again, so a cube at rest has nothing
left over and draws as cheaply as it would if none of this existed. Laid on its
back the cube has no angle to follow, and the face stays where it was.

Standing the cube on a face starts that face's timer, unless there's a paused
one belonging to that same face, in which case it picks up where it left off.
Three of the faces count down and the arc drains as they run; the fourth is
[flow mode](#flow-mode), which counts up:

![The arc draining over a five-minute timer](https://coddingtonbear-public.s3.amazonaws.com/github/pomodoro-cube/countdown.png)

When the timer reaches zero the vibration motor buzzes on and off, the whole
face flashes red, and both go on until the cube is turned to another face. Left
alone, it puts itself to sleep after thirty seconds.
Work counts towards a pomodoro total and breaks do not — the fixed work face
when it reaches zero, and flow's a lap at a time as it runs.

![Paused, and the low battery warning](https://coddingtonbear-public.s3.amazonaws.com/github/pomodoro-cube/states.png)

A paused timer and the pomodoro count live in RTC memory, which survives deep
sleep but not a flat battery. Because a cold boot leaves arbitrary bits there,
the block carries a magic word; anything that doesn't match it is discarded
rather than resumed as a plausible-looking timer.

### Brightness

The backlight is the largest single draw while the cube is awake, of the same
order as the whole rest of the board, so full brightness is rationed rather than
held. It is spent on the moments worth looking at — the five seconds after the
cube is set on a face, and the last five seconds of a countdown, which is also
what keeps a finished timer lit while it buzzes. Everything between sits at 20%,
which is legible across a desk for a fraction of the current, and swaps to a
filled-colour scheme that reads at that brightness where a thin arc does not.

A tap on the glass buys ten seconds at full brightness and nothing else: the
faces are what choose a timer, and tapping never changes one. Longer than a face
change is worth, because a tap is someone coming to the cube cold rather than
someone already looking at it. The QMI8658 has a tap detector of its own, which
is what makes this work — a tap is over in a couple of milliseconds, so the
50 Hz polling loop would miss almost every one if it had to find them in the
accelerometer stream itself. The sensor latches the event and the loop reads it
out. Single and double taps both brighten it; `main.ino`'s loop is where to
narrow that to `QMI::Tap::Double` if the cube turns out to brighten at things
that were not taps.

The one face this really matters for is flow's. A countdown brightens on its own
as it runs out, but a stint has no end to approach, so before this the only way
to read one that had settled was to pick the cube up — which ends the stint.

### Switching off

There are no buttons, so off has to be a way of setting the cube down, and it
has to survive being carried. Every bump wakes a sleeping cube — that is how
standing it on a face starts a timer — so off can't just be a sleep, or a cube
in a bag would light up and start counting whenever it landed on an edge.

Laying the cube face down switches it off, however it gets there: set down
face down while it is running, or turned over while it sleeps face up. It parks
whatever was running, as face up does, blanks the panel and notes in RTC memory
that it is off.

A wake that finds that note listens for the gesture that turns it back on
instead of settling and deciding. It has to, because the first thing a hand
reaching for a face-down cube does is lift it, which wakes it while it is still
face down. Deciding then went back to sleep, and finished going to sleep while
the cube was being turned over. The turn woke nothing, and the double tap was
spent waking the cube instead of switching it on.

So from the moment it wakes, the cube waits for itself to come to rest face up.
Standing it on a timer face does nothing. Once it is lying face up it shows
**Shake to start** in white on black and gives you five seconds to shake it,
picked up or where it lies. A double tap on the glass does as well. Either
only counts from then on, so the flip and the landing can't be taken for them.

A shake is read off the accelerometer: five readings at least half a g more or
less than gravity within a second, which handling the cube does not reach. The tap engine is no use for it, because what it
listens for is one sharp jolt that dies away. The price is that a cube which
comes to rest face up in a bag, and is then jostled, switches on.

Either in that window switches it on with a buzz and shows **Let's go**, black
on white, for a second and a half. Then the paused face comes up: the timer
that was parked, or with nothing parked a full 25:00 that has not started.
Standing it on a face starts a timer as usual, while the message is up or
after.

The taps are detected by the QMI8658's own tap engine, with its peak threshold
lowered from the datasheet's example of 0.8 g² to 0.15 g². On a cube lying on a
hard desk the example let through only about one tap in six, and never both
taps of a double tap. The lower threshold also makes the tap that brightens the
panel easier to trigger. If the cube is left still on
any other face for a second and a half, or never settles within five seconds,
it goes back to sleep still off. Those wakes, which are what a bag causes,
never light the panel and never use the radio. A double tap that misses the
window is movement, so it wakes the cube and opens a new window.

The sleep that ends an unanswered alarm doesn't switch the cube off; only face
down does.

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
the break face spends it, and only a flat battery clears it.

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
| A stint reaching four hours | Ends itself and buzzes, nine laps scored and 48 minutes credited. Left standing, the cube would otherwise hold the backlight on until the pack went flat |
| The bank reaching four hours | Capped there, for the same reason a stint is |
| Flow break with an empty bank | Finishes on the spot: `00:00` and the finish pattern |
| Face down | Off, with the stint parked and the bank kept, as face up keeps them |

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

**A farewell advert goes out before every sleep**, with `connectivity` *and*
`running` dropped to 0, because Home Assistant holds the last state it heard —
otherwise a busy light keyed on `running` would stay lit until the cube was next
picked up. Why it slept is inferable from the rest of the payload:
`remaining == 0` is a normal finish, a low voltage is a flat pack, and anything
else is the cube being put away.

`running` has to go for a second reason: a pause that has gone to sleep is
still a pause. State 5 below needs `running` false, which a cube paused awake
advertises for itself, and the farewell goes on saying once it sleeps.

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
  GATT server and nothing to connect to: a BTHome device is a beacon. Adverts go
  out every 300 ms while the cube is awake, which is under a tenth of a milliamp
  against twenty-odd for the backlight at its dimmest. The farewell goes out at
  100 ms and stays on the air for the whole of the shutdown sequence — the panel
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

## Unverified

Things still to be checked on the hardware, collected so they can be done in
one sitting:

- **Which rotation direction is clockwise.** `DEG_0` → `DEG_90` → `DEG_180` →
  `DEG_270` is one consistent direction, but whether it's clockwise depends on
  the accelerometer's sign convention. If it runs backwards, swap two cases in
  `Util::getTimerSpec()`.
- **`BAT_FULL_VOLTAGE` was removed, but `BAT_EMPTY_VOLTAGE` and
  `LOW_BATTERY_VOLTAGE` are still guesses** inherited from upstream. They
  calibrate the whole measurement chain — divider, ADC, cell — not just the
  cell, so they should only be retuned against real readings.
- **The vibration patterns.** The pulse lengths and `HAPTIC_SETTLE_MS`, the
  allowance for the motor spinning down, were written with no motor on the
  board.
- **Whether the motor shakes the face.** The tilt tracker takes every reading
  that passes for gravity, including those taken while the motor runs. If an
  alarm makes the picture twitch, feed it `!Haptic::disturbing()` as well.
- **Whether the tilt constants suit a hand.** `TILT_SENSE_SMOOTHING_MS` and
  `TILT_EASE_MS` were chosen in the simulator, where the cube turns in no time
  at all.
- **Whether the tap and shake thresholds suit the finished cube.** The tap
  peak was lowered to 0.15 g² on the bare board; the quiet floor is still the
  datasheet's 0.4 g², and neither has been tried in a printed enclosure. Too
  deaf and taps go unnoticed; too keen and the panel lights when the desk is
  knocked. The shake (`SHAKE_*` in `consts.h`) hasn't been tuned in the hand.
- **Whether the farewell advert really escapes.** Verified in the simulator, and
  the awake adverts are confirmed on hardware, but the farewell is the one that
  races the CPU stopping. Lay a running cube down and watch whether Home
  Assistant's `running` goes false rather than going stale.
- **Whether the BLE MAC survives a reflash and a flat battery.** It is derived
  from the eFused base MAC, so it should, but Home Assistant keys the device on
  it and a change would silently orphan the entities.

## Layout

```
ArduinoIDE_V2/lv_conf.h   LVGL settings, linked into ~/Arduino/libraries
ArduinoIDE_V2/User_Setup.h TFT_eSPI panel settings, linked into the library
ArduinoIDE_V2/main/       the firmware, built with the Arduino IDE
ArduinoIDE_V2/main/src/   the LVGL UI, originally SquareLine Studio output
SquareLine/               the SquareLine Studio project the UI came from
cad/                      the parametric enclosure; see cad/README.md
fonts/                    source faces for the countdown label
sim/                      desktop simulator and host tests
tools/                    flashing the board, and font conversion
```
