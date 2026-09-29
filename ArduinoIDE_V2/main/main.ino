#include <Arduino.h>
#include "consts.h"
#include "battery.h"
#include "display.h"
#include "util.h"
#include "display.h"
#include "qmi.h"
#include <lvgl.h>
#include "haptic.h"
#include "rtc_state.h"
#include "ble.h"
#include "tilt.h"


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
// When the timer on screen ran out, which is what the sleep that ends an
// unanswered alarm is timed from.
unsigned long startedAlarm = 0;
// The timer face the cube was last stood on, so a pause knows what it paused.
Orientation lastTimerFace = Orientation::UNDEFINED;
// When the cube was last set on a different face. Only the backlight reads it:
// a face change is a decision worth lighting the panel for.
unsigned long lastFaceChange = 0;
// When the cube was last tapped, or 0 for not since this boot. Only the
// backlight reads this one too.
unsigned long lastTap = 0;
// Lying face up with a timer parked, and awake: nothing counts, the panel
// shows the parked figures, and the loop goes on watching the faces so that
// standing the cube back up is a face change like any other rather than a
// wake. `pausedAt` is what the sleep that ends a forgotten pause is timed from.
bool paused = false;
unsigned long pausedAt = 0;

// The angle the face is drawn at, followed from the accelerometer on every
// pass. Nothing to do with which face the cube is on, which is debounced and
// chooses the timer; this only turns the picture.
Tilt::Tracker tilt;

