#include "sim_input.h"

#include "tilt.h"
#include "util.h"

namespace SimInput {

// Start on a face that runs a timer, so the sim doesn't immediately deep-sleep
// the way setup() would if it booted flat on a desk.
Orientation orientation = Orientation::DEG_90;
float lean = 0.0f;
float batteryVoltage = 3.9f;
BatteryOverride batteryOverride = BatteryOverride::None;
QMI::Tap pendingTap = QMI::Tap::None;
bool motorActive = false;

float attitudeDegrees() {
  static float last = 0.0f;
  if (orientation != Orientation::UNDEFINED && !Util::isRestingFace(orientation)) {
    last = Tilt::faceAngle(orientation) + lean;
  }
  return last;
}

}  // namespace SimInput
