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
constexpr uint32_t MAGIC = 0x504F4D34;  // "POM4"

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

  // Break time flow work has earned and the break face has not yet spent, in
  // break seconds rather than worked ones -- the fifth is taken on the way in.
  // It survives working on other faces, because it is a balance rather than a
  // handoff, and the break face keeps it in step as it counts down, so an
  // unspent break is still in here whatever interrupts it.
  int32_t flowBankSeconds;

  // True when the panel was left refreshing a frame of its own rather than put
  // into sleep-in. Only the sleep paths write it, and only a wake that decides
  // without booting the display reads it: that wake has no other way of knowing
  // whether there is anything on the glass.
  bool panelHoldingFrame;
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

// Credit break seconds a flow stint earned. Clamped at FLOW_MAX_SECONDS, for
// the same reason a stint is.
void addFlowBank(Data &data, int seconds);

// The balance. Reading it does not spend it: the break face counts it down and
// writes back what is left, so several spells of work accumulate and an
// abandoned break is not lost.
int flowBank(const Data &data);

// Set the balance outright, which is how a running break keeps it in step.
void setFlowBank(Data &data, int seconds);

void clearFlowBank(Data &data);

}  // namespace RtcState
