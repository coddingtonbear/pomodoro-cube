#include "bthome.h"

#include <string.h>

namespace {

// --- Advertising data structure types, from the Bluetooth assigned numbers ---
constexpr uint8_t AD_TYPE_FLAGS = 0x01;
constexpr uint8_t AD_TYPE_SERVICE_DATA_16BIT = 0x16;

// LE General Discoverable, BR/EDR not supported.
constexpr uint8_t FLAGS_LE_ONLY_DISCOVERABLE = 0x06;

// BTHome's 16-bit service UUID, sent little-endian like everything else.
constexpr uint16_t BTHOME_UUID = 0xFCD2;

// The device information byte: version 2 in bits 5-7, and bit 2 marking the
// device as trigger based. A cube that sleeps most of the time advertises
// irregularly, and the flag is what stops Home Assistant treating the silence
// as a fault.
constexpr uint8_t DEVICE_INFO = (2 << 5) | 0x04;

// --- Object ids, in the ascending order the spec requires them to be sent ---
constexpr uint8_t OBJ_PACKET_ID = 0x00;
constexpr uint8_t OBJ_VOLTAGE = 0x0C;        // uint16, x0.001 V
constexpr uint8_t OBJ_WORK = 0x0F;           // binary (generic boolean)
constexpr uint8_t OBJ_CONNECTIVITY = 0x19;   // binary
constexpr uint8_t OBJ_RUNNING = 0x27;        // binary
constexpr uint8_t OBJ_COUNT = 0x3D;          // uint16
constexpr uint8_t OBJ_DURATION = 0x42;       // uint24, x0.001 s

constexpr uint32_t UINT24_MAX = 0xFFFFFF;

// Appends to a fixed buffer, and remembers if it ever ran out of room so the
// caller can check once at the end instead of after every write.
class Writer {
 public:
  Writer(uint8_t *out, size_t capacity) : out_(out), capacity_(capacity) {}

  void u8(uint8_t value) {
    if (length_ >= capacity_) {
      overflowed_ = true;
      return;
    }
    out_[length_++] = value;
  }

  void u16(uint16_t value) {
    u8((uint8_t)(value & 0xFF));
    u8((uint8_t)(value >> 8));
  }

  void u24(uint32_t value) {
    u8((uint8_t)(value & 0xFF));
    u8((uint8_t)((value >> 8) & 0xFF));
    u8((uint8_t)((value >> 16) & 0xFF));
  }

  size_t length() const { return length_; }
  bool overflowed() const { return overflowed_; }

 private:
  uint8_t *out_;
  size_t capacity_;
  size_t length_ = 0;
  bool overflowed_ = false;
};

uint16_t millivolts(float volts) {
  if (volts <= 0.0f) return 0;
  const float scaled = volts * 1000.0f + 0.5f;
  if (scaled >= 65535.0f) return 65535;
  return (uint16_t)scaled;
}

// Durations are sent in milliseconds, which a uint24 runs out of after about
// four and a half hours -- far beyond the longest face, but clamp rather than
// wrap, since a wrapped duration would read as a plausible short timer.
uint32_t durationMs(int seconds) {
  if (seconds <= 0) return 0;
  const uint32_t ms = (uint32_t)seconds * 1000u;
  return ms > UINT24_MAX ? UINT24_MAX : ms;
}

}  // namespace

size_t BTHome::encode(const State &state, uint8_t *out, size_t capacity) {
  if (out == nullptr) return 0;

  // Built in a scratch buffer so a payload that does not fit leaves the
  // caller's buffer alone rather than half-written.
  uint8_t scratch[MAX_ADVERTISEMENT];
  Writer writer(scratch, sizeof(scratch));

  writer.u8(2);
  writer.u8(AD_TYPE_FLAGS);
  writer.u8(FLAGS_LE_ONLY_DISCOVERABLE);

  // Length is filled in once the service data is complete.
  const size_t lengthIndex = writer.length();
  writer.u8(0);
  writer.u8(AD_TYPE_SERVICE_DATA_16BIT);
  writer.u16(BTHOME_UUID);
  writer.u8(DEVICE_INFO);

  writer.u8(OBJ_PACKET_ID);
  writer.u8(state.packetId);

  writer.u8(OBJ_VOLTAGE);
  writer.u16(millivolts(state.batteryVolts));

  // A generic boolean, because BTHome has no object that means "this interval is
  // work". It is what separates the two work faces from the two break ones, now
  // that neither flow face has a length to classify it by.
  writer.u8(OBJ_WORK);
  writer.u8(state.work ? 1 : 0);

  writer.u8(OBJ_CONNECTIVITY);
  writer.u8(state.awake ? 1 : 0);

  writer.u8(OBJ_RUNNING);
  writer.u8(state.running ? 1 : 0);

  writer.u8(OBJ_COUNT);
  writer.u16(state.pomodoroCount);

  writer.u8(OBJ_DURATION);
  writer.u24(durationMs(state.remainingSeconds));

  writer.u8(OBJ_DURATION);
  writer.u24(durationMs(state.selectedSeconds));

  if (writer.overflowed()) return 0;

  const size_t length = writer.length();
  if (length > capacity) return 0;

  // Everything after the length byte itself.
  scratch[lengthIndex] = (uint8_t)(length - lengthIndex - 1);

  for (size_t i = 0; i < length; i++) out[i] = scratch[i];
  return length;
}


size_t BTHome::Sequencer::update(const State &state, uint8_t *out, size_t capacity) {
  if (out == nullptr) return 0;

  // Encoded under the id the last advertisement carried, so the comparison
  // below is between everything *except* the id. Two payloads that differ only
  // there say the same thing, and a receiver has nothing new to hear.
  State candidate = state;
  candidate.packetId = published_ ? last_.packetId : 0;

  uint8_t probe[MAX_ADVERTISEMENT];
  const size_t length = encode(candidate, probe, sizeof(probe));
  if (length == 0) return 0;

  if (published_ && length == lastLength_ && memcmp(probe, lastPayload_, length) == 0) {
    return 0;
  }

  // Only now does the id move. Wrapping past 255 is what the spec expects of a
  // one-byte counter, and a receiver deduping on it sees a change either way.
  if (published_) candidate.packetId = (uint8_t)(candidate.packetId + 1);

  const size_t published = encode(candidate, out, capacity);
  if (published == 0) return 0;

  last_ = candidate;
  lastLength_ = published;
  memcpy(lastPayload_, out, published);
  published_ = true;
  return published;
}

size_t BTHome::Sequencer::farewell(uint8_t *out, size_t capacity) {
  if (!published_) return 0;

  State state = last_;
  state.awake = false;
  state.running = false;
  // Back through update(), so the farewell is numbered like any other change --
  // and so a cube that has already said this much sends nothing, which is the
  // right answer rather than a special case.
  return update(state, out, capacity);
}
