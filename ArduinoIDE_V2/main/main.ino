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
#include "ble.h"


// Counting down, this is the time left. Counting up, it is the time elapsed.
int remSeconds = 0;
// The interval being counted down, and 0 while counting up: nothing was chosen.
int selSeconds = 0;
TimerKind timerKind = TimerKind::Work;
TimerMode timerMode = TimerMode::Countdown;
// True when the countdown on screen is the flow bank draining, so each tick has
// to write the new balance back. The fixed faces leave the bank alone.
bool spendingFlowBank = false;
// One of flow's faces, either of them: the panel inverts for both.
bool onFlowFace = false;
unsigned long lastTick = 0;  // last count tick timestamp
unsigned long startedBeeping = 0;
// The timer face the cube was last stood on, so a pause knows what it paused.
Orientation lastTimerFace = Orientation::UNDEFINED;
// When the cube was last set on a different face. Only the backlight reads it:
// a face change is a decision worth lighting the panel for.
unsigned long lastFaceChange = 0;
// When the cube was last tapped, or 0 for not since this boot. Only the
// backlight reads this one too.
unsigned long lastTap = 0;

// How long since the last tap, or past any window when there has not been one.
// millis() - 0 is millis(), which reads as a tap a moment ago for the first ten
// seconds of every boot.
unsigned long sinceTap() {
  return lastTap == 0 ? ~0UL : millis() - lastTap;
}


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

// What the panel is drawn from. The bank shown on a running stint includes what
// that stint has earned so far, because what you want to know while looking at
// it is what turning the cube over would give you.
Display::TimerView timerView() {
  const int banked = RtcState::flowBank(RtcState::data());
  return {remSeconds, selSeconds, countingUp(), onFlowFace,
          countingUp() ? Util::flowBankPreview(banked, remSeconds) : banked};
}

// What goes out over BLE. The packet id is left at zero: BLE::publish() hands
// this to a BTHome::Sequencer, which numbers it and drops it if nothing has
// changed, so this is free to be called on every pass.
BTHome::State bthomeState() {
  BTHome::State state;
  state.packetId = 0;
  state.batteryVolts = Util::batteryVolts();
  state.awake = true;
  // A stint counting up is running from its first second, before remSeconds has
  // anything in it.
  state.running = countingUp() || remSeconds > 0;
  state.work = timerKind == TimerKind::Work;
  state.pomodoroCount = RtcState::data().pomodoroCount;
  state.remainingSeconds = remSeconds;
  state.selectedSeconds = selSeconds;
  return state;
}


// Everything standing the cube on a timer face asks for: closing off whatever
// the face being left was counting, picking up a pause that belongs to the new
// one, and repainting to match. Called from loop() when the debounce settles,
// and from setup() for the face a wake settles onto -- that one has already
// been through the debouncer, so nothing in loop() would announce it.
void applyFace(Orientation ori) {
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
  onFlowFace = spec.flow;

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
  Display::updateTimer(timerView());
  lastTick = millis();
}

// Poll the accelerometer until one reading has held still long enough to be
// believed, or until the cube has plainly been in a hand for too long to wait
// out. UNDEFINED means it never settled, which is not a resting face and so
// boots the cube -- the right way round to be wrong.
Orientation settleOrientation() {
  const unsigned long deadline = millis() + WAKE_SETTLE_TIMEOUT_MS;
  while (millis() < deadline) {
    float ax, ay, az;
    if (QMI::getAccelerometer(ax, ay, az) &&
        Util::updateOriDebounce(Util::calcOrientation(ax, ay, az), millis())) {
      return Util::getDebouncedOriState();
    }
    delay(20);
  }
  return Orientation::UNDEFINED;
}

// The frame a parked cube shows: the figures as they stood when it was set
// down, drawn from the stored pause rather than from the live timer, which on
// this path has not been set up and holds nothing.
Display::TimerView pausedView(const RtcState::Data &stored) {
  const int banked = RtcState::flowBank(stored);
  // The face it was paused from is what says whether this is one of flow's,
  // which the break face is as much as the work face -- countingUp alone would
  // draw a paused flow break in the fixed faces' colours.
  const Util::TimerSpec spec = Util::getTimerSpec(stored.pausedFace, banked);
  const bool up = stored.pausedCountingUp;
  return {(int)stored.pausedRemaining, (int)stored.pausedSelected, up, spec.flow,
          up ? Util::flowBankPreview(banked, (int)stored.pausedRemaining) : banked};
}

