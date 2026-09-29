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

// Below this the firmware deep-sleeps rather than running the pack flat.
#define BAT_EMPTY_VOLTAGE 3.5

#define ORI_DEBOUNCE_DELAY 300

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

// How long a woken, switched-off cube listens for the gesture that switches it
// back on: turned face up, then double-tapped on the glass. Face down is off,
// and off has to survive a bag -- every bump wakes the cube, so the wake alone
// cannot be what switches it on. The window runs from waking, for the turn
// over, and starts again from the moment the cube comes to rest face up, for
// the taps. A double tap that misses it still wakes the cube -- it is movement
// -- which opens a fresh one, so the second attempt works however long the cube
// has been lying there.
//
// The listening has to start at the wake rather than once the cube has settled:
// the first thing a hand reaching for a face-down cube does is lift it, which
// wakes it while it is still face down. Settling first, as on the hardware the
// first time round, read that as "still face down", went back to sleep, and
// finished going to sleep while the cube was being turned over -- so the turn
// woke nothing, and the double tap was spent waking the cube rather than
// switching it on.
constexpr unsigned long SWITCH_ON_WINDOW_MS = 5000;

// What counts as a shake, which switches on a cube lying face up as a double
// tap does: this many readings at least SHAKE_MIN_DEVIATION_G more or less
// than one g, within this long of each other. Read off the accelerometer rather than left to the tap engine,
// which is listening for one sharp jolt that dies away and hears a shake as
// neither. Readings come a pass apart, 20 ms, so this is about a tenth of a
// second of being moved about in the space of a second -- more than a knock on
// the desk, which is over in a reading or two. Not tuned against a cube in the
// hand.
//
// The deviation is what makes it a shake rather than being picked up. Anything
// outside the gravity band counted at first, which is 0.15 g and was met by
// handling the cube at all. Half a g takes a deliberate shake.
constexpr float SHAKE_MIN_DEVIATION_G = 0.5f;
constexpr int SHAKE_READINGS = 5;
constexpr unsigned long SHAKE_WINDOW_MS = 1000;

// How long a woken, switched-off cube will sit still on any face but face up
// before giving up and going back to sleep. What cuts short the wakes a bag
// causes, each of which would otherwise keep the CPU up for the whole window;
// long enough that the pause as a hand takes hold of a face-down cube, before
// turning it, is not taken for it having been left alone.
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

// Where a stint gives up and finishes on its own. A cube left standing on the
// flow face would otherwise hold the backlight on until the pack went flat, and
// the advertisement's uint24 of milliseconds runs out shortly after this.
constexpr int FLOW_MAX_SECONDS = 4 * 60 * 60;

// The backlight is the largest single draw while the cube is awake -- of the
// same order as the whole rest of the board -- so full brightness is rationed
// rather than held. It is spent on the moments worth looking at: just after the
// cube is set on a face, and as a countdown runs out. Everything between sits at
// BACKLIGHT_IDLE_PERCENT, which is legible across a desk for a fraction of the
// current.
constexpr int BACKLIGHT_FULL_PERCENT = 100;
constexpr int BACKLIGHT_IDLE_PERCENT = 20;

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

// The countdown arc runs full to empty, shading from ARC_COLOR_FULL through
// ARC_COLOR_MID to ARC_COLOR_LOW as the remaining percentage falls past these
// stops. Below ARC_LOW_PERCENT it stays at ARC_COLOR_LOW.
constexpr int ARC_MID_PERCENT = 50;
constexpr int ARC_LOW_PERCENT = 25;

// The battery warning is hidden entirely above this pack voltage. Expressed in
// volts rather than as a percentage because the percentage was a linear fiction
// over a range that has never been checked against real hardware, and because
// the warning shows the measured voltage for exactly that calibration job.
constexpr float LOW_BATTERY_VOLTAGE = 3.6f;

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
