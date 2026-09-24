#pragma once
#include <stdint.h>

#include "ui_colors.h"

#define BAT_ADC_PIN 1
#define TFT_BL_PIN 2
#define IMU_INT_PIN GPIO_NUM_4
#define I2C_SDA_PIN 6
#define I2C_SCL_PIN 7
#define BEEPER_PIN 15

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

// How long either side of a transition counts as worth looking at. Applied to
// both ends: this many seconds after a face change, and the last this many
// seconds of a countdown.
constexpr int BACKLIGHT_ATTENTION_SECONDS = 5;

// How long a tap holds the panel at full brightness. Longer than a face change
// is worth, because a tap is someone asking to read the thing rather than
// someone having just set it down and already looking at it.
constexpr int BACKLIGHT_TAP_SECONDS = 10;

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

constexpr int beepDurations[3] = { 120, 120, 100 };
constexpr int beepFrequencies[3] = { 1500, 1000, 2000 };
constexpr int beepDelays[3] = { 800, 20, 20 };

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
