#pragma once
#include <stdint.h>

#include "ui_colors.h"

#define BAT_ADC_PIN 1
#define TFT_BL_PIN 2
#define IMU_INT_PIN GPIO_NUM_4
#define I2C_SDA_PIN 6
#define I2C_SCL_PIN 7
// The vibration motor's driver input: high runs the motor. One of the six GPIOs
// the board brings out on its SH1.0 connector (15, 16, 17, 18, 21 and 33). This
// one because it is in the RTC domain, so its level can be held through deep
// sleep, and is not a strapping pin, so a motor hanging off it cannot change
// how the board boots.
#define HAPTIC_PIN 15

#define ORI_DEBOUNCE_DELAY 300

// How many accelerometer reads in a row have to fail before the I2C bus and
// the QMI8658 are recovered and set up again (see QMI::getAccelerometer), and
// how long to leave between attempts while it stays silent. 25 is half a second
// of 20 ms passes: no glitch lasts that long, and no face change is missed for it.
constexpr unsigned QMI_RECOVER_AFTER_FAILURES = 25;
constexpr unsigned long QMI_RECOVER_INTERVAL_MS = 5000;

// A reading only says which way is down when it is gravity and very little
// else. Outside this band the cube is being accelerated -- carried, tapped, set
// down hard -- or the QMI8658 is still coming up after being configured, which
// it does by emitting a few samples of nonsense. Measured on the board: at rest
// the magnitude sits at 1.01 g and barely moves; the rejected samples in a
// fifteen-minute trace ran from 0.00 to 2.64 g, so nothing near the edges of
// this band is a real attitude.
constexpr float ORI_GRAVITY_MIN_G = 0.85f;
constexpr float ORI_GRAVITY_MAX_G = 1.15f;

// How far the dominant axis has to lead the runner-up before a reading is
// allowed to move the cube onto a *new* face. An attitude poised between two
// faces goes on confirming the face the cube is already believed to be on, and
// is never enough to move it, so a cube resting near the halfway point does not
// alternate between the two.
constexpr float ORI_DOMINANCE_MARGIN_G = 0.10f;

// The face is drawn upright against gravity at whatever angle the cube is held,
// rather than at the nearest quarter turn. That needs some of gravity to lie in
// the plane of the screen: below this much, the cube is close enough to flat
// that the angle is mostly noise, and the face stays where it last was.
constexpr float TILT_MIN_INPLANE_G = 0.30f;

// Within TILT_SNAP_CAPTURE_DEG of a quarter turn the face is drawn at exactly
// that quarter turn, and stays there until the cube is more than
// TILT_SNAP_RELEASE_DEG off it. A cube stood on a face is never quite square to
// gravity -- the desk is not level and neither is the print -- and the gap
// between the two figures is what stops one resting near the edge of the first
// from flickering in and out of it. Measured on the board, a reading at rest
// wanders by about a degree and a half, which both are well clear of.
constexpr float TILT_SNAP_CAPTURE_DEG = 8.0f;
constexpr float TILT_SNAP_RELEASE_DEG = 12.0f;

// Two time constants, in milliseconds. The first smooths the readings, which
// is what keeps a hand's unsteadiness out of the angle. The second eases the
// face towards wherever the readings point, which is what turns the jumps --
// snapping onto a quarter turn, letting go of one -- into movement. Neither has
// been tuned against a cube in the hand.
constexpr int TILT_SENSE_SMOOTHING_MS = 50;
constexpr int TILT_EASE_MS = 70;

// How far the angle has to move before the face is drawn again. Every redraw
// at an angle is the whole panel sent over SPI, so movement too small to see
// is not worth one.
constexpr float TILT_REDRAW_STEP_DEG = 1.0f;

