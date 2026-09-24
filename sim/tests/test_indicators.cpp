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

// Bright keeps the arrangement the cube has always had. Dim swaps the ramp onto
// the background and draws everything over it in what used to be the
// background, which is also what tells the two modes apart once the field no
// longer does.
void testBrightPaletteIsUnchanged() {
  const Indicators::Palette normal = Indicators::palette(100, false, false);
  CHECK(normal.background == SCREEN_BG_COLOR);
  CHECK(normal.text == COUNTDOWN_COLOR);
  CHECK(normal.arc == Indicators::arcColor(100));
  CHECK(normal.track == ARC_TRACK_COLOR);
  CHECK(normal.battery == LOW_BATTERY_COLOR);

  const Indicators::Palette flow = Indicators::palette(100, true, false);
  CHECK(flow.background == FLOW_BG_COLOR);
  CHECK(flow.text == COUNTDOWN_COLOR_FLOW);
  CHECK(flow.arc == Indicators::flowArcColor(100));
  CHECK(flow.track == FLOW_ARC_TRACK_COLOR);
}

void testDimSwapsTheRampOntoTheBackground() {
  const Indicators::Palette p = Indicators::palette(100, false, true);
  CHECK(p.background == Indicators::arcColor(100));
  // Black, which is what the background was.
  CHECK(p.arc == SCREEN_BG_COLOR);
  CHECK(p.text == SCREEN_BG_COLOR);
  // Hidden: the groove would be a second tone on a field already carrying the
  // reading.
  CHECK(p.track == p.background);
}

// Flow keeps its darker ramp, so the one colour drawn over it is white.
void testDimFlowDrawsInWhite() {
  const Indicators::Palette p = Indicators::palette(100, true, true);
  CHECK(p.background == Indicators::flowArcColor(100));
  CHECK(p.arc == FLOW_BG_COLOR);
  CHECK(p.text == FLOW_BG_COLOR);
  CHECK(p.track == p.background);

  // The two dim schemes must never draw in the same colour, because that is the
  // only thing left distinguishing them.
  CHECK(Indicators::palette(100, false, true).arc != p.arc);
}

// Red on a red field is nothing at all, and the end of a countdown is exactly
// when the warning matters.
void testDimBatteryWarningLeavesTheRampColour() {
  const Indicators::Palette low = Indicators::palette(0, false, true);
  CHECK(low.background == ARC_COLOR_LOW);
  CHECK(low.battery != ARC_COLOR_LOW);
  CHECK(low.battery == SCREEN_BG_COLOR);

  CHECK(Indicators::palette(0, true, true).battery == FLOW_BG_COLOR);
}

// The background follows the ramp the whole way down, so the field shifts green
// to red as the timer runs out.
void testDimBackgroundFollowsTheRamp() {
  CHECK(Indicators::palette(100, false, true).background == ARC_COLOR_FULL);
  CHECK(Indicators::palette(ARC_MID_PERCENT, false, true).background == ARC_COLOR_MID);
  CHECK(Indicators::palette(ARC_LOW_PERCENT, false, true).background == ARC_COLOR_LOW);
  CHECK(Indicators::palette(0, false, true).background == ARC_COLOR_LOW);
}

void testPalette() {
  testBrightPaletteIsUnchanged();
  testDimSwapsTheRampOntoTheBackground();
  testDimFlowDrawsInWhite();
  testDimBatteryWarningLeavesTheRampColour();
  testDimBackgroundFollowsTheRamp();
}

// The alarm is the whole face, not a detail on it: black with red digits, then
// red with black ones.
void testAlertFlashInvertsTheWholeFace() {
  const Indicators::Palette dark = Indicators::alertPalette(false);
  CHECK(dark.background == SCREEN_BG_COLOR);
  CHECK(dark.text == ARC_COLOR_LOW);

  const Indicators::Palette lit = Indicators::alertPalette(true);
  CHECK(lit.background == ARC_COLOR_LOW);
  CHECK(lit.text == SCREEN_BG_COLOR);

  // The two halves are each other's inverse, which is what makes the flash a
  // flash rather than two unrelated frames.
  CHECK(dark.background == lit.text);
  CHECK(dark.text == lit.background);
}

constexpr bool kFlashHalves[] = {false, true};

// The ring is hidden while alerting, but a groove left in the old track colour
// would still draw a circle on the face.
void testAlertLeavesNoRingBehind() {
  for (const bool inverted : kFlashHalves) {
    const Indicators::Palette p = Indicators::alertPalette(inverted);
    CHECK(p.track == p.background);
    CHECK(p.arc == p.background);
  }
}

// Red on red is nothing at all, and a flat pack is worth knowing about while
// the alarm has your attention.
void testAlertBatteryWarningStaysVisible() {
  CHECK(Indicators::alertPalette(true).battery == SCREEN_BG_COLOR);
  CHECK(Indicators::alertPalette(false).battery == ARC_COLOR_LOW);
  for (const bool inverted : kFlashHalves) {
    const Indicators::Palette p = Indicators::alertPalette(inverted);
    CHECK(p.battery != p.background);
  }
}

void testAlertPalette() {
  testAlertFlashInvertsTheWholeFace();
  testAlertLeavesNoRingBehind();
  testAlertBatteryWarningStaysVisible();
}
