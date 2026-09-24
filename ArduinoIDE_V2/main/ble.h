#pragma once
#include "bthome.h"

// The radio half of the BTHome broadcast: takes the payload bthome.cpp builds
// and puts it on the air over NimBLE.
//
// Advertising only -- there is no GATT server, no scanning and nothing to
// connect to. A BTHome device is a beacon, and a non-connectable advertisement
// is about a millisecond of radio every few hundred, which is nothing beside
// the backlight.
namespace BLE {

// Brings up the controller and starts advertising. Blocks for a few hundred
// milliseconds while the NimBLE host syncs, so call it once the panel is
// already up and there is something to look at.
void setup();

// Offer the current state. Cheap enough to call on every pass of the loop: the
// payload only reaches the radio when the bytes a receiver would see have
// changed. Does nothing before setup().
void publish(const BTHome::State &state);

// The advertisement that goes out as the cube falls asleep, with connectivity
// and running both dropped -- see BTHome::Sequencer::farewell(). Blocks long
// enough for several copies to go out, because the radio is about to lose
// power, and then shuts the controller down.
//
// Safe to call when BLE was never brought up, which is the path a cube takes
// when it wakes on a resting face and goes straight back to sleep.
void farewell();

}  // namespace BLE
