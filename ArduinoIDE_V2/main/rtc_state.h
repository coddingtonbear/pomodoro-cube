#pragma once
#include <stdint.h>

#include "consts.h"

// State that has to outlive deep sleep but not a power cycle.
//
// The ESP32 keeps a small block of RAM powered through deep sleep and loses it
// when the pack is disconnected or runs flat. On that cold boot the block holds
// whatever was left in it -- arbitrary bits that can easily look like a
// plausible timer -- so nothing here can be trusted until the magic word says
// this firmware wrote it.
namespace RtcState {

// Bump the trailing digit whenever Data's layout changes, so a firmware update
// discards the old layout instead of misreading it.
constexpr uint32_t MAGIC = 0x504F4D32;  // "POM2"

struct Data {
  uint32_t magic;

  // Completed work timers since the last power cycle.
  uint16_t pomodoroCount;

  // A timer left paused, and the face it was paused from. Resuming only on
  // that same face is deliberate: choosing a different face is choosing a
  // different interval.
  bool pauseValid;
  Orientation pausedFace;
  int32_t pausedRemaining;
  int32_t pausedSelected;
  // A paused flow stint has to resume counting up rather than down, and carries
  // no selected length to tell the two apart by.
  bool pausedCountingUp;

  // A finished flow stint waiting for the break face to claim it. Separate from
  // the pause because the two are consumed by different faces: a pause belongs
  // to the face it was paused from, and this belongs to the break face.
  int32_t flowEarnedSeconds;
};

// True when the block was written by this firmware and survived intact.
bool isInitialised(const Data &data);

// Zero every field and stamp the magic word.
void initialise(Data &data);

// The block itself, in RTC memory on the device.
Data &data();

// Call once at startup: keeps the block if it survived, resets it if not.
void begin();

// Remember a timer to pick up again later. Does nothing when there is nothing
// worth resuming -- a finished or never-started timer is not a pause.
void storePause(Data &data, Orientation face, int remaining, int selected, bool countingUp);

void clearPause(Data &data);
bool hasPause(const Data &data);

// Hands back a stored pause, but only to the face it was paused from: setting
// the cube down on a different face is choosing a different interval, so the
// pause is abandoned. Either way the stored pause is consumed.
bool takePause(Data &data, Orientation face, int &remaining, int &selected, bool &countingUp);

// Bank a finished flow stint for the break face to spend.
void storeFlowEarned(Data &data, int seconds);

// Hand back the banked stint and clear it, so a break is only earned once.
int takeFlowEarned(Data &data);

void clearFlowEarned(Data &data);

}  // namespace RtcState
