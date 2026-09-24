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
#include "ble.h"

VoltageSmoother<10> vFilter;


void Util::updateBattery() {
  vFilter.add(Battery::getVoltage());
  Display::updateBattery(vFilter.getAverage());
}

float Util::batteryVolts() {
  return vFilter.getAverage();
}

Orientation Util::calcOrientation(float ax, float ay, float az) {
  // Verified on hardware: the QMI8658's +Z points into the back of the cube, not
  // out of the front, so a cube resting screen-up reads -1g on Z. The first
  // guess had these the other way round and put the cube to sleep when it was
  // set down to be read. Nothing else depends on the polarity.
  if (az < -0.8) return Orientation::FACE_UP;
  if (az > 0.8) return Orientation::FACE_DOWN;
  if (ay > 0.8) return Orientation::DEG_270;
  if (ax > 0.8) return Orientation::DEG_180;
  if (ay < -0.8) return Orientation::DEG_90;
  if (ax < -0.8) return Orientation::DEG_0;
  return Orientation::DEG_0;
}


void Util::deepSleep(SleepMode mode, bool playSound) {
  // Before anything else that takes time: Home Assistant holds the last state
  // it heard, so a cube that just stopped counting has to say so while it still
  // has a radio. Everything after this is either invisible to a receiver or
  // already decided.
  BLE::farewell();

  // Written down rather than worked out again on the next wake. A wake that
  // goes straight back to sleep never brings the panel up, so it cannot ask the
  // panel what it is showing -- and the two answers need different handling:
  // a held frame has to be put away before the panel can go dark, and a blank
  // panel has to be drawn on before the backlight is worth holding up.
  RtcState::data().panelHoldingFrame = (mode == SleepMode::Paused);

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

int Util::backlightPercent(const BacklightView &view) {
  // Just set down. The face was chosen a moment ago, and what it came up with is
  // the answer to that choice.
  if (view.sinceFaceChangeMs < (unsigned long)BACKLIGHT_ATTENTION_SECONDS * 1000UL) {
    return BACKLIGHT_FULL_PERCENT;
  }

  // Just tapped, which is the same request made deliberately: someone wants to
  // read a face that has settled to the idle level. Held longer than a face
  // change, because they are coming to it cold rather than already looking.
  if (view.sinceTapMs < (unsigned long)BACKLIGHT_TAP_SECONDS * 1000UL) {
    return BACKLIGHT_FULL_PERCENT;
  }

  // Running out, or run out. The same test covers both: a countdown at 0 has
  // finished and is beeping, and stays lit until it is dealt with -- which is
  // bounded, because a finished timer sleeps the cube after thirty seconds.
  if (!view.countingUp && view.remainingSeconds <= BACKLIGHT_ATTENTION_SECONDS) {
    return BACKLIGHT_FULL_PERCENT;
  }

  return BACKLIGHT_IDLE_PERCENT;
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

// The face the cube is currently being read as, and the one it has been read as
// for long enough to believe.
Orientation candidateState = Orientation::UNDEFINED;
unsigned long candidateSince = 0;
Orientation debouncedState = Orientation::UNDEFINED;

bool Util::updateOriDebounce(Orientation rawState, unsigned long nowMs) {
  // Any change of reading restarts the clock. The earlier version timed from
  // the last sample that agreed with the *accepted* face instead, which means a
  // cube tumbling through three faces on its way to a fourth never reset it:
  // whatever sample happened to land ORI_DEBOUNCE_DELAY after the cube left the
  // old face was taken as the new one. Picking a cube up off its face-up rest
  // and standing it on a timer face goes through face-up on the way, so that is
  // exactly how a cube ended up believing it had been set down again, parking
  // the paused frame on a panel that should have gone back to work.
  if (rawState != candidateState) {
    candidateState = rawState;
    candidateSince = nowMs;
    return false;
  }

  if (rawState == debouncedState) return false;
  if (nowMs - candidateSince < (unsigned long)ORI_DEBOUNCE_DELAY) return false;

  debouncedState = rawState;
  return true;
}

Orientation Util::getDebouncedOriState() {
  return debouncedState;
}

void Util::resetOriDebounce() {
  candidateState = Orientation::UNDEFINED;
  candidateSince = 0;
  debouncedState = Orientation::UNDEFINED;
}
