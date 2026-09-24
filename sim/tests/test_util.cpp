// Host tests for the parts of util.cpp that are pure logic. Built by the
// simulator's CMake project; run with `ctest --test-dir sim/build`.
#include "check.h"
#include "consts.h"
#include "util.h"

namespace {

// The accelerometer reading for a cube resting on each face, matching what
// sim_qmi.cpp synthesises. calcOrientation() must map these back to the same
// face, which is what makes the simulator's faces mean anything.
struct Face {
  const char *name;
  float ax, ay, az;
  Orientation expected;
  TimerKind expectedKind;
  TimerMode expectedMode;
};

// In clockwise order from the default orientation.
constexpr Face kFaces[] = {
    {"default", -1.0f, 0.0f, 0.0f, Orientation::DEG_0, TimerKind::Work, TimerMode::Countdown},
    {"clockwise x1", 0.0f, -1.0f, 0.0f, Orientation::DEG_90, TimerKind::Break, TimerMode::Countdown},
    {"clockwise x2", 1.0f, 0.0f, 0.0f, Orientation::DEG_180, TimerKind::Work, TimerMode::CountUp},
    {"clockwise x3", 0.0f, 1.0f, 0.0f, Orientation::DEG_270, TimerKind::Break, TimerMode::Countdown},
};

void testFacesMapToTimers() {
  for (const Face &face : kFaces) {
    const Orientation ori = Util::calcOrientation(face.ax, face.ay, face.az);
    CHECK_MSG(ori == face.expected, face.name);

    const Util::TimerSpec spec = Util::getTimerSpec(ori, 0);
    CHECK_MSG(spec.kind == face.expectedKind, face.name);
    CHECK_MSG(spec.mode == face.expectedMode, face.name);
  }
}

void testFixedFaceLengths() {
  CHECK(Util::getTimerSpec(Orientation::DEG_0, 0).seconds == TIMER_WORK_SECONDS);
  CHECK(Util::getTimerSpec(Orientation::DEG_90, 0).seconds == TIMER_SHORT_BREAK_SECONDS);
}

void testFlowWorkHasNoLength() {
  // There is nothing to count down to, and a caller that treated the length as
  // one would start a zero-second timer.
  CHECK(Util::getTimerSpec(Orientation::DEG_180, 0).seconds == 0);

  // Earned time belongs to the break face; the work face ignores it.
  CHECK(Util::getTimerSpec(Orientation::DEG_180, 3000).seconds == 0);
}

void testAStintCreditsAFifthToTheBank() {
  // The pair flow mode replaced: fifty minutes of work bought ten of break.
  CHECK(Util::flowBreakCredit(50 * 60) == 10 * 60);
  CHECK(Util::flowBreakCredit(25 * 60) == 5 * 60);
  CHECK(Util::flowBreakCredit(2 * 60 * 60) == 24 * 60);
  CHECK(Util::flowBreakCredit(0) == 0);
  CHECK(Util::flowBreakCredit(-1) == 0);
}

void testTheBreakFaceCountsTheBankDown() {
  // Whatever is banked is what the break runs for, with no conversion left to
  // do: the fifth was taken on the way in.
  CHECK(Util::getTimerSpec(Orientation::DEG_270, 9 * 60).seconds == 9 * 60);

  // Down to the smallest balance. A floor would have to be conjured from
  // nowhere and then written back, leaving the bank saying something untrue.
  CHECK(Util::getTimerSpec(Orientation::DEG_270, 12).seconds == 12);

  // Only the break face writes back; the fixed faces leave the bank alone.
  CHECK(Util::getTimerSpec(Orientation::DEG_270, 9 * 60).spendsBank);
  CHECK(!Util::getTimerSpec(Orientation::DEG_0, 9 * 60).spendsBank);
  CHECK(!Util::getTimerSpec(Orientation::DEG_90, 9 * 60).spendsBank);
  CHECK(!Util::getTimerSpec(Orientation::DEG_180, 9 * 60).spendsBank);
}

void testWorkedThenBankedMatchesTheWholeJourney() {
  // The worked example: 25 minutes of flow, a 5 minute break, a minute of it
  // taken, another 25 minutes of flow -- and the break that follows is 9.
  int bank = 0;
  bank += Util::flowBreakCredit(25 * 60);
  CHECK(Util::getTimerSpec(Orientation::DEG_270, bank).seconds == 5 * 60);

  bank = 4 * 60;  // a minute of it spent, written back by the running break
  bank += Util::flowBreakCredit(25 * 60);
  CHECK(Util::getTimerSpec(Orientation::DEG_270, bank).seconds == 9 * 60);
}

void testTheBankPreviewIncludesTheRunningStint() {
  // Nothing worked yet: just the balance.
  CHECK(Util::flowBankPreview(0, 0) == 0);
  CHECK(Util::flowBankPreview(300, 0) == 300);

  // Then it climbs a second for every five worked, on top of what was there.
  CHECK(Util::flowBankPreview(0, 25 * 60) == 5 * 60);
  CHECK(Util::flowBankPreview(4 * 60, 25 * 60) == 9 * 60);
  CHECK(Util::flowBankPreview(0, 5) == 1);

  // Clamped where the bank is, so the figure on screen is one the cube can
  // actually honour.
  CHECK(Util::flowBankPreview(FLOW_MAX_SECONDS, FLOW_MAX_SECONDS) == FLOW_MAX_SECONDS);
  CHECK(Util::flowBankPreview(-5, 0) == 0);
}

void testBothFlowFacesInvertThePanel() {
  // The break face is spending the bank just as much as the work face is
  // filling it, so both wear flow's colours.
  CHECK(Util::getTimerSpec(Orientation::DEG_180, 0).flow);
  CHECK(Util::getTimerSpec(Orientation::DEG_270, 0).flow);
  CHECK(!Util::getTimerSpec(Orientation::DEG_0, 0).flow);
  CHECK(!Util::getTimerSpec(Orientation::DEG_90, 0).flow);
  CHECK(!Util::getTimerSpec(Orientation::FACE_UP, 0).flow);

  // Spending the bank implies being one of flow's faces, never the other way
  // round: the work face fills it rather than draining it.
  CHECK(Util::getTimerSpec(Orientation::DEG_270, 0).spendsBank);
  CHECK(!Util::getTimerSpec(Orientation::DEG_180, 0).spendsBank);
}

void testAStintScoresALapAtATime() {
  // Nothing yet, and nothing for the seconds that are not a lap.
  CHECK(!Util::completesFlowLap(0));
  CHECK(!Util::completesFlowLap(-1));
  CHECK(!Util::completesFlowLap(1));
  CHECK(!Util::completesFlowLap(FLOW_LAP_SECONDS - 1));
  CHECK(!Util::completesFlowLap(FLOW_LAP_SECONDS + 1));

  // One on the second each lap closes, which is the second the arc comes back
  // round -- so a two hour stint is worth four, not one.
  CHECK(Util::completesFlowLap(FLOW_LAP_SECONDS));
  CHECK(Util::completesFlowLap(2 * FLOW_LAP_SECONDS));
  CHECK(Util::completesFlowLap(4 * FLOW_LAP_SECONDS));

  // A lap is the fixed work face's length, so a pomodoro is the same amount of
  // work whichever face counted it.
  CHECK(FLOW_LAP_SECONDS == TIMER_WORK_SECONDS);
}

void testAnEmptyBankIsABreakOfNoLength() {
  // Not a fallback to some other length: anything conjured here would be break
  // time nobody worked for. It finishes on the spot instead.
  CHECK(Util::getTimerSpec(Orientation::DEG_270, 0).seconds == 0);
  CHECK(Util::getTimerSpec(Orientation::DEG_270, -1).seconds == 0);

  // Still a break, and still the face that owns the bank -- there is just
  // nothing in it to spend.
  CHECK(Util::getTimerSpec(Orientation::DEG_270, 0).kind == TimerKind::Break);
  CHECK(Util::getTimerSpec(Orientation::DEG_270, 0).mode == TimerMode::Countdown);

  // The fixed faces still report a real length, so nothing else can reach zero:
  // a zero-length countdown on one of those would divide by zero in the arc.
  CHECK(Util::getTimerSpec(Orientation::DEG_0, 0).seconds == TIMER_WORK_SECONDS);
  CHECK(Util::getTimerSpec(Orientation::DEG_90, 0).seconds == TIMER_SHORT_BREAK_SECONDS);
  CHECK(Util::getTimerSpec(Orientation::FACE_UP, 0).seconds == TIMER_WORK_SECONDS);
}

void testRestingFaces() {
  // The signs are the ones the board actually reads: +Z points into the back of
  // the cube, so screen-up is -1g on Z.
  CHECK(Util::calcOrientation(0.0f, 0.0f, -1.0f) == Orientation::FACE_UP);
  CHECK(Util::calcOrientation(0.0f, 0.0f, 1.0f) == Orientation::FACE_DOWN);

  CHECK(Util::isRestingFace(Orientation::FACE_UP));
  CHECK(Util::isRestingFace(Orientation::FACE_DOWN));
  for (const Face &face : kFaces) {
    CHECK_MSG(!Util::isRestingFace(face.expected), face.name);
  }
}

void testUnknownOrientationsFallBackToWork() {
  // A resting face runs no timer, but must still not report zero: a zero-length
  // countdown would divide by zero in the arc.
  const Orientation resting[] = {Orientation::FACE_UP, Orientation::FACE_DOWN,
                                 Orientation::UNDEFINED};
  for (const Orientation ori : resting) {
    const Util::TimerSpec spec = Util::getTimerSpec(ori, 0);
    CHECK(spec.seconds == TIMER_WORK_SECONDS);
    CHECK(spec.mode == TimerMode::Countdown);
  }
}

// Setting the cube down parks what was running. The two resting faces differ
// only in the screen, so everything below is asserted for both of them.
constexpr Orientation kRestingFaces[] = {Orientation::FACE_UP, Orientation::FACE_DOWN};

void testOnlyFaceUpStaysLit() {
  RtcState::Data data;
  RtcState::initialise(data);

  const Util::RestPlan up =
      Util::restOnFace(data, Orientation::FACE_UP, Orientation::DEG_0, 900, 1500, false);
  CHECK(up.lit);
  CHECK(up.mode == Util::SleepMode::Paused);

  const Util::RestPlan down =
      Util::restOnFace(data, Orientation::FACE_DOWN, Orientation::DEG_0, 900, 1500, false);
  CHECK(!down.lit);
  CHECK(down.mode == Util::SleepMode::Off);
}

// The regression this pair guards: face down used to empty the bank on its way
// into sleep, so a break earned before putting the cube away was gone when it
// came back.
void testBothRestingFacesKeepTheBank() {
  for (const Orientation ori : kRestingFaces) {
    RtcState::Data data;
    RtcState::initialise(data);
    RtcState::addFlowBank(data, 600);

    Util::restOnFace(data, ori, Orientation::DEG_180, 1800, 0, true);
    CHECK(RtcState::flowBank(data) == 600);
  }
}

void testBothRestingFacesParkTheTimer() {
  for (const Orientation ori : kRestingFaces) {
    RtcState::Data data;
    RtcState::initialise(data);

    Util::restOnFace(data, ori, Orientation::DEG_0, 900, 1500, false);
    CHECK(RtcState::hasPause(data));

    // Parked against the face it was running on, not the face it is resting on,
    // so picking the cube up onto that face resumes where it left off.
    int remaining = 0;
    int selected = 0;
    bool countingUp = true;
    CHECK(RtcState::takePause(data, Orientation::DEG_0, remaining, selected, countingUp));
    CHECK(remaining == 900);
    CHECK(selected == 1500);
    CHECK(!countingUp);
  }
}

void testAParkedFlowStintIsStillCountingUp() {
  for (const Orientation ori : kRestingFaces) {
    RtcState::Data data;
    RtcState::initialise(data);

    // A flow stint carries no selected length; the elapsed time is the whole of
    // it, and it has to come back counting up rather than down.
    Util::restOnFace(data, ori, Orientation::DEG_180, 742, 0, true);

    int remaining = 0;
    int selected = 0;
    bool countingUp = false;
    CHECK(RtcState::takePause(data, Orientation::DEG_180, remaining, selected, countingUp));
    CHECK(remaining == 742);
    CHECK(countingUp);
  }
}

void testRestingWithNothingRunningParksNothing() {
  for (const Orientation ori : kRestingFaces) {
    RtcState::Data data;
    RtcState::initialise(data);
    RtcState::addFlowBank(data, 300);

    // A finished timer has nothing worth resuming, but the bank is a balance
    // rather than a handoff and is not touched either way.
    Util::restOnFace(data, ori, Orientation::DEG_0, 0, 1500, false);
    CHECK(!RtcState::hasPause(data));
    CHECK(RtcState::flowBank(data) == 300);
  }
}

// Full brightness is rationed to the moments worth looking at. Everything here
// is in milliseconds since the last face change.
constexpr unsigned long kJustSetDown = 0;
constexpr unsigned long kSettled = (unsigned long)BACKLIGHT_ATTENTION_SECONDS * 1000UL + 1;
// Long enough ago that no tap is holding the panel up. Written as the largest
// value rather than a big number, because that is what the firmware passes
// before the cube has been tapped at all.
constexpr unsigned long kNoTap = ~0UL;
constexpr unsigned long kJustTapped = 0;

void testAFaceChangeLightsThePanel() {
  CHECK(Util::backlightPercent({kJustSetDown, TIMER_WORK_SECONDS, false, kNoTap}) ==
        BACKLIGHT_FULL_PERCENT);

  // Right up to the boundary, and dim immediately after it.
  const unsigned long lastBrightMs = (unsigned long)BACKLIGHT_ATTENTION_SECONDS * 1000UL - 1;
  CHECK(Util::backlightPercent({lastBrightMs, TIMER_WORK_SECONDS, false, kNoTap}) ==
        BACKLIGHT_FULL_PERCENT);
  CHECK(Util::backlightPercent({kSettled, TIMER_WORK_SECONDS, false, kNoTap}) ==
        BACKLIGHT_IDLE_PERCENT);
}

void testTheLastSecondsOfACountdownLightThePanel() {
  CHECK(Util::backlightPercent({kSettled, BACKLIGHT_ATTENTION_SECONDS + 1, false, kNoTap}) ==
        BACKLIGHT_IDLE_PERCENT);
  CHECK(Util::backlightPercent({kSettled, BACKLIGHT_ATTENTION_SECONDS, false, kNoTap}) ==
        BACKLIGHT_FULL_PERCENT);
  CHECK(Util::backlightPercent({kSettled, 1, false, kNoTap}) == BACKLIGHT_FULL_PERCENT);
}

// A timer at zero is beeping and wants to be seen across a room. It is the same
// test as "about to run out", which is why the policy does not need telling
// about the finished state separately.
void testAFinishedTimerStaysLit() {
  CHECK(Util::backlightPercent({kSettled, 0, false, kNoTap}) == BACKLIGHT_FULL_PERCENT);
}

// A stint has no end to approach, so it dims and stays dim however long it runs.
// The moment worth lighting is turning the cube off it, and that is a face
// change like any other.
void testAFlowStintDimsAndStaysDim() {
  CHECK(Util::backlightPercent({kJustSetDown, 0, true, kNoTap}) == BACKLIGHT_FULL_PERCENT);
  CHECK(Util::backlightPercent({kSettled, 0, true, kNoTap}) == BACKLIGHT_IDLE_PERCENT);
  CHECK(Util::backlightPercent({kSettled, 1, true, kNoTap}) == BACKLIGHT_IDLE_PERCENT);
  CHECK(Util::backlightPercent({kSettled, FLOW_MAX_SECONDS, true, kNoTap}) == BACKLIGHT_IDLE_PERCENT);
}

// A tap is someone asking to read a face that has gone dim. It buys longer than
// a face change does, because they are coming to it cold.
void testATapLightsThePanel() {
  CHECK(Util::backlightPercent({kSettled, TIMER_WORK_SECONDS, false, kJustTapped}) ==
        BACKLIGHT_FULL_PERCENT);

  const unsigned long lastBrightMs = (unsigned long)BACKLIGHT_TAP_SECONDS * 1000UL - 1;
  CHECK(Util::backlightPercent({kSettled, TIMER_WORK_SECONDS, false, lastBrightMs}) ==
        BACKLIGHT_FULL_PERCENT);

  const unsigned long expiredMs = (unsigned long)BACKLIGHT_TAP_SECONDS * 1000UL;
  CHECK(Util::backlightPercent({kSettled, TIMER_WORK_SECONDS, false, expiredMs}) ==
        BACKLIGHT_IDLE_PERCENT);
}

// A stint is the face most worth being able to glance at: it never brightens on
// its own, so a tap is the only way to read it without picking the cube up.
void testATapLightsARunningFlowStint() {
  CHECK(Util::backlightPercent({kSettled, 0, true, kJustTapped}) == BACKLIGHT_FULL_PERCENT);
  CHECK(Util::backlightPercent({kSettled, 0, true, kNoTap}) == BACKLIGHT_IDLE_PERCENT);
}

// The debouncer decides which face the cube is on, and a cube in a hand passes
// through faces it is not being put down on. Timestamps are supplied rather than
// read from millis(), so the whole of ORI_DEBOUNCE_DELAY can pass in no time.
// The unit vector for a cube sitting square on each face, as sim_qmi.cpp
// synthesises it and the board reads it.
struct Vector { float x, y, z; };
constexpr Vector kUp = {0.0f, 0.0f, -1.0f};
constexpr Vector kDown = {0.0f, 0.0f, 1.0f};
constexpr Vector kDeg0 = {-1.0f, 0.0f, 0.0f};
constexpr Vector kDeg90 = {0.0f, -1.0f, 0.0f};
constexpr Vector kDeg180 = {1.0f, 0.0f, 0.0f};

bool feed(const Vector &v, unsigned long nowMs) {
  return Util::updateOriDebounce(v.x, v.y, v.z, nowMs);
}

void testAReadingMustHoldStillToBeAccepted() {
  Util::resetOriDebounce();

  // Not on the first sighting, however long the clock has been running.
  CHECK(!feed(kDeg0, 10000));
  CHECK(!feed(kDeg0, 10000 + ORI_DEBOUNCE_DELAY - 1));
  CHECK(Util::getDebouncedOriState() == Orientation::UNDEFINED);

  CHECK(feed(kDeg0, 10000 + ORI_DEBOUNCE_DELAY));
  CHECK(Util::getDebouncedOriState() == Orientation::DEG_0);

  // And only once: the same face held longer is not a second change.
  CHECK(!feed(kDeg0, 20000));
}

// The bug this was written for. A cube picked up off its face-up rest and stood
// on a timer face reads FACE_UP on the way, and the old debouncer -- which timed
// from the last sample that agreed with the face being left rather than from the
// last change -- would accept whichever reading landed at the end of the window.
// Landing on FACE_UP there is what parked the cube again and left the paused
// frame on a panel that should have gone back to work.
void testAFaceInPassingIsNotAcceptedAsAFaceChange() {
  Util::resetOriDebounce();
  CHECK(!feed(kDeg0, 0));
  CHECK(feed(kDeg0, ORI_DEBOUNCE_DELAY));

  // Lifted: a tumble through several faces, none held for long.
  unsigned long now = 1000;
  const Vector tumble[] = {kUp, {0.0f, 1.0f, 0.0f}, kUp, kDeg90, kUp};
  for (const Vector &v : tumble) {
    now += ORI_DEBOUNCE_DELAY - 1;
    CHECK_MSG(!feed(v, now), "a face in passing was accepted");
  }
  CHECK(Util::getDebouncedOriState() == Orientation::DEG_0);

  // Set down at last, and held.
  CHECK(!feed(kDeg180, now + 1));
  CHECK(feed(kDeg180, now + 1 + ORI_DEBOUNCE_DELAY));
  CHECK(Util::getDebouncedOriState() == Orientation::DEG_180);
}

// Every reading names a face. The old classifier tested each axis against a
// fixed +-0.8 and fell through to DEG_0 when nothing matched, which reported a
// timer face for any attitude more than about 37 degrees off an axis.
void testEveryAttitudeNamesAFace() {
  // Straight off the board: resting, |a| = 1.01, and 37 degrees off the Y axis.
  // Nothing here reaches 0.8, so the old code called this DEG_0 by fall-through
  // -- and the pause the cube was carrying had been stored against DEG_90, so
  // standing the cube back up threw it away and started a fresh 25 minutes.
  CHECK(Util::calcOrientation(-0.138f, -0.796f, -0.610f) == Orientation::DEG_90);

  // The halfway point between two faces still names one rather than falling
  // through, and isDecisive() is what stops it moving the cube there.
  CHECK(Util::calcOrientation(0.0f, -0.707f, -0.707f) == Orientation::FACE_UP);
  CHECK(!Util::isDecisive(0.0f, -0.707f, -0.707f));
  CHECK(Util::isDecisive(-0.138f, -0.796f, -0.610f));
}

// A reading has to be gravity and little else before it says anything at all.
void testOnlyGravityNamesAFace() {
  // The first samples after QMI::setup(), captured from the board.
  CHECK(!Util::isGravityOnly(0.0f, 0.0f, 0.0f));
  CHECK(!Util::isGravityOnly(-1.291f, -1.774f, -1.460f));
  // Mid-tumble, also from the board.
  CHECK(!Util::isGravityOnly(0.094f, 0.405f, 1.677f));
  // At rest.
  CHECK(Util::isGravityOnly(-0.138f, -0.796f, -0.610f));
  CHECK(Util::isGravityOnly(0.0f, 0.0f, -1.0f));
}

// The bug the board actually showed: sitting perfectly still at an attitude
// that straddles a threshold, it alternated between two faces, restarting a
// timer each time. Replayed from real samples, whose Y component crosses -0.8
// in both directions while the cube has not moved at all.
void testAMotionlessCubeDoesNotChangeFace() {
  Util::resetOriDebounce();

  const Vector resting[] = {
      {-0.139f, -0.795f, -0.610f}, {-0.137f, -0.796f, -0.609f},
      {-0.138f, -0.858f, -0.612f}, {-0.136f, -0.780f, -0.604f},
      {-0.142f, -0.798f, -0.612f}, {-0.138f, -0.792f, -0.604f},
      {-0.137f, -0.858f, -0.611f}, {-0.140f, -0.794f, -0.611f},
  };

  unsigned long now = 0;
  int changes = 0;
  for (int pass = 0; pass < 40; pass++) {
    for (const Vector &v : resting) {
      now += 20;
      if (feed(v, now)) changes++;
    }
  }

  // One: the cube arriving on the face it has been on all along.
  CHECK(changes == 1);
  CHECK(Util::getDebouncedOriState() == Orientation::DEG_90);
}

}  // namespace

