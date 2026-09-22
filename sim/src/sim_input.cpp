#include "sim_input.h"

namespace SimInput {

// Start on a face that runs a timer, so the sim doesn't immediately deep-sleep
// the way setup() would if it booted flat on a desk.
Orientation orientation = Orientation::DEG_90;
float batteryVoltage = 3.9f;
BatteryOverride batteryOverride = BatteryOverride::None;
bool beeperActive = false;
unsigned int beeperFrequency = 0;

}  // namespace SimInput
