#pragma once
#include <stddef.h>
#include <stdint.h>

// What the sim's stand-in for ble.cpp last put "on the air", so the sim can
// print the real firmware's advertisements rather than building its own.
namespace SimBLE {

extern uint8_t lastPayload[31];
extern size_t lastLength;
// How many advertisements have gone out since boot. With the payload only
// changing when the state does, this counts real changes rather than passes of
// the loop.
extern unsigned long adverts;
// True when the last one was the farewell sent on the way into deep sleep.
extern bool lastWasFarewell;

}  // namespace SimBLE
