// The stand-ins for the cube's physical inputs. The SDL layer writes these
// from keypresses; the IMU and battery stubs read them.
#pragma once

#include "consts.h"

namespace SimInput {

// What face the cube is currently resting on.
extern Orientation orientation;

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

// Set by tone()/noTone() so the renderer can show when the beeper is on.
extern bool beeperActive;
extern unsigned int beeperFrequency;

}  // namespace SimInput
