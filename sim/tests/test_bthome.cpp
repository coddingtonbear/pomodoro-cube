// Tests for the BTHome v2 encoder. These assert exact bytes rather than round
// tripping through the encoder's own logic: the whole point is that a receiver
// that has never seen this code can read the result.
#include "bthome.h"

#include <cstring>

#include "check.h"

namespace {

BTHome::State workTimerRunning() {
  BTHome::State state;
  state.packetId = 7;
  state.batteryVolts = 3.82f;
  state.awake = true;
  state.running = true;
  state.pomodoroCount = 3;
  state.remainingSeconds = 1234;
  state.selectedSeconds = 1500;
  return state;
}

// Flags is 3 bytes; then the service data's length, type, 2 UUID bytes and the
// device info byte. Objects start after that.
constexpr size_t OBJECTS_START = 3 + 1 + 1 + 2 + 1;

size_t valueSizeOf(uint8_t id) {
  switch (id) {
    case 0x00: case 0x19: case 0x27: return 1;  // uint8 and binaries
    case 0x0C: case 0x3D: return 2;             // uint16
    case 0x42: return 3;                        // uint24
    default: return 0;                          // unknown
  }
}

// Walks the object section, so ordering can be checked without hardcoding it.
size_t objectIdsOf(const uint8_t *advert, size_t length, uint8_t *ids, size_t max) {
  size_t offset = OBJECTS_START;
  size_t count = 0;

  while (offset < length && count < max) {
    const uint8_t id = advert[offset];
    const size_t size = valueSizeOf(id);
    if (size == 0) return count;

    ids[count++] = id;
    offset += 1 + size;
  }
  return count;
}

// The value bytes of the nth object with this id, or nullptr. Scanning beats
// hardcoded offsets: the first draft of these tests counted by hand and got
// every offset one byte low.
const uint8_t *findObject(const uint8_t *advert, size_t length, uint8_t wanted,
                          size_t occurrence = 0) {
  size_t offset = OBJECTS_START;
  size_t seen = 0;

  while (offset < length) {
    const uint8_t id = advert[offset];
    const size_t size = valueSizeOf(id);
    if (size == 0) return nullptr;

    if (id == wanted && seen++ == occurrence) return advert + offset + 1;
    offset += 1 + size;
  }
  return nullptr;
}

}  // namespace

void testEncodesKnownStateExactly() {
  uint8_t advert[BTHome::MAX_ADVERTISEMENT];
  const size_t length = BTHome::encode(workTimerRunning(), advert, sizeof(advert));

  const uint8_t expected[] = {
      0x02, 0x01, 0x06,              // Flags: LE general discoverable, BR/EDR not supported
      0x18, 0x16, 0xD2, 0xFC, 0x44,  // 24 bytes of service data, UUID 0xFCD2, BTHome v2 trigger based
      0x00, 0x07,                    // packet id 7
      0x0C, 0xEC, 0x0E,              // voltage 3820 mV
      0x19, 0x01,                    // connectivity: awake
      0x27, 0x01,                    // running
      0x3D, 0x03, 0x00,              // 3 pomodoros
      0x42, 0x50, 0xD4, 0x12,        // remaining 1_234_000 ms
      0x42, 0x60, 0xE3, 0x16,        // started at 1_500_000 ms
  };

  CHECK(length == sizeof(expected));
  CHECK(std::memcmp(advert, expected, sizeof(expected)) == 0);
}

void testFitsInALegacyAdvertisement() {
  BTHome::State state = workTimerRunning();
  state.packetId = 255;
  state.pomodoroCount = 65535;
  state.batteryVolts = 4.2f;
  state.remainingSeconds = 50 * 60;
  state.selectedSeconds = 50 * 60;

  uint8_t advert[BTHome::MAX_ADVERTISEMENT];
  const size_t length = BTHome::encode(state, advert, sizeof(advert));

  CHECK(length > 0);
  CHECK(length <= BTHome::MAX_ADVERTISEMENT);
}

void testObjectIdsAscend() {
  // The spec requires ascending object ids, and a receiver may reject a packet
  // that breaks it. Repeated ids are allowed, so this is non-decreasing.
  uint8_t advert[BTHome::MAX_ADVERTISEMENT];
  const size_t length = BTHome::encode(workTimerRunning(), advert, sizeof(advert));

  uint8_t ids[16];
  const size_t count = objectIdsOf(advert, length, ids, sizeof(ids));

  CHECK(count == 7);
  for (size_t i = 1; i < count; i++) {
    CHECK_MSG(ids[i] >= ids[i - 1], "object ids must not go backwards");
  }
}

void testFarewellAdvert() {
  // What goes out just before deep sleep: not awake, not running, but still
  // carrying the timer so HA can tell paused from idle.
  BTHome::State state = workTimerRunning();
  state.awake = false;
  state.running = false;

  uint8_t advert[BTHome::MAX_ADVERTISEMENT];
  const size_t length = BTHome::encode(state, advert, sizeof(advert));
  CHECK(length > 0);

  const uint8_t *connectivity = findObject(advert, length, 0x19);
  const uint8_t *running = findObject(advert, length, 0x27);
  CHECK(connectivity != nullptr && connectivity[0] == 0x00);
  CHECK(running != nullptr && running[0] == 0x00);

  // The timer is still carried, so HA can tell a paused cube from an idle one.
  const uint8_t *remaining = findObject(advert, length, 0x42, 0);
  CHECK(remaining != nullptr);
  CHECK(remaining[0] != 0 || remaining[1] != 0 || remaining[2] != 0);
}

void testOutOfRangeValuesClamp() {
  BTHome::State state = workTimerRunning();
  state.remainingSeconds = -5;
  state.selectedSeconds = 100000;  // beyond what a uint24 of milliseconds holds
  state.batteryVolts = -1.0f;

  uint8_t advert[BTHome::MAX_ADVERTISEMENT];
  const size_t length = BTHome::encode(state, advert, sizeof(advert));

  // Still a well-formed advertisement of the usual size: a clamped value must
  // not wrap into something that reads as a plausible short timer.
  CHECK(length == 28);

  const uint8_t *voltage = findObject(advert, length, 0x0C);
  CHECK(voltage != nullptr && voltage[0] == 0x00 && voltage[1] == 0x00);

  const uint8_t *remaining = findObject(advert, length, 0x42, 0);
  CHECK(remaining != nullptr);
  CHECK(remaining[0] == 0x00 && remaining[1] == 0x00 && remaining[2] == 0x00);

  const uint8_t *selected = findObject(advert, length, 0x42, 1);
  CHECK(selected != nullptr);
  CHECK(selected[0] == 0xFF && selected[1] == 0xFF && selected[2] == 0xFF);
}

void testRefusesABufferItCannotFill() {
  uint8_t tooSmall[10];
  CHECK(BTHome::encode(workTimerRunning(), tooSmall, sizeof(tooSmall)) == 0);

  // And leaves it alone rather than half-writing it.
  uint8_t untouched[10];
  std::memset(untouched, 0xAA, sizeof(untouched));
  uint8_t probe[10];
  std::memset(probe, 0xAA, sizeof(probe));
  BTHome::encode(workTimerRunning(), probe, sizeof(probe));
  CHECK(std::memcmp(probe, untouched, sizeof(probe)) == 0);
}
