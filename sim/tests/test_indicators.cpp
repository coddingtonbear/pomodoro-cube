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

void testLapPercent() {
  // Counting up has no total, so the arc fills over a lap and starts again.
  CHECK(Indicators::lapPercent(0) == 0);
  CHECK(Indicators::lapPercent(-5) == 0);
  CHECK(Indicators::lapPercent(FLOW_LAP_SECONDS / 2) == 50);
  CHECK(Indicators::lapPercent(FLOW_LAP_SECONDS - 1) == 99);

  // A lap boundary starts over rather than saturating at full.
  CHECK(Indicators::lapPercent(FLOW_LAP_SECONDS) == 0);
  CHECK(Indicators::lapPercent(FLOW_LAP_SECONDS + FLOW_LAP_SECONDS / 4) == 25);
  CHECK(Indicators::lapPercent(5 * FLOW_LAP_SECONDS) == 0);
}

void testFlowArcColorMatchesTheCountdownRamp() {
  // Same direction as arcColor -- how much is left -- so both flow faces read
  // the same way round. A lap inverts at the call site instead.
  CHECK(Indicators::flowArcColor(100) == FLOW_ARC_COLOR_FULL);
  CHECK(Indicators::flowArcColor(0) == FLOW_ARC_COLOR_LOW);
  CHECK(Indicators::flowArcColor(ARC_MID_PERCENT) == FLOW_ARC_COLOR_MID);

  // A fresh lap has all of itself left, so it is the green end.
  CHECK(Indicators::flowArcColor(100 - 0) == FLOW_ARC_COLOR_FULL);
  CHECK(Indicators::flowArcColor(100 - 100) == FLOW_ARC_COLOR_LOW);

  // And darker than the countdown's at every stop, because it is drawn on white.
  CHECK(Indicators::flowArcColor(100) != Indicators::arcColor(100));
  CHECK(Indicators::flowArcColor(0) != Indicators::arcColor(0));
}

void testClockFieldsSwitchToHoursPastAnHour() {
  // Minutes and seconds while there is room for them.
  CHECK(Indicators::clockFields(0).left == 0 && Indicators::clockFields(0).right == 0);
  CHECK(!Indicators::clockFields(0).hours);
  CHECK(Indicators::clockFields(-5).left == 0 && Indicators::clockFields(-5).right == 0);

  const Indicators::ClockFields work = Indicators::clockFields(TIMER_WORK_SECONDS);
  CHECK(work.left == 25 && work.right == 0 && !work.hours);

  const Indicators::ClockFields nearly = Indicators::clockFields(3599);
  CHECK(nearly.left == 59 && nearly.right == 59 && !nearly.hours);

  // At an hour MM:SS would need three digits for the minutes, which does not
  // fit, so the fields become hours and minutes.
  const Indicators::ClockFields hour = Indicators::clockFields(3600);
  CHECK(hour.left == 1 && hour.right == 0 && hour.hours);

  const Indicators::ClockFields long_ = Indicators::clockFields(3 * 3600 + 25 * 60 + 40);
  CHECK(long_.left == 3 && long_.right == 25 && long_.hours);

  // The longest a stint can run still fits two digits on each side.
  const Indicators::ClockFields cap = Indicators::clockFields(FLOW_MAX_SECONDS);
  CHECK(cap.left == 4 && cap.right == 0 && cap.hours);
}
