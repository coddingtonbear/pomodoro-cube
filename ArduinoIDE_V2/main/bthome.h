#pragma once
#include <stddef.h>
#include <stdint.h>

// Encodes the cube's state as a BTHome v2 advertisement, which Home Assistant
// discovers natively: https://bthome.io/format/
//
// Nothing here touches a radio. Assembling the bytes is the part that is easy
// to get wrong and easy to test, so it lives on its own; handing the result to
// NimBLE is ble.cpp's job.
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
//
// `state.packetId` is written out as given. Callers that want the id to mean
// "something changed" should go through a Sequencer rather than counting
// themselves.
size_t encode(const State &state, uint8_t *out, size_t capacity);

// Turns a stream of states into a stream of advertisements, which is a narrower
// job than it sounds: it decides when there is anything new to say, and numbers
// the packets accordingly.
//
// Holding that here rather than at the call site is what lets the firmware
// offer its state on every pass of the loop and still only reach for the radio
// when the bytes a receiver would see have actually changed. It also makes the
// packet id honest by construction: BTHome's id exists so a receiver can drop
// duplicates, which only works if it moves when the reading does and not when
// the clock does.
class Sequencer {
 public:
  // Encodes `state` into `out` if it differs from the last advertisement, under
  // the next packet id. Returns the number of bytes written, or 0 when there is
  // nothing new to send -- or when the payload would not fit -- leaving `out`
  // untouched either way.
  size_t update(const State &state, uint8_t *out, size_t capacity);

  // The advertisement that goes out as the cube falls asleep: the last state
  // again, with `awake` and `running` both cleared.
  //
  // `running` goes too because a sleeping cube is not counting anything. It is
  // also the only way a pause can be described at all: the README's state table
  // reads a pause as a timer stopped part way through, which needs `running`
  // false, and the cube is always asleep while paused -- so this advert is the
  // one that has to say it.
  //
  // Returns 0 if nothing has been published yet, there being no state to say
  // farewell from.
  size_t farewell(uint8_t *out, size_t capacity);

 private:
  // As published, packet id included. Meaningless until `published_`.
  State last_;
  // The bytes that went out, so "has anything changed" is answered in terms of
  // what a receiver would actually see rather than the fields behind it. It
  // means a pack voltage wobbling below a millivolt reads as no change, which
  // comparing the floats would not.
  uint8_t lastPayload_[MAX_ADVERTISEMENT];
  size_t lastLength_ = 0;
  bool published_ = false;
};

}  // namespace BTHome