void setup() {
  Serial.begin(115200);
  setCpuFrequencyMhz(80);  // reducing CPU clock to 80MHz

  // Before anything reads it: keeps what survived deep sleep, discards what
  // did not.
  RtcState::begin();
  
  QMI::setup(); 

  // --------- go back to sleep mode ---------
  // Settled rather than sampled. The interrupt that woke us fired because the
  // cube moved, so a single reading taken a fixed delay later is as likely to
  // catch it in the air as on a face -- and a mid-air reading that looks like a
  // resting face sends the cube straight back to sleep on a face it is no
  // longer on, where nothing further will move to wake it. That is the second
  // half of why a cube picked up from its face-up rest and stood on a timer
  // face sometimes sat there still showing the paused frame.
  const Orientation settled = settleOrientation();
  if (Util::isRestingFace(settled)) {
    // Woken but still resting: go back down without touching what is parked.
    // Only face up keeps the panel lit, and only when there is a pause to show.
    const RtcState::Data &stored = RtcState::data();
    const bool showPause =
        settled == Orientation::FACE_UP && RtcState::hasPause(stored);
    if (!showPause) Util::deepSleep(Util::SleepMode::Off, false);

    // The frame is already on the glass, refreshing itself out of the panel's
    // own memory. Hold it there rather than spending a second booting the
    // display to draw what is already drawn.
    if (stored.panelHoldingFrame) Util::deepSleep(Util::SleepMode::Paused, false);

    // Otherwise the cube was parked face down, where the panel was blanked, and
    // has since been turned over: there is a pause to show and nothing on the
    // glass to show it with. Holding the backlight up over a sleeping panel is
    // what this used to do, which lit a black screen and flattened the pack.
    Display::setup();
    Util::updateBattery();
    Display::rotateScreen(stored.pausedFace);
    Display::updateTimer(pausedView(stored));
    Display::showPaused();
    Util::deepSleep(Util::SleepMode::Paused, false);
  }
  // -----------------------------------------

  Display::setup();
  Beeper::setup();
  Util::updateBattery();
  QMI::enableTapDetection();

  // After the panel, because bringing the radio up blocks for a moment while
  // the NimBLE host syncs, and a blank screen is the one thing worth avoiding
  // while it does. Deliberately not reached on the path above that wakes onto a
  // resting face: that path exists to get back to sleep in under a second, and
  // it has nothing new to say -- the advertisement that matters there already
  // went out when the cube was set down.
  BLE::setup();

  // The face the settling above landed on. loop()'s debouncer has already
  // accepted it, so nothing there would announce it -- the timer has to be set
  // up from here or the cube stands on a face that never starts. Skipped when
  // the cube never settled: there is no face to apply, and applying UNDEFINED
  // would take a pause that belongs to a real one and throw it away.
  if (settled != Orientation::UNDEFINED) {
    lastFaceChange = millis();
    applyFace(settled);
  }
}

void loop() {
  lv_timer_handler();
  delay(20);
  lv_tick_inc(20);

  float ax, ay, az;
  if (QMI::getAccelerometer(ax, ay, az)) {
    Orientation currentOri = Util::calcOrientation(ax, ay, az);
    if (Util::updateOriDebounce(currentOri, millis())) {
      Orientation ori = Util::getDebouncedOriState();
      lastFaceChange = millis();

      if (Util::isRestingFace(ori)) {
        // Both resting faces park the timer, stored against the face it was
        // running on and picked up again only by that face. A flow stint is
        // parked rather than ended -- standing the cube back on its face carries
        // on counting up. The two faces differ only in the screen.
        const Util::RestPlan plan =
            Util::restOnFace(RtcState::data(), ori, lastTimerFace, remSeconds, selSeconds,
                             countingUp());
        if (plan.lit) Display::showPaused();
        Util::deepSleep(plan.mode, true);
      }

      applyFace(ori);
    }
  }

  // A tap only ever buys brightness, so it is read here and left to the
  // backlight policy below. Nothing else on the cube changes because it was
  // touched -- the faces are what choose the timer.
  if (QMI::takeTap()) lastTap = millis();

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
    Display::updateTimer(timerView());
  }

  if (!countingUp() && remSeconds > 0 && millis() - lastTick >= 1000) {
    remSeconds--;
    // Keep the balance in step as the break is spent, so whatever interrupts it
    // -- another face, a pause, a flat battery -- leaves the rest still banked.
    if (spendingFlowBank) RtcState::setFlowBank(RtcState::data(), remSeconds);
    Display::updateTimer(timerView());
    lastTick = millis();
    if (remSeconds == 0) {
      startedBeeping = millis();
      // Only work timers count as pomodoros; breaks do not. Flow stints are
      // counted a lap at a time, as they run.
      if (timerKind == TimerKind::Work) RtcState::data().pomodoroCount++;
    }
  }

  if (!countingUp() && remSeconds == 0) {
    // The flash no longer hangs off the beep sequence. Beeper::beep() blocks for
    // the length of each note, so a flash driven from it inherits that cadence;
    // cycleTimerFinish() keeps its own clock and is called every pass.
    Beeper::cycleBeeper();
    Display::cycleTimerFinish();
    if (millis() - startedBeeping >= 1000 * 30) Util::deepSleep(Util::SleepMode::Off, true);
  }

  // Last, so it sees the tick this pass produced rather than the one before it.
  Display::setBacklight(Util::backlightPercent(
      {millis() - lastFaceChange, remSeconds, countingUp(), sinceTap()}));

  Battery::cycleBatteryUpdate();

  // Offered unconditionally, after the battery so the voltage is this pass's:
  // the sequencer behind this only reaches for the radio when the payload has
  // actually changed, which is a cheaper thing to get right than remembering to
  // publish at each of the places state moves.
  //
  // Not before a face is established, though. Between waking and the debounce
  // settling there is no timer at all, and the fields for that read as a flow
  // stint at zero seconds -- a state the cube is not in, and one it would
  // otherwise broadcast for the first 300 ms of every wake.
  if (lastTimerFace != Orientation::UNDEFINED) BLE::publish(bthomeState());
}
