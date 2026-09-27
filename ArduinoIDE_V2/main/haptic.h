#pragma once
#include "haptic_pattern.h"

// The vibration motor: one pin, driven high to run it.
namespace Haptic {

// Takes the pin back from the hold deep sleep left on it, with the motor off.
void setup();

// Start a pattern and return at once; cycle() is what plays it.
void play(Pattern pattern);

// Play a pattern through before returning. For the deep-sleep path, which has
// no loop() left to play one from. A repeating pattern is refused, since it
// would never return.
void playBlocking(Pattern pattern);

void stop();

// Called on every pass of loop() to move the pin through whatever is playing.
void cycle();

Pattern playing();

// True while the motor is shaking the accelerometer, or has only just stopped.
bool disturbing();

// Motor off and the pin held there through deep sleep, where a floating gate
// would be free to run it.
void holdForSleep();

}  // namespace Haptic
