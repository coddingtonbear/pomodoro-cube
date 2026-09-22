// Tests for the arc's fill and colour, and the low-battery threshold.
#include "indicators.h"

#include <cstdlib>

#include "check.h"
#include "consts.h"

namespace {

int red(uint32_t c) { return (int)((c >> 16) & 0xFF); }
int green(uint32_t c) { return (int)((c >> 8) & 0xFF); }

}  // namespace

void testRemainingPercent() {
  // Full at the start, empty at the end: the arc drains rather than fills.
  CHECK(Indicators::remainingPercent(25 * 60, 25 * 60) == 100);
  CHECK(Indicators::remainingPercent(0, 25 * 60) == 0);
  CHECK(Indicators::remainingPercent(12 * 60 + 30, 25 * 60) == 50);
  CHECK(Indicators::remainingPercent(5 * 60, 50 * 60) == 10);

  // A 50-minute timer is the longest, and must not overflow on the way.
  CHECK(Indicators::remainingPercent(50 * 60 - 1, 50 * 60) == 99);

  // Defensive: a zero-length timer must not divide by zero.
  CHECK(Indicators::remainingPercent(60, 0) == 0);
  CHECK(Indicators::remainingPercent(-5, 25 * 60) == 0);
  CHECK(Indicators::remainingPercent(99999, 25 * 60) == 100);
}

void testArcColorStops() {
  CHECK(Indicators::arcColor(100) == ARC_COLOR_FULL);
  CHECK(Indicators::arcColor(ARC_MID_PERCENT) == ARC_COLOR_MID);
  CHECK(Indicators::arcColor(ARC_LOW_PERCENT) == ARC_COLOR_LOW);

  // Below the last stop it stays red rather than continuing to darken.
  CHECK(Indicators::arcColor(10) == ARC_COLOR_LOW);
  CHECK(Indicators::arcColor(0) == ARC_COLOR_LOW);
}

void testArcColorIsGradual() {
  // Between stops the colour interpolates, so neighbouring percentages differ
  // by a little rather than jumping at a threshold.
  CHECK(Indicators::arcColor(75) != ARC_COLOR_FULL);
  CHECK(Indicators::arcColor(75) != ARC_COLOR_MID);
  CHECK(Indicators::arcColor(37) != ARC_COLOR_MID);
  CHECK(Indicators::arcColor(37) != ARC_COLOR_LOW);

  // Red only ever climbs as the timer runs down, and no single percent moves
  // a channel far enough to read as a jump between two flat colours.
  for (int percent = 100; percent > ARC_LOW_PERCENT; percent--) {
    const uint32_t here = Indicators::arcColor(percent);
    const uint32_t next = Indicators::arcColor(percent - 1);
    CHECK_MSG(red(next) >= red(here), "red must not fall as time runs out");
    CHECK_MSG(abs(red(next) - red(here)) <= 8, "red must change gradually");
    CHECK_MSG(abs(green(next) - green(here)) <= 8, "green must change gradually");
  }

  // Overall the arc loses green as it drains, even though amber carries
  // slightly more green than the full-timer colour does.
  CHECK(green(Indicators::arcColor(ARC_LOW_PERCENT)) < green(Indicators::arcColor(100)));
}

void testLowBatteryThreshold() {
  CHECK(Indicators::showLowBattery(LOW_BATTERY_VOLTAGE - 0.01f));
  CHECK(Indicators::showLowBattery(3.0f));
  CHECK(!Indicators::showLowBattery(LOW_BATTERY_VOLTAGE));
  CHECK(!Indicators::showLowBattery(4.2f));

  // The warning has to arrive before the firmware gives up and deep-sleeps,
  // or it would never be seen.
  CHECK(LOW_BATTERY_VOLTAGE > BAT_EMPTY_VOLTAGE);
}
