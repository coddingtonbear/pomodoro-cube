#include "ble.h"

#include <Arduino.h>
#include <NimBLEDevice.h>

#include <string.h>
#include <string>

namespace {

// NimBLE's advertising intervals are in units of 0.625 ms.
constexpr uint16_t intervalUnits(uint32_t ms) {
  return (uint16_t)(ms * 1000UL / 625UL);
}

// Every advertisement goes out in a burst at the fastest interval a legacy
// non-connectable advertisement is allowed, and the radio is quiet between
// bursts -- see BTHome::Scheduler for when one is due. BURST_MS at this
// interval is about twenty copies, which a receiver scanning at a low duty
// cycle (an ESPHome Bluetooth proxy listens for 30 ms in every 320 by default)
// should catch at least one of, and the next heartbeat covers one it does not.
//
// The farewell goes out the same way, but stays on the air through the whole
// of the shutdown sequence rather than for a burst of its own: it is the one
// advertisement that cannot be repeated later.
constexpr uint32_t ADVERT_INTERVAL_MS = 100;
constexpr unsigned long BURST_MS = 2000;

BTHome::Sequencer sequencer;
BTHome::Scheduler scheduler;
// setup() has been called: the cube is meant to be on the air.
bool ready = false;
// The controller and the NimBLE host are up. Brought up with the first burst
// and left up until shutdown(): only advertising starts and stops with each
// burst. Tearing the stack down after every burst crashed about one time in
// thirty -- NimBLE-Arduino 1.4.3's deinit() races its own host task as that
// task exits, and the board panicked in NimBLEDevice::host_task
// (InstrFetchProhibited) roughly two seconds into a burst, as it ended.
bool radioOn = false;
// The advertisement last put on the air, which a heartbeat with nothing new in
// it sends again. Under the same packet id, so a receiver deduping on it
// hears the cube is still there without taking it for a new reading.
uint8_t lastPayload[BTHome::MAX_ADVERTISEMENT];
size_t lastLength = 0;
// millis() when the burst on the air started; meaningful while `bursting`.
unsigned long burstStarted = 0;
bool bursting = false;
// millis() when the farewell went on the air; meaningful while `farewelling`.
unsigned long farewellStarted = 0;
bool farewelling = false;

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

static void radioUp() {
  if (radioOn) return;
  const unsigned long started = millis();

  // No name: the payload already uses 30 of the 31 bytes a legacy
  // advertisement holds, so there is nowhere to put one. Home Assistant names
  // the device after its MAC instead, which is renameable there.
  NimBLEDevice::init("");

  // Set again on every bring-up rather than trusted to survive the last
  // deinit, which keeps the advertising object but is not promised to keep
  // what was set on it.
  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  // A beacon, not a peripheral. Without the scan response this also drops the
  // advertisement to ADV_NONCONN_IND, so the radio never listens for scan
  // requests and nothing can try to connect.
  advertising->setAdvertisementType(BLE_GAP_CONN_MODE_NON);
  advertising->setScanResponse(false);
  advertising->setMinInterval(intervalUnits(ADVERT_INTERVAL_MS));
  advertising->setMaxInterval(intervalUnits(ADVERT_INTERVAL_MS));

  radioOn = true;
  Serial.printf("[trace] BLE t=%lu up ms=%lu heap=%u largest=%u\n", millis(), millis() - started,
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
}

static void radioDown() {
  if (!radioOn) return;
  NimBLEDevice::deinit(false);
  radioOn = false;
}

bool BLE::onAir() {
  return bursting || farewelling;
}

void BLE::setup() {
  ready = true;
}

void BLE::publish(const BTHome::State &state) {
  if (!ready) return;

  if (bursting && millis() - burstStarted >= BURST_MS) {
    NimBLEDevice::getAdvertising()->stop();
    bursting = false;
  }

  const BTHome::Burst burst = scheduler.due(state, millis());
  if (burst == BTHome::Burst::None) return;

  uint8_t payload[BTHome::MAX_ADVERTISEMENT];
  const size_t length = sequencer.update(state, payload, sizeof(payload));
  if (length > 0) {
    memcpy(lastPayload, payload, length);
    lastLength = length;
  }
  if (lastLength == 0) return;

  Serial.printf("[trace] BLE t=%lu burst=%s new=%d\n", millis(),
                burst == BTHome::Burst::Change ? "change" : "heartbeat", length > 0 ? 1 : 0);
  radioUp();
  broadcast(lastPayload, lastLength);
  burstStarted = millis();
  bursting = true;
}

void BLE::farewell() {
  if (!ready) return;

  uint8_t payload[BTHome::MAX_ADVERTISEMENT];
  const size_t length = sequencer.farewell(payload, sizeof(payload));
  if (length == 0) return;  // already said, or nothing to say it from

  radioUp();
  broadcast(payload, length);
  farewellStarted = millis();
  farewelling = true;
  // On the air until shutdown(), not for a burst: nothing is to stop it.
  bursting = false;
}

void BLE::shutdown() {
  if (!ready) return;

  if (farewelling) {
    const unsigned long onAir = millis() - farewellStarted;
    if (onAir < FAREWELL_MIN_AIRTIME_MS) delay(FAREWELL_MIN_AIRTIME_MS - onAir);
    farewelling = false;
  }

  radioDown();
  ready = false;
}
