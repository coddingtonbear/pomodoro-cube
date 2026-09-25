// Stands in for ble.cpp: instead of handing the payload to NimBLE, it keeps the
// most recent one so the sim can print it. The BTHome::Sequencer behind it is
// the firmware's own, so what the sim prints -- including when it declines to
// send anything -- is what the board would do.
#include "ble.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "sim_ble.h"

uint8_t SimBLE::lastPayload[31] = {0};
size_t SimBLE::lastLength = 0;
unsigned long SimBLE::adverts = 0;
bool SimBLE::lastWasFarewell = false;

namespace {

BTHome::Sequencer sequencer;
bool ready = false;

// SIM_BLE_TRACE prints every advertisement as it goes out. `a` covers reading
// one by hand; this is for watching the sequence without a keyboard, which is
// the only way to check the timing of them from a script.
bool tracing() {
  static const bool on = std::getenv("SIM_BLE_TRACE") != nullptr;
  return on;
}

void dump(const char *what) {
  std::printf("[sim] %s %lu (%zu bytes):", what, SimBLE::adverts, SimBLE::lastLength);
  for (size_t i = 0; i < SimBLE::lastLength; i++) std::printf(" %02X", SimBLE::lastPayload[i]);
  std::printf("\n");
}

void record(size_t length, bool farewell) {
  if (length == 0) return;
  SimBLE::lastLength = length;
  SimBLE::adverts++;
  SimBLE::lastWasFarewell = farewell;
  if (tracing()) dump(farewell ? "farewell advertisement" : "advertisement");
}

}  // namespace

void BLE::setup() {
  ready = true;
}

void BLE::publish(const BTHome::State &state) {
  if (!ready) return;
  record(sequencer.update(state, SimBLE::lastPayload, sizeof(SimBLE::lastPayload)), false);
}

void BLE::farewell() {
  if (!ready) return;
  const size_t length = sequencer.farewell(SimBLE::lastPayload, sizeof(SimBLE::lastPayload));
  const bool traced = tracing();
  record(length, true);
  // Always printed, traced or not: the sim's deep sleep ends the process, so
  // this is the only chance to see the advertisement that mattered most.
  if (!traced && length > 0) dump("farewell advertisement");
}

void BLE::shutdown() {
  ready = false;
}
