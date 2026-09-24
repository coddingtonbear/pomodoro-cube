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

void testFlowBreakIsAFifthOfTheStint() {
  // The pair flow mode replaced: fifty minutes of work bought ten of break.
  CHECK(Util::flowBreakSeconds(50 * 60) == 10 * 60);
  CHECK(Util::flowBreakSeconds(25 * 60) == 5 * 60);
  CHECK(Util::flowBreakSeconds(2 * 60 * 60) == 24 * 60);

  // And the break face is where that shows up.
  CHECK(Util::getTimerSpec(Orientation::DEG_270, 50 * 60).seconds == 10 * 60);
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

void testTooShortAStintEarnsTheFallbackBreak() {
  // Turning the cube through the flow face on the way somewhere else must not
  // leave a break of a few seconds, which would beep the moment it started.
  CHECK(Util::flowBreakSeconds(0) == TIMER_LONG_BREAK_SECONDS);
  CHECK(Util::flowBreakSeconds(FLOW_MIN_STINT_SECONDS - 1) == TIMER_LONG_BREAK_SECONDS);
  CHECK(Util::flowBreakSeconds(-1) == TIMER_LONG_BREAK_SECONDS);

  // At the threshold it earns its fifth, however little that is.
  CHECK(Util::flowBreakSeconds(FLOW_MIN_STINT_SECONDS) ==
        FLOW_MIN_STINT_SECONDS / FLOW_BREAK_DIVISOR);
}

void testRestingFaces() {
  // Which sign is which is an unverified guess; that the two are distinguished
  // at all, and that both rest rather than run a timer, is the contract.
  CHECK(Util::calcOrientation(0.0f, 0.0f, 1.0f) == Orientation::FACE_UP);
  CHECK(Util::calcOrientation(0.0f, 0.0f, -1.0f) == Orientation::FACE_DOWN);

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

}  // namespace

void testTimerSelection() {
  testFacesMapToTimers();
  testFixedFaceLengths();
  testFlowWorkHasNoLength();
  testFlowBreakIsAFifthOfTheStint();
  testAStintScoresALapAtATime();
  testTooShortAStintEarnsTheFallbackBreak();
  testRestingFaces();
  testUnknownOrientationsFallBackToWork();
}