// How much movement wakes a sleeping cube, in milli-g, as the QMI8658's
// wake-on-motion detector counts it. The vendor default of 200 mg wanted a
// deliberate knock: picking the cube up and standing it on a face often failed
// to reach it, so the cube sat there still showing what it was showing when it
// was put down, and had to be tapped awake.
//
// Set from the noise floor measured on the board rather than picked: at rest
// the sample-to-sample movement averages 5 mg and its 99th percentile is 24 mg,
// so this is comfortably clear of a cube sitting still on a desk while being
// three times more sensitive than the default. A spurious wake is cheap now in
// a way it was not before -- the settling below sends a cube that has not
// actually moved back to sleep in about a third of a second.
constexpr uint8_t WAKE_ON_MOTION_THRESHOLD_MG = 64;

// How long a wake will wait for the cube to stop moving before deciding what it
// is resting on. The interrupt that wakes the cube fires *because* it moved, so
// the first readings after one are taken in mid-air as often as not. Past this
// the cube is plainly still in a hand, and the firmware boots rather than
// guessing -- booting a cube that was only being carried costs a few seconds of
// backlight, where sleeping one that was being set down costs the wrong face on
// screen until something moves it again.
constexpr unsigned long WAKE_SETTLE_TIMEOUT_MS = 3000;

// What switches a switched-off cube back on: flipped over this many times,
// each flip ending face up. Face down is off, and off has to survive a bag --
// every bump wakes the cube, so the wake alone cannot be what switches it on,
// and neither can anything a bag might do once or twice by chance. A face-up
// rest followed by a shake or a double tap was the gesture before this, and
// was too easy to make by accident.
//
// The cube was put to sleep face down, so the first flip is only the turn face
// up; each one after it is over and back. The screen faces the desk for half
// of every flip but the first, which is why only the arrivals face up count:
// they are the only moments the count on the glass can be read.
constexpr int SWITCH_ON_FLIPS = 3;

// How long each flip may take. The first is timed from the wake and each after
// it from the flip before, so a cube left alone partway through goes back to
// sleep -- still off, and starting from three again the next time it moves.
//
// The listening has to start at the wake rather than once the cube has settled:
// the first thing a hand reaching for a face-down cube does is lift it, which
// wakes it while it is still face down. Settling first, as on the hardware the
// first time round, read that as "still face down", went back to sleep, and
// finished going to sleep while the cube was being turned over.
constexpr unsigned long SWITCH_ON_FLIP_WINDOW_MS = 5000;

// What counts as the cube being on one side or the other of a flip: Z carrying
// at least this much of a reading whose size is within the band below of one
// g, for at least FLIP_HOLD_MS. Deliberately looser than the orientation
// debouncer, which wants a face held dead still for ORI_DEBOUNCE_DELAY: a cube
// flipped in the hand is never quite level or quite still, and making it pause
// on every face would make the gesture a chore. The band and the hold are what
// keep a shake from passing for a flip -- shaking a face-up cube hard enough to
// throw Z past +0.7 g is well outside the band, and over in a reading or two.
constexpr float FLIP_SIDE_MIN_G = 0.7f;
constexpr float FLIP_MAGNITUDE_MIN_G = 0.6f;
constexpr float FLIP_MAGNITUDE_MAX_G = 1.4f;
constexpr unsigned long FLIP_HOLD_MS = 100;

// The shortest time between the starts of two passes of loop(). Passes that
// take longer -- drawing a face at an angle, mostly -- are not padded further.
constexpr uint32_t LOOP_PASS_MS = 20;

// The shortest time between passes while nothing on the cube needs a fast
// loop: the panel dimmed, the motor quiet, no alarm, and the cube still on the
// face it is believed to be on. That is nearly all of a running timer, where
// the only thing that happens is the seconds going down, and every pass is the
// accelerometer read over I2C and the rest of the loop run for nothing. A face
// change is still seen within the debounce plus one of these, and the first
// reading that shows the cube moving puts the loop back at LOOP_PASS_MS. The
// wait is also cut short to land on the next tick, so the seconds step evenly
// however long a pass is allowed to be.
constexpr uint32_t LOOP_IDLE_PASS_MS = 200;

// How long the loop stays fast after the last reading that showed the cube
// moving, so that the ease of a tilt and the debounce of a face both finish at
// the rate they were tuned at rather than being cut to a few samples.
constexpr unsigned long LOOP_STILL_HOLD_MS = 1000;

