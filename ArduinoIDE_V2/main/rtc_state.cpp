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
  data.pausedCountingUp = false;
  data.flowBankSeconds = 0;
  data.panelHoldingFrame = false;
}

RtcState::Data &RtcState::data() {
  return g_rtcData;
}

void RtcState::begin() {
  if (!isInitialised(g_rtcData)) initialise(g_rtcData);
}

void RtcState::storePause(Data &data, Orientation face, int remaining, int selected,
                          bool countingUp) {
  // A timer at zero has finished rather than paused, and one that never started
  // has nothing to hold. A count-up stint carries no selected length, so for one
  // of those the elapsed time is the whole test.
  if (remaining <= 0) return;
  if (!countingUp && selected <= 0) return;

  data.pauseValid = true;
  data.pausedFace = face;
  data.pausedRemaining = remaining;
  data.pausedSelected = selected;
  data.pausedCountingUp = countingUp;
}

void RtcState::clearPause(Data &data) {
  data.pauseValid = false;
  data.pausedFace = Orientation::UNDEFINED;
  data.pausedRemaining = 0;
  data.pausedSelected = 0;
  data.pausedCountingUp = false;
}

bool RtcState::hasPause(const Data &data) {
  return data.pauseValid;
}

bool RtcState::takePause(Data &data, Orientation face, int &remaining, int &selected,
                         bool &countingUp) {
  const bool resumable = data.pauseValid && data.pausedFace == face;
  if (resumable) {
    remaining = (int)data.pausedRemaining;
    selected = (int)data.pausedSelected;
    countingUp = data.pausedCountingUp;
  }

  clearPause(data);
  return resumable;
}

void RtcState::addFlowBank(Data &data, int seconds) {
  if (seconds <= 0) return;
  setFlowBank(data, (int)data.flowBankSeconds + seconds);
}

int RtcState::flowBank(const Data &data) {
  return (int)data.flowBankSeconds;
}

void RtcState::setFlowBank(Data &data, int seconds) {
  if (seconds < 0) seconds = 0;
  if (seconds > FLOW_MAX_SECONDS) seconds = FLOW_MAX_SECONDS;
  data.flowBankSeconds = seconds;
}

void RtcState::clearFlowBank(Data &data) {
  data.flowBankSeconds = 0;
}