// Feed the tracker whatever the accelerometer had to say, including nothing.
// True when the angle to draw has moved.
bool senseTilt(bool haveReading, float ax, float ay, float az) {
  return tilt.update(haveReading && Util::isGravityOnly(ax, ay, az), ax, ay, millis());
}

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
  state.running = !paused && (countingUp() || remSeconds > 0);
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
  // Turning off the flow face ends the stint it was counting. Not out of a
  // pause, where the stint is the parked one and is banked as that below.
  if (!paused && countingUp() && ori != lastTimerFace) bankFlowStint(remSeconds);
  paused = false;

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
  // is finished before it starts. Dating the alarm from here rather than
  // leaving the last finish's timestamp in place is what gives it the usual
  // thirty seconds before sleeping, instead of a stale one that could sleep
  // the cube on the spot.
  if (!countingUp() && remSeconds == 0) startedAlarm = millis();

  // At the angle the cube is actually held, which by now the tracker has been
  // following for at least the length of the debounce. The face's own angle is
  // only the fallback for a tracker that has somehow heard nothing.
  if (!tilt.hasAngle()) tilt.seed(Tilt::faceAngle(ori));
  Display::setAngle(tilt.angle());
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
    const bool haveReading = QMI::getAccelerometer(ax, ay, az);
    // Followed from here, with no panel up yet to draw on, so that the first
    // frame is drawn at the angle the cube is held at rather than swinging
    // round to it.
    senseTilt(haveReading, ax, ay, az);
    if (haveReading && Util::updateOriDebounce(ax, ay, az, millis())) {
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

// Hold the parked timer on the panel with the cube awake. The pause itself is
// already in RTC memory, which is where standing the cube back up will take it
// from; the live timer is set to match so that what is advertised, and what is
// parked again if the cube is turned face down from here, are the same figures.
void holdPause() {
  const RtcState::Data &stored = RtcState::data();
  const Util::TimerSpec spec =
      Util::getTimerSpec(stored.pausedFace, RtcState::flowBank(stored));
  lastTimerFace = stored.pausedFace;
  remSeconds = (int)stored.pausedRemaining;
  selSeconds = (int)stored.pausedSelected;
  timerMode = stored.pausedCountingUp ? TimerMode::CountUp : TimerMode::Countdown;
  timerKind = spec.kind;
  spendingFlowBank = spec.spendsBank;
  onFlowFace = spec.flow;

  paused = true;
  pausedAt = millis();

  // Square on the face the timer was running on, which is how a wake that has
  // to redraw this frame will draw it. A cube laid down part-way through a
  // turn would otherwise be parked at whatever angle the turn had reached.
  Display::rotateScreen(stored.pausedFace);
  Display::updateTimer(pausedView(stored));
  Display::showPaused();
}

// Follow the faces for up to `forMs`, and return the first face other than
// `current` the cube settles on, or `current` if it settles on nothing else.
Orientation watchFaces(unsigned long forMs, Orientation current) {
  const unsigned long started = millis();
  while (millis() - started < forMs) {
    feedLoopWDT();
    float ax, ay, az;
    if (QMI::getAccelerometer(ax, ay, az) && Util::updateOriDebounce(ax, ay, az, millis())) {
      const Orientation face = Util::getDebouncedOriState();
      if (face != current) return face;
    }
    delay(20);
  }
  return current;
}

// A switched-off cube has been woken: listen for the gesture that switches it
// back on. Dark and silent throughout -- no panel, no radio, no buzz -- because
// most wakes of a switched-off cube are a bag being carried, and the only thing
// worth spending on one of those is getting back to sleep.
bool heardSwitchOn() {
  QMI::enableTapDetection();
  Util::SwitchOnListener listener(millis());
  for (;;) {
    feedLoopWDT();
    const QMI::Tap tap = QMI::takeTap();
    if (tap != QMI::Tap::None) {
      Serial.printf("[trace] SWITCH_ON_TAP t=%lu tap=%d ori=%d\n", millis(), (int)tap,
                    (int)Util::getDebouncedOriState());
    }
    float ax, ay, az;
    const bool haveReading = QMI::getAccelerometer(ax, ay, az);
    const Util::SwitchOnVerdict verdict =
        listener.update(haveReading, ax, ay, az, tap == QMI::Tap::Double, millis());
    // Lit only once the cube is lying face up, which is the moment the taps
    // start to count. A wake in a bag never gets this far, and stays dark. The
    // panel then stays up until the cube sleeps or is switched on, even if it
    // is picked up again in between.
    if (listener.armed()) Display::showMessage("Double-tap\nto start");
    if (verdict != Util::SwitchOnVerdict::Listening) {
      return verdict == Util::SwitchOnVerdict::On;
    }

    delay(20);
  }
}

void setup() {
  Serial.begin(115200);
  setCpuFrequencyMhz(80);  // reducing CPU clock to 80MHz

  // Put this task under the task watchdog, which the core then feeds before
  // every pass of loop(). A pass that has not come back within the watchdog's
  // 5 seconds -- an I2C read that never returns, a wait on something that never
  // happens -- panics and reboots the cube rather than leaving it frozen on
  // whatever was last drawn. Nothing that blocks here comes near that: the
  // longest is going to sleep, a little over two seconds end to end. setup()
  // is covered too, which is why the settle below feeds it on its way out.
  enableLoopWDT();
  // A watchdog reboot looks like any other boot from the outside; this is how
  // to tell one apart on the serial log.
  Serial.printf("[trace] RESET reason=%d\n", (int)esp_reset_reason());

  // First, so the motor's pin is driven low from the earliest moment there is
  // code to drive it, on every path out of here.
  Haptic::setup();

  // Before anything reads it: keeps what survived deep sleep, discards what
  // did not.
  RtcState::begin();
  
  QMI::setup(); 

  Serial.printf("[trace] BOOT hasPause=%d pausedFace=%d holdingFrame=%d switchedOff=%d\n",
                (int)RtcState::hasPause(RtcState::data()),
                (int)RtcState::data().pausedFace,
                (int)RtcState::data().panelHoldingFrame,
                (int)RtcState::data().switchedOff);

  // Switched off by being set down face down. Nothing wakes it but being turned
  // face up and double-tapped: not standing it on a timer face, and not being
  // carried. Anything short of that and it goes back to sleep still switched
  // off, to ask again the next time it moves.
  Orientation settled;
  bool resumesPause = false;
  if (RtcState::data().switchedOff) {
    const bool switchOn = heardSwitchOn();
    Serial.printf("[trace] SWITCHED_OFF switchOn=%d\n", (int)switchOn);
    if (!switchOn) Util::deepSleep(Util::SleepMode::Off, false);

    // On, and lying face up, which is where the resting-face path below takes
    // over: a parked timer comes up on the glass, and with none the cube sleeps
    // dark, now waiting to be stood on a face like any other. The buzz is the
    // only sign of having been switched on when there is nothing to show.
    RtcState::data().switchedOff = false;
    Haptic::playBlocking(Haptic::Pattern::Wake);
    settled = Orientation::FACE_UP;

    // A parked timer is what the resting-face path below puts up. With none,
    // there is nothing to show, and a panel that went dark the moment it was
    // switched on read as the switch-on having failed -- so answer the double
    // tap, for a moment, inverted from the prompt so the change is plain from
    // across a desk. Watching the faces while it does: a cube stood on one now
    // has to start its timer rather than sleep through being set down, which
    // would leave nothing to wake it.
    if (!RtcState::hasPause(RtcState::data())) {
      Display::showMessage("Let's go", true);
      settled = watchFaces(SWITCHED_ON_MESSAGE_MS, Orientation::FACE_UP);
    }
    Display::hideMessage();
  } else {
    // --------- go back to sleep mode ---------
    // Settled rather than sampled. The interrupt that woke us fired because the
    // cube moved, so a single reading taken a fixed delay later is as likely to
    // catch it in the air as on a face -- and a mid-air reading that looks like
    // a resting face sends the cube straight back to sleep on a face it is no
    // longer on, where nothing further will move to wake it. That is the second
    // half of why a cube picked up from its face-up rest and stood on a timer
    // face sometimes sat there still showing the paused frame.
    settled = settleOrientation();
  }
  // Either of the above can take a good part of the watchdog's five seconds;
  // the rest of setup() gets a fresh allowance.
  feedLoopWDT();
  Serial.printf("[trace] SETTLED ori=%d\n", (int)settled);
  if (Util::isRestingFace(settled)) {
    // Woken but still resting: go back down without touching what is parked.
    // Only face up keeps the panel lit, and only when there is a pause to show.
    //
    // Face down is off however the cube got there -- including turned over
    // while it slept face up, which is the commonest way to put it away and the
    // one that, before this, left it merely asleep.
    if (settled == Orientation::FACE_DOWN) RtcState::data().switchedOff = true;
    resumesPause = settled == Orientation::FACE_UP && RtcState::hasPause(RtcState::data());
    if (!resumesPause) Util::deepSleep(Util::SleepMode::Off, false);

    // Face up with a timer parked: stay up and hold the pause awake, whether
    // the frame was still on the glass or not. Going straight back to sleep is
    // what this used to do, and it takes over a second in which the cube sees
    // nothing -- so one picked up, held level for a moment and then stood on a
    // face was set down during it, and sat there showing the paused frame
    // until it was bumped.
  }
  // -----------------------------------------

  Display::setup();
  // Only for a cube that is going back to work. One that woke still lying
  // where it was has nothing to announce, and may only have been jostled.
  if (!resumesPause) Haptic::play(Haptic::Pattern::Wake);
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
  // up from here or the cube stands on a face that never starts. Without the
  // face change's own buzz: the wake pattern already playing is the
  // announcement, and this face is where the cube woke rather than somewhere
  // it was moved to. Skipped when
  // the cube never settled: there is no face to apply, and applying UNDEFINED
  // would take a pause that belongs to a real one and throw it away.
  if (resumesPause) {
    lastFaceChange = millis();
    holdPause();
  } else if (settled != Orientation::UNDEFINED) {
    lastFaceChange = millis();
    applyFace(settled);
  }
}

void loop() {
  lv_timer_handler();
  delay(20);
  lv_tick_inc(20);

  float ax, ay, az;
  const bool haveReading = QMI::getAccelerometer(ax, ay, az);
  // Followed throughout, drawn only while there is a face to turn: a paused
  // frame stays square on the face it was parked from.
  if (senseTilt(haveReading, ax, ay, az) && !paused) Display::setAngle(tilt.angle());

  if (haveReading) {
    if (Util::updateOriDebounce(ax, ay, az, millis())) {
      Orientation ori = Util::getDebouncedOriState();
      lastFaceChange = millis();

      Serial.printf("[trace] FACE t=%lu ori=%d\n", millis(), (int)ori);
      if (Util::isRestingFace(ori)) {
        // Both resting faces park the timer, stored against the face it was
        // running on and picked up again only by that face. A flow stint is
        // parked rather than ended -- standing the cube back on its face carries
        // on counting up. The two faces differ only in the screen.
        const Util::RestPlan plan =
            Util::restOnFace(RtcState::data(), ori, lastTimerFace, remSeconds, selSeconds,
                             countingUp());
        if (plan.lit && RtcState::hasPause(RtcState::data())) {
          // Paused, and staying awake for it, so that leaving the pause is a
          // face change rather than a wake.
          Haptic::play(Haptic::Pattern::FaceChange);
          holdPause();
          return;
        }
        if (plan.lit) {
          if (lastTimerFace != Orientation::UNDEFINED) Display::rotateScreen(lastTimerFace);
          Display::showPaused();
        }
        Util::deepSleep(plan.mode, true);
      }

      // Replaces whatever was playing, which is what silences an alarm: a
      // finished timer buzzes until the cube is turned to something else.
      Haptic::play(Haptic::Pattern::FaceChange);
      applyFace(ori);
    }
  }

  {
    static unsigned long lastTrace = 0;
    if (millis() - lastTrace >= 200) {
      lastTrace = millis();
      Serial.printf("[trace] awake t=%lu ori=%d ok=%d dec=%d a=%.3f,%.3f,%.3f\n", millis(),
                    (int)Util::calcOrientation(ax, ay, az), (int)Util::isGravityOnly(ax, ay, az),
                    (int)Util::isDecisive(ax, ay, az), ax, ay, az);
    }
  }

  // A tap only ever buys brightness, so it is read here and left to the
  // backlight policy below. Nothing else on the cube changes because it was
  // touched -- the faces are what choose the timer. Single or double, either
  // counts: what was asked for is a tap. Narrowing this to Tap::Double is the
  // whole change if single taps turn out to fire at things that were not taps.
  //
  // Read every pass so the sensor's latch is cleared, but not believed while
  // the motor is running: the tap detector cannot tell a finger from the cube
  // shaking itself.
  if (QMI::takeTap() != QMI::Tap::None && !Haptic::disturbing()) lastTap = millis();

  if (paused) {
    // Nothing counts and nothing alarms. Long enough forgotten, the cube goes
    // to sleep holding the frame, as a pause always used to from the start.
    if (millis() - pausedAt >= PAUSE_AWAKE_MS) {
      Display::showPaused();
      Util::deepSleep(Util::SleepMode::Paused, false);
    }
  } else if (countingUp() && millis() - lastTick >= 1000) {
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
      startedAlarm = millis();
    }
    Display::updateTimer(timerView());
  }

  if (!paused && !countingUp() && remSeconds > 0 && millis() - lastTick >= 1000) {
    remSeconds--;
    // Keep the balance in step as the break is spent, so whatever interrupts it
    // -- another face, a pause, a flat battery -- leaves the rest still banked.
    if (spendingFlowBank) RtcState::setFlowBank(RtcState::data(), remSeconds);
    Display::updateTimer(timerView());
    lastTick = millis();
    if (remSeconds == 0) {
      startedAlarm = millis();
      // Only work timers count as pomodoros; breaks do not. Flow stints are
      // counted a lap at a time, as they run.
      if (timerKind == TimerKind::Work) RtcState::data().pomodoroCount++;
    }
  }

  if (!paused && !countingUp() && remSeconds == 0) {
    // Asked for on every pass and started once: a repeating pattern that is
    // already playing is left to carry on. Not before a face is established,
    // where the timer reads as finished only because there is not one yet, and
    // a motor running in the hand would keep the cube from settling on a face.
    if (lastTimerFace != Orientation::UNDEFINED) Haptic::play(Haptic::Pattern::Alarm);
    Display::cycleTimerFinish();
    // Unannounced: thirty seconds of alarm has said everything a parting buzz
    // could.
    if (millis() - startedAlarm >= 1000 * 30) Util::deepSleep(Util::SleepMode::Off, false);
  }

  Haptic::cycle();

  // Last, so it sees the tick this pass produced rather than the one before it.
  Display::setBacklight(Util::backlightPercent(
      {millis() - lastFaceChange, remSeconds, countingUp(), sinceTap(), paused}));

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
