// The order Util::deepSleep() talks to the peripherals in. The path is a
// straight line of side effects, so the order *is* the behaviour: what it
// guards is that the farewell advertisement is on the air for the whole of the
// shutdown work, from before the panel is put away until just before the CPU
// stops, rather than being sent and cut off ahead of it.
#include <algorithm>
#include <string>
#include <vector>

#include "Arduino.h"
#include "check.h"
#include "sim_deep_sleep.h"
#include "sim_host.h"
#include "stubs.h"
#include "util.h"

namespace {

// Skips the second's pause on the sleep path. The order is what is under test,
// not the wall clock.
void noDelay(unsigned long ms) { (void)ms; }

// Runs the deep-sleep path to the point the CPU would stop, and returns the
// calls it made, with the shim's stand-in for stopping the CPU appended so
// the tests can say "before the CPU stops" in the same terms.
std::vector<std::string> sleepCalls(Util::SleepMode mode, bool playSound) {
  Stubs::clear();
  SimHost::delayHook = noDelay;
  bool slept = false;
  try {
    Util::deepSleep(mode, playSound);
  } catch (const SimDeepSleep &) {
    slept = true;
  }
  SimHost::delayHook = nullptr;
  CHECK(slept);
  std::vector<std::string> calls = Stubs::calls;
  calls.push_back("esp_deep_sleep_start");
  return calls;
}

long indexOf(const std::vector<std::string> &calls, const char *what) {
  const auto it = std::find(calls.begin(), calls.end(), what);
  return it == calls.end() ? -1 : (long)(it - calls.begin());
}

void checkFarewellSpansTheShutdown(const std::vector<std::string> &calls) {
  const long farewell = indexOf(calls, "BLE::farewell");
  const long shutdown = indexOf(calls, "BLE::shutdown");
  const long stop = indexOf(calls, "esp_deep_sleep_start");
  CHECK(farewell >= 0);
  CHECK(shutdown >= 0);

  // Said first: Home Assistant holds the last state it heard, and everything
  // else on this path is invisible to it.
  CHECK(farewell == 0);

  // The radio is the last thing to go, immediately before the CPU stops, so
  // the whole of the panel, pause and beeper work counts as airtime for the
  // one advertisement that cannot be repeated.
  CHECK(shutdown == stop - 1);

  // Which puts every other device call between the two.
  for (const char *device : {"Beeper::playShutdown", "QMI::setupWakeup"}) {
    const long at = indexOf(calls, device);
    if (at < 0) continue;
    CHECK_MSG(farewell < at && at < shutdown, device);
  }
}

void testFarewellSpansASleepWithTheScreenOff() {
  const auto calls = sleepCalls(Util::SleepMode::Off, true);
  checkFarewellSpansTheShutdown(calls);
  const long panel = indexOf(calls, "Display::deepSleep");
  CHECK(panel > 0);
  CHECK(panel < indexOf(calls, "BLE::shutdown"));
  CHECK(indexOf(calls, "Beeper::playShutdown") >= 0);
}

void testFarewellSpansASleepHoldingThePausedFrame() {
  const auto calls = sleepCalls(Util::SleepMode::Paused, false);
  checkFarewellSpansTheShutdown(calls);
  const long panel = indexOf(calls, "Display::holdPausedFrame");
  CHECK(panel > 0);
  CHECK(panel < indexOf(calls, "BLE::shutdown"));
  // No sound asked for, so none played -- but the radio still outlasts the
  // rest of the path.
  CHECK(indexOf(calls, "Beeper::playShutdown") < 0);
}

}  // namespace

void testSleepSequence() {
  testFarewellSpansASleepWithTheScreenOff();
  testFarewellSpansASleepHoldingThePausedFrame();
}
