// Stands in for qmi.cpp: instead of reading a QMI8658 over I2C, it synthesises
// the accelerometer vector for whichever face the sim says the cube is on.
// Vectors are chosen so that Util::calcOrientation() maps them back to exactly
// that orientation.
#include "qmi.h"

#include "sim_input.h"

void QMI::setup() {}
void QMI::setupWakeup() {}

bool QMI::getAccelerometer(float &ax, float &ay, float &az) {
  ax = 0.0f;
  ay = 0.0f;
  az = 0.0f;

  switch (SimInput::orientation) {
    case Orientation::FACE_UP:   az = -1.0f; break;
    case Orientation::FACE_DOWN: az = 1.0f;  break;
    case Orientation::DEG_0:   ax = -1.0f; break;
    case Orientation::DEG_90:  ay = -1.0f; break;
    case Orientation::DEG_180: ax = 1.0f;  break;
    case Orientation::DEG_270: ay = 1.0f;  break;
    case Orientation::UNDEFINED: return false;
  }
  return true;
}
