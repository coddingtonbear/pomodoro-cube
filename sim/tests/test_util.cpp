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
  // timer would divide by zero in updateTimer().
  CHECK(Util::getTimerByOrientation(Orientation::FACE_UP) == TIMER_WORK_SECONDS);
  CHECK(Util::getTimerByOrientation(Orientation::FACE_DOWN) == TIMER_WORK_SECONDS);
  CHECK(Util::getTimerByOrientation(Orientation::UNDEFINED) == TIMER_WORK_SECONDS);
}

}  // namespace

void testTimerSelection() {
  testFacesMapToTimers();
  testRestingFaces();
  testUnknownOrientationsFallBackToWork();
}
