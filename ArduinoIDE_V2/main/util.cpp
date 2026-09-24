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

Util::RestPlan Util::restOnFace(RtcState::Data &data, Orientation ori, Orientation timerFace,
                                int remaining, int selected, bool countingUp) {
  // Park rather than discard, on either face. Setting the cube down is not the
  // same as choosing a different interval -- that is what standing it on another
  // face means -- so the pause and the break bank both survive it. Face down is
  // face up with the lights out.
  RtcState::storePause(data, timerFace, remaining, selected, countingUp);

  const bool lit = ori == Orientation::FACE_UP;
  return {lit, lit ? SleepMode::Paused : SleepMode::Off};
}

Util::TimerSpec Util::getTimerSpec(Orientation ori, int bankedBreakSeconds) {
  switch (ori) {
    case Orientation::DEG_0:
      return {TimerKind::Work, TimerMode::Countdown, TIMER_WORK_SECONDS, false, false};
    case Orientation::DEG_90:
      return {TimerKind::Break, TimerMode::Countdown, TIMER_SHORT_BREAK_SECONDS, false, false};
    case Orientation::DEG_180:
      // Flow's work face: no length, because it counts up until the cube is
      // turned off it.
      return {TimerKind::Work, TimerMode::CountUp, 0, true, false};
    case Orientation::DEG_270:
      // Flow's break face pays out the bank and nothing else. An empty bank is
      // a break of no length, which finishes the moment it starts -- there is
      // nothing to fall back on, because any fallback would be break time
      // nobody worked for.
      return {TimerKind::Break, TimerMode::Countdown,
              bankedBreakSeconds > 0 ? bankedBreakSeconds : 0, true, true};
    default:
      // A resting face runs no timer, but must still not report zero: a
      // zero-length timer would divide by zero in the arc.
      return {TimerKind::Work, TimerMode::Countdown, TIMER_WORK_SECONDS, false, false};
  }
}

int Util::flowBreakCredit(int workedSeconds) {
  if (workedSeconds <= 0) return 0;
  return workedSeconds / FLOW_BREAK_DIVISOR;
}

int Util::flowBankPreview(int bankedSeconds, int elapsedSeconds) {
  if (bankedSeconds < 0) bankedSeconds = 0;
  const int preview = bankedSeconds + flowBreakCredit(elapsedSeconds);
  // Clamped the way the bank itself is, so the figure on screen is one the
  // cube can actually honour.
  return preview > FLOW_MAX_SECONDS ? FLOW_MAX_SECONDS : preview;
}

bool Util::completesFlowLap(int elapsedSeconds) {
  return elapsedSeconds > 0 && elapsedSeconds % FLOW_LAP_SECONDS == 0;
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
