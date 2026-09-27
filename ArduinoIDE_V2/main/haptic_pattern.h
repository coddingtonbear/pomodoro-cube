#pragma once

// Which pulses the vibration motor plays and when, kept free of the pin so it
// can be unit tested on the host. haptic.cpp does the driving; this decides
// what should be driven.
namespace Haptic {

enum class Pattern {
  None,
  // The cube has come up out of deep sleep onto a working face.
  Wake,
  // The cube has been set on a different face.
  FaceChange,
  // A timer has run out. The only pattern that repeats: it goes on until
  // something else is played or the motor is stopped.
  Alarm,
};

// Whether the motor is running `elapsedMs` into a pattern.
bool motorOn(Pattern pattern, unsigned long elapsedMs);

// True once a pattern has nothing left to play. Never true of Alarm.
bool isFinished(Pattern pattern, unsigned long elapsedMs);

// Plays one pattern at a time against a clock it is handed, which is millis()
// on the device and a supplied one in the tests.
class Player {
 public:
  // Start a pattern from its beginning, replacing whatever was playing. The
  // exception is a repeating pattern that is already the one playing, which is
  // left to carry on: the alarm is asked for on every pass of loop() for as
  // long as the timer sits at zero, and restarting it each time would hold the
  // motor on for good.
  void play(Pattern pattern, unsigned long nowMs);

  void stop();

  // Advance to `nowMs` and say whether the motor should be running.
  bool update(unsigned long nowMs);

  // What is playing, or None once a pattern has played out.
  Pattern playing() const;

  // True while the motor is shaking the accelerometer: running as of the last
  // update(), or stopped too recently to have spun down.
  bool disturbing(unsigned long nowMs) const;

 private:
  Pattern pattern_ = Pattern::None;
  unsigned long startedMs_ = 0;
  bool hasRun_ = false;
  unsigned long lastOnMs_ = 0;
};

}  // namespace Haptic
