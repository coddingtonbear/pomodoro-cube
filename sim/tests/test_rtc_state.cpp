// Tests for the magic-word guard on RTC memory. The point of the guard is that
// a cold boot leaves arbitrary bits behind, so these exercise junk as well as
// well-formed blocks.
#include "rtc_state.h"

#include <cstring>
#include <random>

#include "check.h"

namespace {

// Stands in for RTC memory after a power cycle: not zeroed, just whatever was
// there. Zeroes would let a broken guard pass by accident.
RtcState::Data junkBlock(uint32_t seed) {
  RtcState::Data data;
  std::mt19937 rng(seed);
  auto *bytes = reinterpret_cast<unsigned char *>(&data);
  for (size_t i = 0; i < sizeof(RtcState::Data); i++) {
    bytes[i] = (unsigned char)(rng() & 0xFF);
  }
  return data;
}

}  // namespace

void testColdBootIsRejected() {
  // A handful of seeds, because one unlucky block proves nothing either way.
  for (uint32_t seed = 1; seed <= 64; seed++) {
    RtcState::Data data = junkBlock(seed);
    if (data.magic == RtcState::MAGIC) continue;  // astronomically unlikely
    CHECK_MSG(!RtcState::isInitialised(data), "junk must not look initialised");
  }

  // All-zero memory is the other plausible cold-boot state.
  RtcState::Data zeroed;
  std::memset(&zeroed, 0, sizeof(zeroed));
  CHECK(!RtcState::isInitialised(zeroed));
}

void testInitialiseStampsAndClears() {
  RtcState::Data data = junkBlock(99);
  RtcState::initialise(data);

  CHECK(RtcState::isInitialised(data));
  CHECK(data.pomodoroCount == 0);
  CHECK(!data.pauseValid);
  CHECK(data.pausedRemaining == 0);
  CHECK(data.pausedSelected == 0);
  CHECK(!data.pausedCountingUp);
  CHECK(data.pausedFace == Orientation::UNDEFINED);
  CHECK(data.flowEarnedSeconds == 0);
}

void testInitialiseIsDeterministic() {
  // Two blocks initialised from different junk must come out identical, padding
  // included -- otherwise a future checksum over the block would be unstable.
  RtcState::Data a = junkBlock(11);
  RtcState::Data b = junkBlock(22);
  RtcState::initialise(a);
  RtcState::initialise(b);

  CHECK(std::memcmp(&a, &b, sizeof(RtcState::Data)) == 0);
}

void testSurvivingBlockIsKept() {
  // What deep sleep should look like: the block comes back untouched.
  RtcState::Data data;
  RtcState::initialise(data);
  data.pomodoroCount = 7;
  data.pauseValid = true;
  data.pausedFace = Orientation::DEG_90;
  data.pausedRemaining = 143;
  data.pausedSelected = 300;
  data.flowEarnedSeconds = 742;

  CHECK(RtcState::isInitialised(data));
  CHECK(data.flowEarnedSeconds == 742);
  CHECK(data.pomodoroCount == 7);
  CHECK(data.pausedRemaining == 143);
}

void testLayoutChangeInvalidates() {
  // A firmware whose Data layout changed bumps MAGIC, so the old block is
  // discarded rather than misread as the new shape.
  RtcState::Data data;
  RtcState::initialise(data);
  data.magic = RtcState::MAGIC - 1;  // stamped by a previous firmware version

  CHECK(!RtcState::isInitialised(data));
}

void testPauseRoundTrip() {
  RtcState::Data data;
  RtcState::initialise(data);
  CHECK(!RtcState::hasPause(data));

  RtcState::storePause(data, Orientation::DEG_90, 143, 300, false);
  CHECK(RtcState::hasPause(data));

  int remaining = 0;
  int selected = 0;
  bool countingUp = true;
  CHECK(RtcState::takePause(data, Orientation::DEG_90, remaining, selected, countingUp));
  CHECK(remaining == 143);
  CHECK(selected == 300);
  CHECK(!countingUp);

  // Taking it consumes it, so setting the cube down twice does not resume twice.
  CHECK(!RtcState::hasPause(data));
  CHECK(!RtcState::takePause(data, Orientation::DEG_90, remaining, selected, countingUp));
}

void testAPausedFlowStintResumesCountingUp() {
  RtcState::Data data;
  RtcState::initialise(data);

  // A count-up stint has no selected length, which must not be read as a timer
  // that never started.
  RtcState::storePause(data, Orientation::DEG_180, 742, 0, true);
  CHECK(RtcState::hasPause(data));

  int remaining = 0;
  int selected = 99;
  bool countingUp = false;
  CHECK(RtcState::takePause(data, Orientation::DEG_180, remaining, selected, countingUp));
  CHECK(remaining == 742);
  CHECK(selected == 0);
  CHECK(countingUp);
}

void testFlowEarnedRoundTrip() {
  RtcState::Data data;
  RtcState::initialise(data);
  CHECK(RtcState::takeFlowEarned(data) == 0);

  RtcState::storeFlowEarned(data, 3000);
  CHECK(RtcState::takeFlowEarned(data) == 3000);

  // Spent once: a second break is not earned by standing the cube down twice.
  CHECK(RtcState::takeFlowEarned(data) == 0);
}

void testFlowEarnedIsSeparateFromThePause() {
  // The two are consumed by different faces, so clearing one must leave the
  // other: a flow stint parked face-up is both a pause and a banked stint.
  RtcState::Data data;
  RtcState::initialise(data);
  RtcState::storePause(data, Orientation::DEG_180, 742, 0, true);
  RtcState::storeFlowEarned(data, 742);

  RtcState::clearPause(data);
  CHECK(!RtcState::hasPause(data));
  CHECK(data.flowEarnedSeconds == 742);

  RtcState::clearFlowEarned(data);
  CHECK(data.flowEarnedSeconds == 0);
}

void testPauseOnlyResumesOnItsOwnFace() {
  RtcState::Data data;
  RtcState::initialise(data);
  RtcState::storePause(data, Orientation::DEG_90, 143, 300, false);

  int remaining = 99;
  int selected = 99;
  bool countingUp = false;
  CHECK(!RtcState::takePause(data, Orientation::DEG_180, remaining, selected, countingUp));

  // Untouched: the caller falls back to that face's own timer length.
  CHECK(remaining == 99);
  CHECK(selected == 99);

  // And the pause is gone -- picking a different face abandons it rather than
  // leaving it to resurface later.
  CHECK(!RtcState::hasPause(data));
}

void testNothingWorthResumingIsNotStored() {
  RtcState::Data data;
  RtcState::initialise(data);

  // A finished timer has nothing left to resume.
  RtcState::storePause(data, Orientation::DEG_90, 0, 300, false);
  CHECK(!RtcState::hasPause(data));

  // Nor does one that never started.
  RtcState::storePause(data, Orientation::DEG_90, 0, 0, false);
  CHECK(!RtcState::hasPause(data));

  // Nor a flow stint that has not counted a second yet.
  RtcState::storePause(data, Orientation::DEG_180, 0, 0, true);
  CHECK(!RtcState::hasPause(data));
}

void testClearPauseWipesTheFace() {
  RtcState::Data data;
  RtcState::initialise(data);
  RtcState::storePause(data, Orientation::DEG_270, 50, 600, false);
  RtcState::clearPause(data);

  CHECK(!RtcState::hasPause(data));
  CHECK(data.pausedFace == Orientation::UNDEFINED);
  CHECK(data.pausedRemaining == 0);
  CHECK(!data.pausedCountingUp);
}
