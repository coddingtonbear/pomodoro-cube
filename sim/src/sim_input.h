// The stand-ins for the cube's physical inputs. The SDL layer writes these
// from keypresses; the IMU and battery stubs read them.
#pragma once

#include "consts.h"

namespace SimInput {

// What face the cube is currently resting on.
extern Orientation orientation;

// How far the cube is turned off square on that face, in degrees clockwise.
// Only means anything on a timer face.
extern float lean;

// The angle the cube is held at in the plane of the screen: the face's own
// angle plus the lean. On a resting face, where there is no such angle, the one
// it had when it was last on a timer face.
float attitudeDegrees();

// Simulated pack voltage, in volts, before the firmware's smoothing filter.
extern float batteryVoltage;

// Drives the low-battery warning directly, skipping the voltage model. Useful
// because the firmware smooths voltage over ten samples taken five seconds
// apart, so nudging the voltage takes the best part of a minute to show up --
// and pushing it low enough always ends in a deep sleep instead.
enum class BatteryOverride {
  None,      // warning follows the simulated voltage
  Warning,   // warning forced on
  Healthy,   // warning forced off
};
extern BatteryOverride batteryOverride;

// Set by the `t` key and cleared by the first QMI::takeTap() that sees it,
// standing in for the QMI8658's latched tap event.
extern bool tapPending;

// Follows the level on HAPTIC_PIN so the renderer can show when the vibration
// motor is running.
extern bool motorActive;

}  // namespace SimInput
