// What the vibration motor plays and when. The patterns are read back against
// the constants rather than against figures repeated here, so retuning one in
// consts.h does not mean retuning the tests.
#include <initializer_list>

#include "check.h"
#include "consts.h"
#include "haptic_pattern.h"

using Haptic::Pattern;
using Haptic::Player;

namespace {

void testNothingPlaysNothing() {
  CHECK(!Haptic::motorOn(Pattern::None, 0));
  CHECK(Haptic::isFinished(Pattern::None, 0));

  Player player;
  CHECK(!player.update(0));
  CHECK(player.playing() == Pattern::None);
  // Never having run is not the same as having stopped at millisecond zero.
  CHECK(!player.disturbing(0));
}

void testAFaceChangeIsOnePulse() {
  const unsigned long pulse = HAPTIC_FACE_CHANGE_MS[0];
  CHECK(Haptic::motorOn(Pattern::FaceChange, 0));
  CHECK(Haptic::motorOn(Pattern::FaceChange, pulse - 1));
  CHECK(!Haptic::motorOn(Pattern::FaceChange, pulse));
  CHECK(!Haptic::isFinished(Pattern::FaceChange, pulse - 1));
  CHECK(Haptic::isFinished(Pattern::FaceChange, pulse));
}

void testWakingIsTwoPulses() {
  const unsigned long first = HAPTIC_WAKE_MS[0];
  const unsigned long gap = HAPTIC_WAKE_MS[1];
  const unsigned long second = HAPTIC_WAKE_MS[2];
  CHECK(Haptic::motorOn(Pattern::Wake, 0));
  CHECK(!Haptic::motorOn(Pattern::Wake, first));
  CHECK(!Haptic::motorOn(Pattern::Wake, first + gap - 1));
  CHECK(Haptic::motorOn(Pattern::Wake, first + gap));
  CHECK(Haptic::motorOn(Pattern::Wake, first + gap + second - 1));
  CHECK(!Haptic::motorOn(Pattern::Wake, first + gap + second));
  CHECK(Haptic::isFinished(Pattern::Wake, first + gap + second));
}

void testTheAlarmRepeats() {
  const unsigned long on = HAPTIC_ALARM_MS[0];
  const unsigned long period = on + HAPTIC_ALARM_MS[1];
  for (unsigned long lap : {0UL, 1UL, 29UL}) {
    const unsigned long start = lap * period;
    CHECK(Haptic::motorOn(Pattern::Alarm, start));
    CHECK(Haptic::motorOn(Pattern::Alarm, start + on - 1));
    CHECK(!Haptic::motorOn(Pattern::Alarm, start + on));
    CHECK(!Haptic::motorOn(Pattern::Alarm, start + period - 1));
  }
  CHECK(!Haptic::isFinished(Pattern::Alarm, 1000UL * 60 * 60));
}

void testAPatternPlaysFromWhenItWasStarted() {
  Player player;
  const unsigned long start = 5000;
  const unsigned long pulse = HAPTIC_FACE_CHANGE_MS[0];
  player.play(Pattern::FaceChange, start);
  CHECK(player.update(start));
  CHECK(player.playing() == Pattern::FaceChange);
  CHECK(player.update(start + pulse - 1));
  CHECK(!player.update(start + pulse));
  // Played out, so there is nothing playing -- which is what lets a blocking
  // caller know when to stop waiting.
  CHECK(player.playing() == Pattern::None);
  CHECK(!player.update(start + pulse + 10000));
}

// loop() asks for the alarm on every pass for as long as the timer sits at
// zero. Were each of those a restart, the pattern would never get past its
// first millisecond and the motor would run without a break.
void testAskingForTheAlarmAgainDoesNotRestartIt() {
  Player player;
  const unsigned long on = HAPTIC_ALARM_MS[0];
  player.play(Pattern::Alarm, 0);
  for (unsigned long now = 0; now < on; now += 20) {
    player.play(Pattern::Alarm, now);
    CHECK(player.update(now));
  }
  player.play(Pattern::Alarm, on);
  CHECK(!player.update(on));
}

// A one-shot pattern does restart, though: two face changes in quick succession
// are two decisions, and each gets its buzz in full.
void testAOneShotPatternRestarts() {
  Player player;
  const unsigned long pulse = HAPTIC_FACE_CHANGE_MS[0];
  player.play(Pattern::FaceChange, 0);
  player.play(Pattern::FaceChange, pulse - 10);
  CHECK(player.update(pulse + 10));
  CHECK(!player.update(2 * pulse - 10));
}

// Turning the cube to another face is what silences a finished timer.
void testAFaceChangeReplacesTheAlarm() {
  Player player;
  const unsigned long pulse = HAPTIC_FACE_CHANGE_MS[0];
  player.play(Pattern::Alarm, 0);
  CHECK(player.update(0));

  const unsigned long turned = 3210;
  player.play(Pattern::FaceChange, turned);
  CHECK(player.playing() == Pattern::FaceChange);
  CHECK(player.update(turned));
  CHECK(!player.update(turned + pulse));
  CHECK(player.playing() == Pattern::None);
  // And it stays silent through what would have been the alarm's next pulse.
  const unsigned long period = HAPTIC_ALARM_MS[0] + HAPTIC_ALARM_MS[1];
  CHECK(!player.update(4 * period));
}

void testStoppingSilencesTheMotor() {
  Player player;
  player.play(Pattern::Alarm, 0);
  CHECK(player.update(0));
  player.stop();
  CHECK(player.playing() == Pattern::None);
  CHECK(!player.update(1));
}

// The accelerometer is not to be believed about taps while the motor runs, nor
// for a moment after: the motor takes time to spin down.
void testTheMotorDisturbsUntilItHasSpunDown() {
  Player player;
  const unsigned long pulse = HAPTIC_FACE_CHANGE_MS[0];
  player.play(Pattern::FaceChange, 1000);
  player.update(1000);
  CHECK(player.disturbing(1000));

  const unsigned long lastOn = 1000 + pulse - 1;
  player.update(lastOn);
  player.update(1000 + pulse);
  CHECK(player.disturbing(lastOn + HAPTIC_SETTLE_MS - 1));
  CHECK(!player.disturbing(lastOn + HAPTIC_SETTLE_MS));
}

// Every silence in the alarm has to leave room, once the motor has spun down,
// for a face change to sit out the whole debounce. consts.h asserts this at
// compile time; this is the same claim made of the pattern as played.
void testTheAlarmLeavesRoomToDebounceAFaceChange() {
  const unsigned long on = HAPTIC_ALARM_MS[0];
  const unsigned long period = on + HAPTIC_ALARM_MS[1];
  const unsigned long quietFrom = on + HAPTIC_SETTLE_MS;
  CHECK(period - quietFrom > (unsigned long)ORI_DEBOUNCE_DELAY);
  for (unsigned long at = quietFrom; at < period; at += 10) {
    CHECK(!Haptic::motorOn(Pattern::Alarm, at));
  }
}

}  // namespace

void testHaptic() {
  testNothingPlaysNothing();
  testAFaceChangeIsOnePulse();
  testWakingIsTwoPulses();
  testTheAlarmRepeats();
  testAPatternPlaysFromWhenItWasStarted();
  testAskingForTheAlarmAgainDoesNotRestartIt();
  testAOneShotPatternRestarts();
  testAFaceChangeReplacesTheAlarm();
  testStoppingSilencesTheMotor();
  testTheMotorDisturbsUntilItHasSpunDown();
  testTheAlarmLeavesRoomToDebounceAFaceChange();
}
