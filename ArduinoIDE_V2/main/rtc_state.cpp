#include <Arduino.h>

#include <string.h>

#include "rtc_state.h"

// RTC_DATA_ATTR places this in the RTC slow-memory segment, which stays powered
// through deep sleep. The simulator's shim defines the attribute away and backs
// the block with a file instead.
RTC_DATA_ATTR RtcState::Data g_rtcData;

bool RtcState::isInitialised(const Data &data) {
  return data.magic == MAGIC;
}

void RtcState::initialise(Data &data) {
  // Clear the whole block, padding included, so an initialised block is
  // byte-identical whatever junk preceded it. Nothing reads the padding today,
  // but a checksum or a memcmp later would.
  memset(&data, 0, sizeof(Data));

  data.magic = MAGIC;
  data.pomodoroCount = 0;
  data.pauseValid = false;
  data.pausedFace = Orientation::UNDEFINED;
  data.pausedRemaining = 0;
  data.pausedSelected = 0;
}

RtcState::Data &RtcState::data() {
  return g_rtcData;
}

void RtcState::begin() {
  if (!isInitialised(g_rtcData)) initialise(g_rtcData);
}
