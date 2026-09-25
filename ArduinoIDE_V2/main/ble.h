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

// Puts the advertisement that goes out as the cube falls asleep on the air,
// with connectivity and running both dropped -- see
// BTHome::Sequencer::farewell() -- at the fastest interval a legacy
// advertisement allows, and returns at once. The radio keeps repeating it
// through whatever shutdown work follows, until shutdown() is called; the
// farewell cannot be repeated later, so every copy that gets out is one the
// receiver might not otherwise have heard.
//
// Safe to call when BLE was never brought up, which is the path a cube takes
// when it wakes on a resting face and goes straight back to sleep.
void farewell();

// Shuts the controller down. Call it last, immediately before deep sleep: it
// first makes sure a farewell put on the air by farewell() has had at least
// FAREWELL_MIN_AIRTIME_MS to repeat, blocking for the remainder if the work in
// between was quicker than that, so the guarantee does not depend on how long
// the panel and beeper happen to take. Deep sleep cuts the radio's power
// anyway; shutting it down in order means the controller is not
// mid-transmission when that happens.
//
// Safe to call when BLE was never brought up.
void shutdown();

// The least time a farewell is on the air before shutdown() lets the radio go.
// The shutdown work it overlaps -- putting the panel away, the second's pause,
// the shutdown beeps -- takes longer than this in practice; the figure is a
// floor, not the expectation.
constexpr unsigned long FAREWELL_MIN_AIRTIME_MS = 1000;

}  // namespace BLE
