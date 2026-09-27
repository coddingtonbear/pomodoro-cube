#include "tilt.h"

#include <math.h>

namespace {

constexpr float kDegreesPerRadian = 57.29577951f;

// Close enough to the target to call it reached. An exponential ease never
// arrives on its own, and a face that is forever a fraction of a degree short
// of a quarter turn is forever being drawn the expensive way.
constexpr float kArrivedDegrees = 0.5f;

// The longest gap between two passes that is treated as one step. Past this
// the loop was held up by something, and the honest answer is to carry on from
// where it is rather than to account for time nobody saw.
constexpr unsigned long kLongestStepMs = 250;

float wrap(float degrees) {
  degrees = fmodf(degrees, 360.0f);
  if (degrees < 0.0f) degrees += 360.0f;
  // fmodf of a value a hair under zero rounds the sum up to 360 itself.
  if (degrees >= 360.0f) degrees = 0.0f;
  return degrees;
}

float nearestQuarter(float degrees) {
  return wrap(90.0f * roundf(degrees / 90.0f));
}

// How far a first-order filter with this time constant moves towards its input
// over this long. Worked out from the clock rather than fixed per pass: a pass
// that redraws the whole face takes several times longer than one that does
// not, and a turn should take as long either way.
float blend(unsigned long elapsedMs, int timeConstantMs) {
  if (elapsedMs == 0) return 0.0f;
  return 1.0f - expf(-(float)elapsedMs / (float)timeConstantMs);
}

}  // namespace

float Tilt::faceAngle(Orientation ori) {
  switch (ori) {
    case Orientation::DEG_90: return 90.0f;
    case Orientation::DEG_180: return 180.0f;
    case Orientation::DEG_270: return 270.0f;
    default: return 0.0f;
  }
}

float Tilt::shortestTurn(float from, float to) {
  float turn = fmodf(to - from, 360.0f);
  if (turn > 180.0f) turn -= 360.0f;
  if (turn <= -180.0f) turn += 360.0f;
  return turn;
}

bool Tilt::inPlaneAngle(float ax, float ay, float &degrees) {
  if (sqrtf(ax * ax + ay * ay) < TILT_MIN_INPLANE_G) return false;
  // Negated to land on the faces where Util::calcOrientation() puts them: the
  // default face reads -1 g on X, and a quarter turn clockwise -1 g on Y.
  degrees = wrap(atan2f(-ay, -ax) * kDegreesPerRadian);
  return true;
}

Tilt::Split Tilt::split(float degrees) {
  const int whole = (int)lroundf(wrap(degrees)) % 360;
  // To the nearest quarter, with the halfway point going up.
  const int quarter = ((whole + 45) / 90) % 4;
  int lean = whole - quarter * 90;
  if (lean > 45) lean -= 360;
  return {(uint8_t)quarter, lean};
}

void Tilt::Tracker::seed(float degrees) {
  target_ = wrap(degrees);
  eased_ = target_;
  shown_ = target_;
  snapped_ = target_ == nearestQuarter(target_);
  hasAngle_ = true;
}

void Tilt::Tracker::retarget(float reading) {
  // A cube stood on a face is held at exactly that face's angle, and it takes
  // more to pull it off than it took to put it there. Without this the noise
  // in a resting reading is a face that never stops being redrawn, at an angle
  // just far enough off square to blur the digits.
  if (snapped_) {
    if (fabsf(shortestTurn(target_, reading)) <= TILT_SNAP_RELEASE_DEG) return;
    snapped_ = false;
  }

  const float quarter = nearestQuarter(reading);
  if (fabsf(shortestTurn(quarter, reading)) <= TILT_SNAP_CAPTURE_DEG) {
    target_ = quarter;
    snapped_ = true;
  } else {
    target_ = reading;
  }

  // The first angle there is gets drawn as it stands. There is nothing to ease
  // from, and easing from zero would swing the face round on every wake.
  if (!hasAngle_) {
    eased_ = target_;
    shown_ = target_;
    hasAngle_ = true;
  }
}

bool Tilt::Tracker::update(bool trusted, float ax, float ay, unsigned long nowMs) {
  unsigned long elapsed = started_ ? nowMs - lastMs_ : 0;
  if (elapsed > kLongestStepMs) elapsed = kLongestStepMs;
  lastMs_ = nowMs;
  started_ = true;

  if (trusted) {
    // Smoothed as a vector rather than as an angle, so a reading counts for as
    // much as there is of it: one taken with the cube nearly flat has little
    // in the plane and says little about the angle, and moves this little.
    if (!sensed_) {
      senseX_ = ax;
      senseY_ = ay;
      sensed_ = true;
    } else {
      const float weight = blend(elapsed, TILT_SENSE_SMOOTHING_MS);
      senseX_ += weight * (ax - senseX_);
      senseY_ += weight * (ay - senseY_);
    }

    // No angle to be had leaves the target where it was, which is what holds
    // the face steady as the cube is laid on its back.
    float reading;
    if (inPlaneAngle(senseX_, senseY_, reading)) retarget(reading);
  }

  if (!hasAngle_) return false;

  const float left = shortestTurn(eased_, target_);
  if (fabsf(left) <= kArrivedDegrees) eased_ = target_;
  else eased_ = wrap(eased_ + left * blend(elapsed, TILT_EASE_MS));

  // Arriving on a face is always reported, however short the last step, so the
  // picture ends up square rather than within a step of it. Only on a face:
  // anywhere else the target is a reading, which never stops moving a little,
  // and arriving at each new one would be a redraw on every pass.
  const bool arrived = snapped_ && eased_ == target_ && shown_ != target_;
  if (arrived || fabsf(shortestTurn(shown_, eased_)) >= TILT_REDRAW_STEP_DEG) {
    shown_ = eased_;
    return true;
  }
  return false;
}