void testBacklightPolicy() {
  testAFaceChangeLightsThePanel();
  testTheLastSecondsOfACountdownLightThePanel();
  testAFinishedTimerStaysLit();
  testAFlowStintDimsAndStaysDim();
  testATapLightsThePanel();
  testATapLightsARunningFlowStint();
}

// A cube poised between two faces has to be put on one of them. The margin
// that stops it alternating needs a face to alternate with; before there is
// one it would only withhold an answer, which on the board meant nineteen
// seconds awake showing no timer. These are the real samples from that.
void testAnAmbiguousAttitudeStillNamesAFace() {
  Util::resetOriDebounce();

  const Vector poised = {-0.068f, -0.737f, -0.696f};
  CHECK(!Util::isDecisive(poised.x, poised.y, poised.z));

  unsigned long now = 0;
  int changes = 0;
  for (int i = 0; i < 200; i++) {
    now += 20;
    if (feed(poised, now)) changes++;
  }

  // Once, and then it stands: the margin holds the face it settled on.
  CHECK(changes == 1);
  CHECK(Util::getDebouncedOriState() == Orientation::DEG_90);

  // And the face it settled on is not given up to the other side of the
  // halfway line without a decisive reading to justify it.
  const Vector justOver = {-0.068f, -0.696f, -0.737f};
  CHECK(!Util::isDecisive(justOver.x, justOver.y, justOver.z));
  for (int i = 0; i < 200; i++) {
    now += 20;
    CHECK_MSG(!feed(justOver, now), "a narrow reading moved an established face");
  }
  CHECK(Util::getDebouncedOriState() == Orientation::DEG_90);

  // A clear reading still moves it.
  CHECK(!feed(kUp, now + 20));
  CHECK(feed(kUp, now + 20 + ORI_DEBOUNCE_DELAY));
  CHECK(Util::getDebouncedOriState() == Orientation::FACE_UP);
}

void testOrientationDebounce() {
  testAnAmbiguousAttitudeStillNamesAFace();
  testAReadingMustHoldStillToBeAccepted();
  testAFaceInPassingIsNotAcceptedAsAFaceChange();
  testEveryAttitudeNamesAFace();
  testOnlyGravityNamesAFace();
  testAMotionlessCubeDoesNotChangeFace();
}

void testRestingFaceParking() {
  testOnlyFaceUpStaysLit();
  testBothRestingFacesKeepTheBank();
  testBothRestingFacesParkTheTimer();
  testAParkedFlowStintIsStillCountingUp();
  testRestingWithNothingRunningParksNothing();
}

void testTimerSelection() {
  testFacesMapToTimers();
  testFixedFaceLengths();
  testFlowWorkHasNoLength();
  testAStintCreditsAFifthToTheBank();
  testTheBreakFaceCountsTheBankDown();
  testWorkedThenBankedMatchesTheWholeJourney();
  testTheBankPreviewIncludesTheRunningStint();
  testBothFlowFacesInvertThePanel();
  testAStintScoresALapAtATime();
  testAnEmptyBankIsABreakOfNoLength();
  testRestingFaces();
  testUnknownOrientationsFallBackToWork();
}
