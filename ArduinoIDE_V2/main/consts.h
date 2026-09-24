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
