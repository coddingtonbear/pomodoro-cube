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


// Counting down, this is the time left. Counting up, it is the time elapsed.
int remSeconds = 0;
// The interval being counted down, and 0 while counting up: nothing was chosen.
int selSeconds = 0;
TimerKind timerKind = TimerKind::Work;
TimerMode timerMode = TimerMode::Countdown;
// True when the countdown on screen is the flow bank draining, so each tick has
// to write the new balance back. The fixed faces leave the bank alone.
bool spendingFlowBank = false;
unsigned long lastTick = 0;  // last count tick timestamp
unsigned long startedBeeping = 0;
// The timer face the cube was last stood on, so a pause knows what it paused.
Orientation lastTimerFace = Orientation::UNDEFINED;


// A flow stint has ended: a fifth of it is credited to the break bank, on top of
// whatever was already there. Any stint that counted a second at all credits
// something -- a cube turned through the flow face on its way elsewhere never
// reaches a second, so there is nothing to screen out.
//
// Nothing is counted here. A stint scores by the lap while it runs, not by the
// stint when it stops, so its last partial lap is worth no more than abandoning
// a 25-minute timer at 24:00 is.
void bankFlowStint(int elapsed) {
  RtcState::addFlowBank(RtcState::data(), Util::flowBreakCredit(elapsed));
}

bool countingUp() {
  return timerMode == TimerMode::CountUp;
}


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
        // face it was paused from. A flow stint is parked rather than ended --
        // standing the cube back on its face carries on counting up.
        RtcState::storePause(RtcState::data(), lastTimerFace, remSeconds, selSeconds,
                             countingUp());
        Display::showPaused();
        Util::deepSleep(Util::SleepMode::Paused, true);
      }
      if (ori == Orientation::FACE_DOWN) {
        // Face down means off, so nothing is kept -- the break bank included.
        RtcState::clearPause(RtcState::data());
        RtcState::clearFlowBank(RtcState::data());
        Util::deepSleep(Util::SleepMode::Off, true);
      }

      // Turning off the flow face ends the stint it was counting.
      if (countingUp() && ori != lastTimerFace) bankFlowStint(remSeconds);

      // So does standing the cube on a face that abandons a parked one. The
      // pause itself is consumed by takePause() below either way; this is only
      // about banking what it was worth.
      {
        const RtcState::Data &stored = RtcState::data();
        if (RtcState::hasPause(stored) && stored.pausedCountingUp &&
            stored.pausedFace != ori) {
          bankFlowStint((int)stored.pausedRemaining);
        }
      }

      lastTimerFace = ori;

      // Read rather than spend: the bank is a balance, and only the break face
      // draining it writes it down. Every other face leaves it where it is, so
      // a spell on the 25-minute face doesn't cost you the break you earned.
      const Util::TimerSpec spec = Util::getTimerSpec(ori, RtcState::flowBank(RtcState::data()));
      spendingFlowBank = spec.spendsBank;

      bool resumesCountingUp = false;
      if (RtcState::takePause(RtcState::data(), ori, remSeconds, selSeconds,
                              resumesCountingUp)) {
        timerMode = resumesCountingUp ? TimerMode::CountUp : TimerMode::Countdown;
        timerKind = spec.kind;
      } else {
        timerKind = spec.kind;
        timerMode = spec.mode;
        remSeconds = spec.mode == TimerMode::CountUp ? 0 : spec.seconds;
        selSeconds = spec.mode == TimerMode::CountUp ? 0 : spec.seconds;
      }
      // A break face turned to with an empty bank has no time to count, so it
      // is finished before it starts. Dating the beeping from here rather than
      // leaving the last finish's timestamp in place is what gives it the usual
      // thirty seconds before sleeping, instead of a stale one that could sleep
      // the cube on the spot.
      if (!countingUp() && remSeconds == 0) startedBeeping = millis();

      Display::rotateScreen(ori);
      Display::updateTimer(remSeconds, selSeconds, countingUp());
      lastTick = millis();
    }
  }

  if (countingUp() && millis() - lastTick >= 1000) {
    remSeconds++;
    lastTick = millis();
    // Every lap of the arc is a pomodoro, scored as the lap closes rather than
    // when the stint ends: two hours of flow is four pomodoros, and the count
    // reaches Home Assistant while the stint is still running.
    if (Util::completesFlowLap(remSeconds)) RtcState::data().pomodoroCount++;
    if (remSeconds >= FLOW_MAX_SECONDS) {
      // A stint has to end somewhere: left standing on the flow face the cube
      // would hold the backlight on until the pack went flat. Ending it here
      // rather than at the face change means bankFlowStint() still runs once.
      bankFlowStint(remSeconds);
      timerMode = TimerMode::Countdown;
      selSeconds = FLOW_MAX_SECONDS;
      remSeconds = 0;
      startedBeeping = millis();
    }
    Display::updateTimer(remSeconds, selSeconds, countingUp());
  }

  if (!countingUp() && remSeconds > 0 && millis() - lastTick >= 1000) {
    remSeconds--;
    // Keep the balance in step as the break is spent, so whatever interrupts it
    // -- another face, a pause, a flat battery -- leaves the rest still banked.
    if (spendingFlowBank) RtcState::setFlowBank(RtcState::data(), remSeconds);
    Display::updateTimer(remSeconds, selSeconds, false);
    lastTick = millis();
    if (remSeconds == 0) {
      startedBeeping = millis();
      // Only work timers count as pomodoros; breaks do not. Flow stints are
      // counted a lap at a time, as they run.
      if (timerKind == TimerKind::Work) RtcState::data().pomodoroCount++;
    }
  }

  if (!countingUp() && remSeconds == 0) {
    bool waitingLong = Beeper::cycleBeeper();
    if (waitingLong) Display::cycleTimerFinish();
    if (millis() - startedBeeping >= 1000 * 30) Util::deepSleep(Util::SleepMode::Off, true);
  }
  Battery::cycleBatteryUpdate();
}
