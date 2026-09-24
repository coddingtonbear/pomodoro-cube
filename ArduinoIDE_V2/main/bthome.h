#pragma once
#include <stddef.h>
#include <stdint.h>

// Encodes the cube's state as a BTHome v2 advertisement, which Home Assistant
// discovers natively: https://bthome.io/format/
//
// Nothing here touches a radio. Assembling the bytes is the part that is easy
// to get wrong and easy to test, so it lives on its own; handing the result to
// NimBLE is the device's job.
namespace BTHome {

// Everything the advertisement carries, in the units the firmware already
// works in. The encoder applies BTHome's scaling factors.
struct State {
  uint8_t packetId;         // increments on change, so receivers can dedupe
  float batteryVolts;
  bool awake;               // false only in the advert sent before deep sleep
  bool running;             // is the timer advancing
  bool work;                // work interval rather than a break
  uint16_t pomodoroCount;
  int remainingSeconds;
  // The interval being counted down. Zero alongside `work` means the timer is
  // counting up instead, and remainingSeconds is the elapsed time; zero on a
  // break means a break of no length, which an empty flow bank produces. The
  // work flag is what separates the two -- see the README's Bluetooth section.
  int selectedSeconds;
};

// A legacy advertisement carries at most 31 bytes.
constexpr size_t MAX_ADVERTISEMENT = 31;

// Writes the whole advertising payload -- the Flags structure followed by the
// BTHome service data -- into `out`. Returns the number of bytes written, or 0
// if it would not fit, in which case `out` is untouched.
size_t encode(const State &state, uint8_t *out, size_t capacity);

}  // namespace BTHome
