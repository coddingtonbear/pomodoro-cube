// Host tests for tilt.cpp: the angle the face is drawn at, as distinct from
// the face the cube is on.
#include <cmath>

#include "check.h"
#include "consts.h"
#include "tilt.h"
#include "util.h"

namespace {

constexpr unsigned long kPassMs = 20;

bool near(float a, float b, float within = 0.01f) {
  return std::fabs(Tilt::shortestTurn(a, b)) <= within;
}

// The reading for a cube held at this angle in the plane of the screen, with
// this much of gravity in that plane.
struct Reading {
  float ax, ay;
};

Reading heldAt(float degrees, float inPlaneG = 1.0f) {
  const float radians = degrees * 3.14159265f / 180.0f;
  return {-inPlaneG * std::cos(radians), -inPlaneG * std::sin(radians)};
}

// Hold the cube at one angle for a while, the way the loop would see it.
// Returns how many passes asked for a redraw.
int hold(Tilt::Tracker &tilt, unsigned long &now, float degrees, unsigned long forMs) {
  const Reading r = heldAt(degrees);
  int redraws = 0;
  for (unsigned long t = 0; t < forMs; t += kPassMs) {
    now += kPassMs;
    if (tilt.update(true, r.ax, r.ay, now)) redraws++;
  }
  return redraws;
}

void testFacesAreDrawnWhereTheyAreFound() {
  // The readings Util::calcOrientation() names each face from have to point
  // the picture at that same face, or the timer and the drawing disagree.
  const struct {
    float ax, ay;
    Orientation face;
  } faces[] = {
      {-1.0f, 0.0f, Orientation::DEG_0},
      {0.0f, -1.0f, Orientation::DEG_90},
      {1.0f, 0.0f, Orientation::DEG_180},
      {0.0f, 1.0f, Orientation::DEG_270},
  };
  for (const auto &f : faces) {
    CHECK(Util::calcOrientation(f.ax, f.ay, 0.0f) == f.face);
    float degrees = -1.0f;
    CHECK(Tilt::inPlaneAngle(f.ax, f.ay, degrees));
    CHECK(near(degrees, Tilt::faceAngle(f.face)));
  }
}

void testAnglesBetweenFaces() {
  float degrees = 0.0f;
  const Reading r = heldAt(45.0f);
  CHECK(Tilt::inPlaneAngle(r.ax, r.ay, degrees));
  CHECK(near(degrees, 45.0f));

  // Tipped back, so less of gravity is in the plane: the same angle.
  const Reading tipped = heldAt(200.0f, 0.5f);
  CHECK(Tilt::inPlaneAngle(tipped.ax, tipped.ay, degrees));
  CHECK(near(degrees, 200.0f));
}

void testFlatHasNoAngle() {
  float degrees = 123.0f;
  CHECK(!Tilt::inPlaneAngle(0.0f, 0.0f, degrees));
  CHECK(!Tilt::inPlaneAngle(0.02f, -0.03f, degrees));
  const Reading r = heldAt(90.0f, TILT_MIN_INPLANE_G - 0.01f);
  CHECK(!Tilt::inPlaneAngle(r.ax, r.ay, degrees));
  CHECK(degrees == 123.0f);
}

void testShortestTurn() {
  CHECK(near(Tilt::shortestTurn(0.0f, 90.0f), 90.0f));
  CHECK(near(Tilt::shortestTurn(90.0f, 0.0f), -90.0f));
  // Across the join, where the long way round is the obvious arithmetic.
  CHECK(std::fabs(Tilt::shortestTurn(350.0f, 10.0f) - 20.0f) < 0.01f);
  CHECK(std::fabs(Tilt::shortestTurn(10.0f, 350.0f) + 20.0f) < 0.01f);
  CHECK(std::fabs(Tilt::shortestTurn(270.0f, 0.0f) - 90.0f) < 0.01f);
  CHECK(std::fabs(std::fabs(Tilt::shortestTurn(0.0f, 180.0f)) - 180.0f) < 0.01f);
}

void testSplit() {
  const struct {
    float degrees;
    int quarter, lean;
  } cases[] = {
      {0.0f, 0, 0},    {90.0f, 1, 0},    {180.0f, 2, 0},  {270.0f, 3, 0},
      {360.0f, 0, 0},  {30.0f, 0, 30},   {44.0f, 0, 44},  {46.0f, 1, -44},
      {100.0f, 1, 10}, {260.0f, 3, -10}, {359.0f, 0, -1}, {330.0f, 0, -30},
      {-10.0f, 0, -10}, {359.8f, 0, 0},
  };
  for (const auto &c : cases) {
    const Tilt::Split s = Tilt::split(c.degrees);
    CHECK(s.quarter == c.quarter);
    CHECK(s.lean == c.lean);
  }

  // Whichever way the halfway point goes, both halves describe the same angle
  // and the lean stays within what the panel's quarter turns leave over.
  for (int whole = 0; whole < 360; whole++) {
    const Tilt::Split s = Tilt::split((float)whole);
    CHECK(s.quarter < 4);
    CHECK(s.lean >= -45 && s.lean <= 45);
    CHECK((s.quarter * 90 + s.lean + 360) % 360 == whole);
  }
}

void testTheFirstReadingIsDrawnAsItStands() {
  Tilt::Tracker tilt;
  CHECK(!tilt.hasAngle());

  // Nothing to draw until something has been heard.
  CHECK(!tilt.update(false, 0.0f, 0.0f, 100));
  CHECK(!tilt.hasAngle());

  const Reading r = heldAt(180.0f);
  tilt.update(true, r.ax, r.ay, 120);
  CHECK(tilt.hasAngle());
  // No swing round from zero.
  CHECK(near(tilt.angle(), 180.0f));
}

void testAQuarterTurnIsEasedNotJumped() {
  Tilt::Tracker tilt;
  unsigned long now = 0;
  hold(tilt, now, 0.0f, 200);
  CHECK(near(tilt.angle(), 0.0f));

  // The cube is turned in one go, which is the worst case: the simulator does
  // exactly this, and a hand does something gentler.
  const Reading r = heldAt(90.0f);
  float last = 0.0f;
  float biggestStep = 0.0f;
  int redraws = 0;
  unsigned long arrivedAt = 0;
  for (int pass = 0; pass < 100; pass++) {
    now += kPassMs;
    if (tilt.update(true, r.ax, r.ay, now)) {
      redraws++;
      const float step = Tilt::shortestTurn(last, tilt.angle());
      // Always onwards, never back.
      CHECK(step > 0.0f);
      if (step > biggestStep) biggestStep = step;
      last = tilt.angle();
      if (arrivedAt == 0 && last == 90.0f) arrivedAt = now;
    }
  }

  // Ends on the face exactly, so the lean is zero and the drawing is cheap.
  CHECK(tilt.angle() == 90.0f);
  CHECK(redraws > 5);
  CHECK(biggestStep < 30.0f);
  // And in a time that reads as a turn rather than as a wait.
  CHECK(arrivedAt > 0);
  CHECK(arrivedAt - 200 >= 200);
  CHECK(arrivedAt - 200 <= 800);
}

void testATurnTakesAsLongHoweverSlowThePasses() {
  // A pass that redraws the whole face takes several times longer than one
  // that does not. The turn should take the same time either way, in fewer
  // frames, rather than the same number of frames in more time.
  unsigned long took[2] = {0, 0};
  const unsigned long passes[2] = {20, 80};
  for (int i = 0; i < 2; i++) {
    Tilt::Tracker tilt;
    unsigned long now = 0;
    hold(tilt, now, 0.0f, 200);
    const unsigned long from = now;
    const Reading r = heldAt(90.0f);
    while (tilt.angle() != 90.0f && now < 5000) {
      now += passes[i];
      tilt.update(true, r.ax, r.ay, now);
    }
    took[i] = now - from;
  }
  CHECK(took[0] > 0 && took[1] > 0);
  const long difference = (long)took[1] - (long)took[0];
  CHECK(difference > -160 && difference < 160);
}

void testTurnsGoTheShortWayRound() {
  Tilt::Tracker tilt;
  unsigned long now = 0;
  hold(tilt, now, 270.0f, 200);

  // 270 to 0 is a quarter turn onwards, not three back.
  const Reading r = heldAt(0.0f);
  for (int pass = 0; pass < 100; pass++) {
    now += kPassMs;
    if (tilt.update(true, r.ax, r.ay, now)) {
      const float angle = tilt.angle();
      CHECK(angle >= 270.0f || angle == 0.0f);
    }
  }
  CHECK(tilt.angle() == 0.0f);
}

void testNearAFaceIsDrawnSquare() {
  Tilt::Tracker tilt;
  unsigned long now = 0;
  // A desk that is not level.
  hold(tilt, now, 93.0f, 500);
  CHECK(tilt.angle() == 90.0f);

  Tilt::Tracker other;
  now = 0;
  hold(other, now, 357.0f, 500);
  CHECK(other.angle() == 0.0f);
}

void testHeldBetweenFacesIsDrawnBetweenFaces() {
  Tilt::Tracker tilt;
  unsigned long now = 0;
  hold(tilt, now, 45.0f, 500);
  CHECK(near(tilt.angle(), 45.0f, 1.0f));

  hold(tilt, now, 200.0f, 1000);
  CHECK(near(tilt.angle(), 200.0f, 1.0f));
}

void testItTakesMoreToLeaveAFaceThanToReachIt() {
  const float between = (TILT_SNAP_CAPTURE_DEG + TILT_SNAP_RELEASE_DEG) / 2.0f;

  // Square on a face, then nudged to between the two thresholds: still square.
  Tilt::Tracker settled;
  unsigned long now = 0;
  hold(settled, now, 90.0f, 300);
  hold(settled, now, 90.0f + between, 500);
  CHECK(settled.angle() == 90.0f);

  // Arriving at that same angle from well off the face: drawn where it is.
  Tilt::Tracker arriving;
  now = 0;
  hold(arriving, now, 130.0f, 300);
  hold(arriving, now, 90.0f + between, 500);
  CHECK(near(arriving.angle(), 90.0f + between, 1.0f));

  // Past the second threshold the face lets go.
  hold(settled, now, 90.0f + TILT_SNAP_RELEASE_DEG + 3.0f, 500);
  CHECK(near(settled.angle(), 90.0f + TILT_SNAP_RELEASE_DEG + 3.0f, 1.0f));
}

void testACubeAtRestIsNeverRedrawn() {
  Tilt::Tracker tilt;
  unsigned long now = 0;
  hold(tilt, now, 180.0f, 300);

  // The wander measured on the board at rest is about a degree and a half.
  // Deterministic, so a failure is the same failure every time.
  int redraws = 0;
  for (int pass = 0; pass < 500; pass++) {
    const float wander = 2.0f * std::sin((float)pass * 1.7f);
    const Reading r = heldAt(180.0f + wander);
    now += kPassMs;
    if (tilt.update(true, r.ax, r.ay, now)) redraws++;
  }
  CHECK(redraws == 0);
  CHECK(tilt.angle() == 180.0f);
}

void testHeldBetweenFacesSettles() {
  // Off a face there is nothing to snap to, so the smoothing and the redraw
  // step are all that stand between the noise and the panel.
  Tilt::Tracker tilt;
  unsigned long now = 0;
  hold(tilt, now, 46.0f, 500);

  int redraws = 0;
  for (int pass = 0; pass < 500; pass++) {
    const float wander = 1.5f * std::sin((float)pass * 1.7f);
    const Reading r = heldAt(46.0f + wander);
    now += kPassMs;
    if (tilt.update(true, r.ax, r.ay, now)) redraws++;
  }
  CHECK(redraws == 0);
}

void testLayingTheCubeDownHoldsTheAngle() {
  Tilt::Tracker tilt;
  unsigned long now = 0;
  hold(tilt, now, 270.0f, 300);

  // Onto its back: what is left in the plane is noise, pointing anywhere.
  int redraws = 0;
  for (int pass = 0; pass < 100; pass++) {
    const Reading r = heldAt((float)(pass * 37 % 360), 0.03f);
    now += kPassMs;
    if (tilt.update(true, r.ax, r.ay, now)) redraws++;
  }
  CHECK(redraws == 0);
  CHECK(tilt.angle() == 270.0f);
}

void testUntrustedReadingsAreIgnored() {
  Tilt::Tracker tilt;
  unsigned long now = 0;
  hold(tilt, now, 0.0f, 300);

  // Being carried: plenty in the plane, none of it gravity.
  const Reading r = heldAt(120.0f, 1.8f);
  int redraws = 0;
  for (int pass = 0; pass < 50; pass++) {
    now += kPassMs;
    if (tilt.update(false, r.ax, r.ay, now)) redraws++;
  }
  CHECK(redraws == 0);
  CHECK(tilt.angle() == 0.0f);
}

void testTheEaseRunsOnWithoutReadings() {
  // The motor shakes the accelerometer just as a face change is announced.
  // A turn already under way has to finish regardless.
  Tilt::Tracker tilt;
  unsigned long now = 0;
  hold(tilt, now, 0.0f, 200);
  // Long enough for the readings to have found the new face, not for the
  // picture to have reached it.
  hold(tilt, now, 90.0f, 160);
  CHECK(tilt.angle() != 90.0f);

  for (int pass = 0; pass < 50; pass++) {
    now += kPassMs;
    tilt.update(false, 0.0f, 0.0f, now);
  }
  CHECK(tilt.angle() == 90.0f);
}

void testALongGapIsNotATurn() {
  // Seeded, then nothing for a minute, then a reading a little off the face:
  // nothing here should be a jump.
  Tilt::Tracker tilt;
  tilt.seed(Tilt::faceAngle(Orientation::DEG_90));
  CHECK(tilt.hasAngle());
  CHECK(tilt.angle() == 90.0f);

  const Reading r = heldAt(92.0f);
  CHECK(!tilt.update(true, r.ax, r.ay, 60000));
  CHECK(tilt.angle() == 90.0f);
}

}  // namespace

void testTilt() {
  testFacesAreDrawnWhereTheyAreFound();
  testAnglesBetweenFaces();
  testFlatHasNoAngle();
  testShortestTurn();
  testSplit();
  testTheFirstReadingIsDrawnAsItStands();
  testAQuarterTurnIsEasedNotJumped();
  testATurnTakesAsLongHoweverSlowThePasses();
  testTurnsGoTheShortWayRound();
  testNearAFaceIsDrawnSquare();
  testHeldBetweenFacesIsDrawnBetweenFaces();
  testItTakesMoreToLeaveAFaceThanToReachIt();
  testACubeAtRestIsNeverRedrawn();
  testHeldBetweenFacesSettles();
  testLayingTheCubeDownHoldsTheAngle();
  testUntrustedReadingsAreIgnored();
  testTheEaseRunsOnWithoutReadings();
  testALongGapIsNotATurn();
}
