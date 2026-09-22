#pragma once

#define BAT_ADC_PIN 1
#define TFT_BL_PIN 2
#define IMU_INT_PIN GPIO_NUM_4
#define I2C_SDA_PIN 6
#define I2C_SCL_PIN 7
#define BEEPER_PIN 15

#define BAT_FULL_VOLTAGE 4.0
#define BAT_EMPTY_VOLTAGE 3.5

#define ORI_DEBOUNCE_DELAY 300

// Timer length for each face the cube can rest on, in seconds. DEG_0 is the
// default orientation and each step from there is a quarter turn clockwise,
// matching the order of the Orientation enum below.
constexpr int TIMER_WORK_SECONDS = 25 * 60;
constexpr int TIMER_SHORT_BREAK_SECONDS = 5 * 60;
constexpr int TIMER_LONG_WORK_SECONDS = 50 * 60;
constexpr int TIMER_LONG_BREAK_SECONDS = 10 * 60;
constexpr int beepDurations[3] = { 120, 120, 100 };
constexpr int beepFrequencies[3] = { 1500, 1000, 2000 };
constexpr int beepDelays[3] = { 800, 20, 20 };

enum class Orientation {
  SLEEP,
  DEG_0,
  DEG_90,
  DEG_180,
  DEG_270,
  UNDEFINED
};