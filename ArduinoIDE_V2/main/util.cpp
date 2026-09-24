#include "util.h"
#include <Arduino.h>
#include "consts.h"
#include "battery.h"
#include "util.h"
#include "qmi.h"
#include "display.h"
#include <driver/rtc_io.h>
#include "vFilter.h"
#include "beeper.h"

VoltageSmoother<10> vFilter;


void Util::updateBattery() {
  vFilter.add(Battery::getVoltage());
  Display::updateBattery(vFilter.getAverage());
}

Orientation Util::calcOrientation(float ax, float ay, float az) {
  // GUESS, unverified: an accelerometer at rest reads +1g along whichever axis
  // points up, so this assumes the QMI8658's +Z leaves the front of the cube.
  // If face-up and face-down turn out to be swapped on real hardware, exchange
  // these two lines -- nothing else depends on the polarity.
  if (az > 0.8) return Orientation::FACE_UP;
  if (az < -0.8) return Orientation::FACE_DOWN;
  if (ay > 0.8) return Orientation::DEG_270;
  if (ax > 0.8) return Orientation::DEG_180;
  if (ay < -0.8) return Orientation::DEG_90;
  if (ax < -0.8) return Orientation::DEG_0;
  return Orientation::DEG_0;
}


void Util::deepSleep(SleepMode mode, bool playSound) {
  if (mode == SleepMode::Off) Display::deepSleep();
  else Display::holdPausedFrame();

  delay(1000);

  if (playSound) Beeper::playShutdown();

  QMI::setupWakeup();

  // Float I2C lines to prevent parasitic draw
  pinMode(I2C_SDA_PIN, INPUT);
  pinMode(I2C_SCL_PIN, INPUT);

  rtc_gpio_pullup_dis(IMU_INT_PIN);
  rtc_gpio_pulldown_en(IMU_INT_PIN);

  esp_sleep_enable_ext0_wakeup(IMU_INT_PIN, 1);  // 1 = Wakeup at HIGH

  // --- Beeper Hold Logic ---
  // Ensure the pin is explicitly HIGH (OFF) before sleeping
  digitalWrite(BEEPER_PIN, HIGH);

  // Lock the pin state in the RTC domain
  gpio_hold_en((gpio_num_t)BEEPER_PIN);
  gpio_deep_sleep_hold_en();
  // -------------------------

  esp_deep_sleep_start();
}

bool Util::isRestingFace(Orientation ori) {
  return ori == Orientation::FACE_UP || ori == Orientation::FACE_DOWN;
}

Util::TimerSpec Util::getTimerSpec(Orientation ori, int earnedFlowSeconds) {
  switch (ori) {
    case Orientation::DEG_0:
      return {TimerKind::Work, TimerMode::Countdown, TIMER_WORK_SECONDS};
    case Orientation::DEG_90:
      return {TimerKind::Break, TimerMode::Countdown, TIMER_SHORT_BREAK_SECONDS};
    case Orientation::DEG_180:
      // Flow's work face: no length, because it counts up until the cube is
      // turned off it.
      return {TimerKind::Work, TimerMode::CountUp, 0};
    case Orientation::DEG_270:
      return {TimerKind::Break, TimerMode::Countdown, flowBreakSeconds(earnedFlowSeconds)};
    default:
      // A resting face runs no timer, but must still not report zero: a
      // zero-length timer would divide by zero in the arc.
      return {TimerKind::Work, TimerMode::Countdown, TIMER_WORK_SECONDS};
  }
}

int Util::flowBreakSeconds(int earnedSeconds) {
  if (earnedSeconds < FLOW_MIN_STINT_SECONDS) return TIMER_LONG_BREAK_SECONDS;
  return earnedSeconds / FLOW_BREAK_DIVISOR;
}

unsigned long lastOriChangeTime = 0;
Orientation debouncedState = Orientation::UNDEFINED;

bool Util::updateOriDebounce(Orientation rawState) {
  if (rawState == debouncedState) {
    lastOriChangeTime = millis();
  } else if ((millis() - lastOriChangeTime) >= ORI_DEBOUNCE_DELAY) {
    debouncedState = rawState;
    return true;
  }
  return false;
}

Orientation Util::getDebouncedOriState() {
  return debouncedState;
}