// The shortest wait worth light-sleeping through rather than spinning in
// delay(): going in and coming out again costs about a millisecond each way.
constexpr uint32_t NAP_MIN_MS = 5;

// How long a woken, switched-off cube will sit still on any face but face up,
// before its first flip, before giving up and going back to sleep. What cuts
// short the wakes a bag causes, each of which would otherwise keep the CPU up
// for the whole window; long enough that the pause as a hand takes hold of a
// face-down cube, before turning it, is not taken for it having been left
// alone. Once a flip has been made the cube is plainly in a hand, and lies face
// down between the rest of them, so from there only the flip window applies.
constexpr unsigned long SWITCH_ON_GIVE_UP_MS = 1500;

// How long a cube just switched on says so before the paused face comes up:
// the timer that was parked, or a work timer not yet started.
constexpr unsigned long SWITCHED_ON_MESSAGE_MS = 1500;

// Timer length for each face the cube can rest on, in seconds. DEG_0 is the
// default orientation and each step from there is a quarter turn clockwise,
// matching the order of the Orientation enum below.
constexpr int TIMER_WORK_SECONDS = 25 * 60;
constexpr int TIMER_SHORT_BREAK_SECONDS = 5 * 60;

// DEG_180 and DEG_270 are flow mode's pair, and carry no fixed length. Flow work
// counts up and credits a fifth of what it counted to a bank of break time; the
// break face counts that bank down. The bank persists across stints, so several
// spells of work accumulate, and an unspent break goes back in. It also survives
// the cube being set down on either resting face: a balance is not forfeited by
// putting the thing away. Only a power cycle empties it, because RTC memory is
// all that holds it.
constexpr int FLOW_BREAK_DIVISOR = 5;

// There is no floor on a break and nothing to fall back on when the bank is
// empty. The bank is an account: what it says you have is what you get, and a
// break face turned to with nothing in it finishes at 00:00 on the spot.

// A lap of the flow arc, which is also what a stint scores a pomodoro for: the
// arc fills over this long, scores, and starts again. Equal to the fixed work
// face by design -- a pomodoro is a pomodoro however it was counted.
constexpr int FLOW_LAP_SECONDS = TIMER_WORK_SECONDS;

// A stint has no ceiling: it runs until the cube is turned off the face, or
// until the pack goes flat. Between laps the panel sits at the idle level, so
// nothing is lit at full for the length of it. Past about four and a half hours
// the advertisement's uint24 of milliseconds pins at its maximum rather than
// wrapping, and the clock reads hours and minutes up to 99h59.

// The backlight is the largest single draw while the cube is awake -- of the
// same order as the whole rest of the board -- so full brightness is rationed
// rather than held. It is spent on the moments worth looking at: just after the
// cube is set on a face, and as a countdown runs out. Everything between sits at
// BACKLIGHT_IDLE_PERCENT, which is legible across a desk for a fraction of the
// current.
constexpr int BACKLIGHT_FULL_PERCENT = 100;
//
// Chosen by eye on the cube: 20 at first, then 2 from a trial of 20, 10, 5
// and 2, which turned out a little too dark in use.
constexpr int BACKLIGHT_IDLE_PERCENT = 4;

// The CPU clock to run at while the panel is lit at full, and otherwise. Idle
// at 80 MHz is a saving worth keeping, but at 80 MHz a face drawn at an angle
// takes 89 ms to draw, which is 9 frames a second; at 240 it takes 37 and
// the face follows a tilt at 17. Full brightness is when someone is looking,
// and it is rationed to seconds at a time, so that is when the clock goes up.
constexpr uint32_t CPU_MHZ_LIT = 240;
constexpr uint32_t CPU_MHZ_IDLE = 80;

// How long either side of a transition counts as worth looking at. Applied to
// both ends: this many seconds after a face change, and the last this many
// seconds of a countdown.
constexpr int BACKLIGHT_ATTENTION_SECONDS = 5;

