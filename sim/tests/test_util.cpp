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
  int expectedSeconds;
};

// In clockwise order from the default orientation.
constexpr Face kFaces[] = {
    {"default", -1.0f, 0.0f, 0.0f, Orientation::DEG_0, 25 * 60},
    {"clockwise x1", 0.0f, -1.0f, 0.0f, Orientation::DEG_90, 5 * 60},
    {"clockwise x2", 1.0f, 0.0f, 0.0f, Orientation::DEG_180, 50 * 60},
    {"clockwise x3", 0.0f, 1.0f, 0.0f, Orientation::DEG_270, 10 * 60},
};

void testFacesMapToTimers() {
  for (const Face &face : kFaces) {
    const Orientation ori = Util::calcOrientation(face.ax, face.ay, face.az);
    CHECK_MSG(ori == face.expected, face.name);
    CHECK_MSG(Util::getTimerByOrientation(ori) == face.expectedSeconds, face.name);
  }
}

void testFaceDownSleeps() {
  CHECK(Util::calcOrientation(0.0f, 0.0f, 1.0f) == Orientation::SLEEP);
  CHECK(Util::calcOrientation(0.0f, 0.0f, -1.0f) == Orientation::SLEEP);
}

void testUnknownOrientationsFallBackToWork() {
  // Neither face is SLEEP, so the cube should still be usable rather than
  // showing a zero-length timer that would divide by zero in updateTimer().
  CHECK(Util::getTimerByOrientation(Orientation::SLEEP) == TIMER_WORK_SECONDS);
  CHECK(Util::getTimerByOrientation(Orientation::UNDEFINED) == TIMER_WORK_SECONDS);
}

}  // namespace

void testTimerSelection() {
  testFacesMapToTimers();
  testFaceDownSleeps();
  testUnknownOrientationsFallBackToWork();
}
