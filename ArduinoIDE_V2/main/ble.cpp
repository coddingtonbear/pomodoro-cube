#include "ble.h"

#include <Arduino.h>
#include <NimBLEDevice.h>

#include <string>

namespace {

// NimBLE's advertising intervals are in units of 0.625 ms.
constexpr uint16_t intervalUnits(uint32_t ms) {
  return (uint16_t)(ms * 1000UL / 625UL);
}

// While awake. Short enough that turning the cube over shows up in Home
// Assistant while the hand is still moving, and long enough to be irrelevant
// next to the panel: three adverts a second is well under a tenth of a
// milliamp, against twenty-odd for the backlight at its dimmest.
constexpr uint32_t AWAKE_INTERVAL_MS = 300;

// The farewell is the one advertisement that cannot be repeated later, so it
// goes out at the fastest interval a legacy non-connectable advertisement is
// allowed, several times over, before the radio loses power.
constexpr uint32_t FAREWELL_INTERVAL_MS = 100;
constexpr uint32_t FAREWELL_AIRTIME_MS = 400;

BTHome::Sequencer sequencer;
bool ready = false;

// Hands a finished payload to the controller. The advertisement data is set
// whole rather than assembled from NimBLE's field setters, because the BTHome
// service data and its leading Flags structure are already exactly the bytes
// that have to go out -- see bthome.cpp.
void broadcast(const uint8_t *payload, size_t length) {
  NimBLEAdvertisementData data;
  data.addData(std::string((const char *)payload, length));

  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->setAdvertisementData(data);
  // Updating the data while advertising is already running is allowed and is
  // the normal case; start() is a no-op then.
  advertising->start();
}

}  // namespace

void BLE::setup() {
  if (ready) return;

  // No name: the payload already uses 30 of the 31 bytes a legacy
  // advertisement holds, so there is nowhere to put one. Home Assistant names
  // the device after its MAC instead, which is renameable there.
  NimBLEDevice::init("");

  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  // A beacon, not a peripheral. Without the scan response this also drops the
  // advertisement to ADV_NONCONN_IND, so the radio never listens for scan
  // requests and nothing can try to connect.
  advertising->setAdvertisementType(BLE_GAP_CONN_MODE_NON);
  advertising->setScanResponse(false);
  advertising->setMinInterval(intervalUnits(AWAKE_INTERVAL_MS));
  advertising->setMaxInterval(intervalUnits(AWAKE_INTERVAL_MS));

  ready = true;
}

void BLE::publish(const BTHome::State &state) {
  if (!ready) return;

  uint8_t payload[BTHome::MAX_ADVERTISEMENT];
  const size_t length = sequencer.update(state, payload, sizeof(payload));
  if (length == 0) return;  // nothing new to say

  broadcast(payload, length);
}

void BLE::farewell() {
  if (!ready) return;

  uint8_t payload[BTHome::MAX_ADVERTISEMENT];
  const size_t length = sequencer.farewell(payload, sizeof(payload));

  if (length > 0) {
    NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
    // Stopped first because the interval is a start-time parameter: set while
    // advertising, it would not take effect until the next start, which for
    // this advertisement never comes.
    advertising->stop();
    advertising->setMinInterval(intervalUnits(FAREWELL_INTERVAL_MS));
    advertising->setMaxInterval(intervalUnits(FAREWELL_INTERVAL_MS));

    broadcast(payload, length);
    delay(FAREWELL_AIRTIME_MS);
  }

  // Deep sleep cuts the radio's power anyway; shutting it down in order means
  // the controller is not mid-transmission when that happens.
  NimBLEDevice::deinit(false);
  ready = false;
}