// How long either side of a flow lap closing the panel lights for. A lap
// closing is a pomodoro scored, and the arc snapping back round in a new
// colour is the thing to see -- so the window straddles the moment rather than
// leading up to it, as a countdown's does. Milliseconds, because a whole
// second either side is noticeably less of a moment.
constexpr unsigned long BACKLIGHT_LAP_MS = 2500;

// How long a tap holds the panel at full brightness. Longer than a face change
// is worth, because a tap is someone asking to read the thing rather than
// someone having just set it down and already looking at it.
constexpr int BACKLIGHT_TAP_SECONDS = 10;

// How long a cube paused face up stays awake before it goes to sleep holding
// the frame. Awake is what makes leaving the pause seamless: the loop is
// already watching the faces, where a sleeping cube has to be woken by the
// movement, settle, and -- if it settles while still face up in the hand -- is
// busy going back to sleep for the second in which it is stood on its face.
// Awake and dimmed draws about as much as asleep with the backlight held at
// full, which is the only level a sleeping cube can hold it at, so the limit
// is there for the pause that was forgotten rather than for the pack.
constexpr unsigned long PAUSE_AWAKE_MS = 30UL * 60UL * 1000UL;

// Every arc ramp starts at its first colour with the interval whole, reaches
// amber at ARC_MID_PERCENT left -- green to cyan has no amber and passes
// straight through -- and arrives at its last colour at ARC_LOW_PERCENT,
// holding there to the end.
constexpr int ARC_MID_PERCENT = 50;
constexpr int ARC_LOW_PERCENT = 25;

// The battery warning is hidden entirely above this pack voltage. Expressed in
// volts rather than as a percentage because the percentage was a linear fiction
// over a range that has never been checked against real hardware, and because
// the warning shows the measured voltage for exactly that calibration job.
constexpr float LOW_BATTERY_VOLTAGE = 3.3f;

// A finished timer flashes the whole face rather than pulsing the arc, so it
// cannot be mistaken for a running one or missed from across a room. Fast
// enough to read as an alarm rather than a slow breath.
constexpr int ALERT_FLASH_MS = 250;

// The vibration motor's patterns, as milliseconds alternating motor on and
// motor off, starting with on.
//
// Waking is two short pulses and a face change is one, so the two can be told
// apart in the hand. A finished timer repeats its pattern until the cube is
// moved onto another face or puts itself to sleep.
constexpr int HAPTIC_WAKE_MS[] = { 120, 100, 120 };
constexpr int HAPTIC_FACE_CHANGE_MS[] = { 150 };
constexpr int HAPTIC_ALARM_MS[] = { 400, 600 };

// How long after the motor stops the accelerometer is still taken to be
// shaking. An allowance for the motor spinning down, not a measurement: there
// was no motor on the board when this was written.
constexpr int HAPTIC_SETTLE_MS = 150;

// The alarm shakes the same accelerometer that has to notice the cube being
// turned to silence it, and a reading taken while the motor runs may not pass
// for gravity. Its silence therefore has to be long enough, once the motor has
// spun down, for a new face to sit out the whole debounce.
static_assert(HAPTIC_ALARM_MS[1] >= ORI_DEBOUNCE_DELAY + HAPTIC_SETTLE_MS + 100,
              "the alarm's off period leaves no room to debounce a face change");

enum class Orientation {
  // Both resting faces put the cube to sleep and park whatever was running. The
  // only difference between them is the screen: face up holds the paused figures
  // lit, face down goes dark.
  FACE_DOWN,
  FACE_UP,
  DEG_0,
  DEG_90,
  DEG_180,
  DEG_270,
  UNDEFINED
};

// What an interval is for. Both work faces -- the 25 minute one and flow's --
// count towards the pomodoro total, and this is what the advertisement's work
// flag carries, so the two can never disagree about which is which.
enum class TimerKind { Work, Break };

// Which way the seconds run. Countdown is every face but flow's work face.
enum class TimerMode { Countdown, CountUp };
