#include <Arduino.h>
#include "consts.h"
#include "battery.h"
#include "display.h"
#include "util.h"
#include "display.h"
#include "qmi.h"
#include <lvgl.h>
#include "beeper.h"
#include "rtc_state.h"


int remSeconds = 0;          // remaining seconds
int selSeconds = 0;          // selected  timer seconds
unsigned long lastTick = 0;  // last count tick timestamp
unsigned long startedBeeping = 0;
// The timer face the cube was last stood on, so a pause knows what it paused.
Orientation lastTimerFace = Orientation::UNDEFINED;


void setup() {
  Serial.begin(115200);
  setCpuFrequencyMhz(80);  // reducing CPU clock to 80MHz

  // Before anything reads it: keeps what survived deep sleep, discards what
  // did not.
  RtcState::begin();
  
  QMI::setup(); 

  // --------- go back to sleep mode ---------
  delay(200); 
  float ax, ay, az;
  if (QMI::getAccelerometer(ax, ay, az)) {
    Orientation currentOri = Util::calcOrientation(ax, ay, az);
    if (Util::isRestingFace(currentOri)) {
      // Woken but still resting: hold a pause rather than discarding it, and
      // leave the panel alone -- it is still showing the paused frame.
      const bool holdingPause =
          currentOri == Orientation::FACE_UP && RtcState::hasPause(RtcState::data());
      if (!holdingPause) RtcState::clearPause(RtcState::data());
      Util::deepSleep(holdingPause ? Util::SleepMode::Paused : Util::SleepMode::Off, false);
    }
  }
  // -----------------------------------------

  Display::setup();
  Beeper::setup();
  Util::updateBattery();
}

void loop() {
  lv_timer_handler();
  delay(20);
  lv_tick_inc(20);

  float ax, ay, az;
  if (QMI::getAccelerometer(ax, ay, az)) {
    Orientation currentOri = Util::calcOrientation(ax, ay, az);
    if (Util::updateOriDebounce(currentOri)) {
      Orientation ori = Util::getDebouncedOriState();

      if (ori == Orientation::FACE_UP) {
        // Face up parks the timer: stored here, picked up again only by the
        // face it was paused from.
        RtcState::storePause(RtcState::data(), lastTimerFace, remSeconds, selSeconds);
        Display::showPaused();
        Util::deepSleep(Util::SleepMode::Paused, true);
      }
      if (ori == Orientation::FACE_DOWN) {
        // Face down means off, so nothing is kept.
        RtcState::clearPause(RtcState::data());
        Util::deepSleep(Util::SleepMode::Off, true);
      }

      lastTimerFace = ori;
      if (!RtcState::takePause(RtcState::data(), ori, remSeconds, selSeconds)) {
        remSeconds = Util::getTimerByOrientation(ori);
        selSeconds = remSeconds;
      }
      Display::rotateScreen(ori);
      Display::updateTimer(remSeconds, selSeconds);
      lastTick = millis();
    }
  }

  if (remSeconds > 0 && millis() - lastTick >= 1000) {
    remSeconds--;
    Display::updateTimer(remSeconds, selSeconds);
    lastTick = millis();
    if (remSeconds == 0) {
      startedBeeping = millis();
      // Only work timers count as pomodoros; breaks do not.
      if (selSeconds == TIMER_WORK_SECONDS) RtcState::data().pomodoroCount++;
    }
  }

  if (remSeconds == 0) {
    bool waitingLong = Beeper::cycleBeeper();
    if (waitingLong) Display::cycleTimerFinish();
    if (millis() - startedBeeping >= 1000 * 30) Util::deepSleep(Util::SleepMode::Off, true);
  }
  Battery::cycleBatteryUpdate();
}