#pragma once
#include <stdint.h>

#include "consts.h"

// Which way up the face is drawn. Separate from which face the cube is on:
// that is a decision, debounced and held, and it chooses the timer. This is
// only ever a drawing angle, followed continuously, so that turning the cube
// turns the picture with it rather than swapping it a quarter turn at a time.
//
// Nothing in here touches the panel or the sensor, so all of it runs in the
// host tests.
namespace Tilt {

// The angle a timer face is drawn at, in degrees clockwise from the default
// orientation. 0 for anything that is not a timer face.
float faceAngle(Orientation ori);

// The signed short way round from one angle to another, in (-180, 180].
float shortestTurn(float from, float to);

// The angle the in-plane part of a reading points the face at, on the same
// scale as faceAngle(). False when too little of gravity lies in the plane of
// the screen to say -- a cube on its back is not at any angle.
bool inPlaneAngle(float ax, float ay, float &degrees);

// An angle as the panel is asked to draw it: the quarter turn the GC9A01 does
// for nothing, and what is left over for LVGL to do in software. `lean` is
// whole degrees in [-45, 45], and 0 whenever the angle is a quarter turn --
// which is what keeps a cube at rest drawing exactly as cheaply as it did
// before any of this.
struct Split {
  uint8_t quarter;
  int lean;
};
Split split(float degrees);

class Tracker {
public:
  // Feed one pass of the loop in; true when the angle to draw has moved.
  // Called every pass whether or not there is a reading worth having, because
  // the ease towards the target runs on the clock rather than on the samples.
  // `trusted` is false for a reading that is missing or is not gravity.
  bool update(bool trusted, float ax, float ay, unsigned long nowMs);

  // False until a reading has pointed the face somewhere.
  bool hasAngle() const { return hasAngle_; }

  // The angle to draw, in [0, 360).
  float angle() const { return shown_; }

  // Start from a known angle rather than from a reading.
  void seed(float degrees);

private:
  void retarget(float reading);

  // The in-plane reading, smoothed.
  float senseX_ = 0.0f;
  float senseY_ = 0.0f;
  bool sensed_ = false;

  float target_ = 0.0f;
  float eased_ = 0.0f;
  float shown_ = 0.0f;
  bool snapped_ = false;
  bool hasAngle_ = false;

  unsigned long lastMs_ = 0;
  bool started_ = false;
};

}  // namespace Tilt
