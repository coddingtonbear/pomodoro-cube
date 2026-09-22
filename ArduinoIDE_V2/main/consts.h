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
constexpr int TIMER_LONG_WORK_SECONDS = 50 * 60;
constexpr int TIMER_LONG_BREAK_SECONDS = 10 * 60;

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
  // Both resting faces put the cube to sleep today. They are separate states so
  // that face-up can become "paused" and face-down "off" without another change
  // to orientation sensing.
  FACE_DOWN,
  FACE_UP,
  DEG_0,
  DEG_90,
  DEG_180,
  DEG_270,
  UNDEFINED
};