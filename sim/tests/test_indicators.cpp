// Tests for the arc's fill and colour, and the low-battery threshold.
#include "indicators.h"

#include <cstdlib>
#include <cstring>

#include "check.h"
#include "consts.h"

namespace {

int red(uint32_t c) { return (int)((c >> 16) & 0xFF); }
int green(uint32_t c) { return (int)((c >> 8) & 0xFF); }
int blue(uint32_t c) { return (int)(c & 0xFF); }

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

using Indicators::Ramp;

void testArcColorStops() {
  // Work runs red to green: green is the break it is heading for.
  CHECK(Indicators::rampColor(Ramp::ToGreen, 100, false) == ARC_COLOR_RED);
  CHECK(Indicators::rampColor(Ramp::ToGreen, ARC_MID_PERCENT, false) == ARC_COLOR_AMBER);
  CHECK(Indicators::rampColor(Ramp::ToGreen, ARC_LOW_PERCENT, false) == ARC_COLOR_GREEN);
  CHECK(Indicators::rampColor(Ramp::ToGreen, 0, false) == ARC_COLOR_GREEN);

  // A break runs green to red: red is the work waiting at the end of it.
  CHECK(Indicators::rampColor(Ramp::ToRed, 100, false) == ARC_COLOR_GREEN);
  CHECK(Indicators::rampColor(Ramp::ToRed, ARC_MID_PERCENT, false) == ARC_COLOR_AMBER);
  CHECK(Indicators::rampColor(Ramp::ToRed, ARC_LOW_PERCENT, false) == ARC_COLOR_RED);

  // Below the last stop it holds rather than continuing to shift.
  CHECK(Indicators::rampColor(Ramp::ToRed, 10, false) == ARC_COLOR_RED);
  CHECK(Indicators::rampColor(Ramp::ToRed, 0, false) == ARC_COLOR_RED);

  // Flow's later laps run green to cyan, arriving where the others do.
  CHECK(Indicators::rampColor(Ramp::ToCyan, 100, true) == FLOW_ARC_COLOR_GREEN);
  CHECK(Indicators::rampColor(Ramp::ToCyan, ARC_LOW_PERCENT, true) == FLOW_ARC_COLOR_CYAN);
  CHECK(Indicators::rampColor(Ramp::ToCyan, 0, true) == FLOW_ARC_COLOR_CYAN);
}

constexpr Ramp kRamps[] = {Ramp::ToGreen, Ramp::ToRed, Ramp::ToCyan};
constexpr bool kPanels[] = {false, true};

void testArcColorIsGradual() {
  // Between stops the colour interpolates, so neighbouring percentages differ
  // by a little rather than jumping at a threshold.
  CHECK(Indicators::rampColor(Ramp::ToRed, 75, false) != ARC_COLOR_GREEN);
  CHECK(Indicators::rampColor(Ramp::ToRed, 75, false) != ARC_COLOR_AMBER);
  CHECK(Indicators::rampColor(Ramp::ToRed, 37, false) != ARC_COLOR_AMBER);
  CHECK(Indicators::rampColor(Ramp::ToRed, 37, false) != ARC_COLOR_RED);

  // No single percent moves a channel far enough to read as a jump between two
  // flat colours, on any ramp or either panel. Ten levels, because amber to
  // green takes red from 0xFF to 0x35 over the last quarter: a touch over 8.
  for (const Ramp ramp : kRamps) {
    for (const bool flow : kPanels) {
      for (int percent = 100; percent > ARC_LOW_PERCENT; percent--) {
        const uint32_t here = Indicators::rampColor(ramp, percent, flow);
        const uint32_t next = Indicators::rampColor(ramp, percent - 1, flow);
        CHECK_MSG(abs(red(next) - red(here)) <= 10, "red must change gradually");
        CHECK_MSG(abs(green(next) - green(here)) <= 10, "green must change gradually");
        CHECK_MSG(abs(blue(next) - blue(here)) <= 10, "blue must change gradually");
      }
    }
  }

  // Red only ever climbs as a break runs down, and only ever falls as work does.
  for (int percent = 100; percent > ARC_LOW_PERCENT; percent--) {
    CHECK(red(Indicators::rampColor(Ramp::ToRed, percent - 1, false)) >=
          red(Indicators::rampColor(Ramp::ToRed, percent, false)));
    CHECK(red(Indicators::rampColor(Ramp::ToGreen, percent - 1, false)) <=
          red(Indicators::rampColor(Ramp::ToGreen, percent, false)));
  }

  // Green to cyan goes straight across, with no amber on the way: blue only
  // ever climbs.
  for (int percent = 100; percent > ARC_LOW_PERCENT; percent--) {
    CHECK(blue(Indicators::rampColor(Ramp::ToCyan, percent - 1, true)) >=
          blue(Indicators::rampColor(Ramp::ToCyan, percent, true)));
  }
}

// Which ramp each interval runs. Work and breaks are fixed; a flow stint's
// first lap is work like any other, and every lap after it runs on to cyan.
void testRampFor() {
  CHECK(Indicators::rampFor(true, false, TIMER_WORK_SECONDS) == Ramp::ToGreen);
  CHECK(Indicators::rampFor(true, false, 0) == Ramp::ToGreen);
  CHECK(Indicators::rampFor(false, false, TIMER_SHORT_BREAK_SECONDS) == Ramp::ToRed);

  CHECK(Indicators::rampFor(true, true, 0) == Ramp::ToGreen);
  CHECK(Indicators::rampFor(true, true, FLOW_LAP_SECONDS - 1) == Ramp::ToGreen);
  CHECK(Indicators::rampFor(true, true, FLOW_LAP_SECONDS) == Ramp::ToCyan);
  CHECK(Indicators::rampFor(true, true, 3 * FLOW_LAP_SECONDS + 1) == Ramp::ToCyan);
}

// The first lap ends on the green the second starts on, so that handover is
// seamless. Every lap after that ends on cyan and the next snaps back to green:
// the snap is a pomodoro scored.
void testFlowLapsHandOver() {
  const auto lapColour = [](int elapsed) {
    return Indicators::rampColor(Indicators::rampFor(true, true, elapsed),
                                 100 - Indicators::lapPercent(elapsed), true);
  };
  CHECK(lapColour(0) == FLOW_ARC_COLOR_RED);
  CHECK(lapColour(FLOW_LAP_SECONDS - 1) == FLOW_ARC_COLOR_GREEN);
  CHECK(lapColour(FLOW_LAP_SECONDS) == FLOW_ARC_COLOR_GREEN);
  CHECK(lapColour(2 * FLOW_LAP_SECONDS - 1) == FLOW_ARC_COLOR_CYAN);
  CHECK(lapColour(2 * FLOW_LAP_SECONDS) == FLOW_ARC_COLOR_GREEN);
  CHECK(lapColour(3 * FLOW_LAP_SECONDS - 1) == FLOW_ARC_COLOR_CYAN);
}

void testLowBatteryThreshold() {
  CHECK(Indicators::showLowBattery(LOW_BATTERY_VOLTAGE - 0.01f));
  CHECK(Indicators::showLowBattery(3.0f));
  CHECK(!Indicators::showLowBattery(LOW_BATTERY_VOLTAGE));
  CHECK(!Indicators::showLowBattery(4.2f));
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
  // Flow's stops are the countdown's a step darker, because they are drawn on
  // white -- the same ramps, just not the same colours.
  for (const Ramp ramp : kRamps) {
    CHECK(Indicators::rampColor(ramp, 100, true) != Indicators::rampColor(ramp, 100, false));
    CHECK(Indicators::rampColor(ramp, 0, true) != Indicators::rampColor(ramp, 0, false));
  }
  CHECK(Indicators::rampColor(Ramp::ToRed, 100, true) == FLOW_ARC_COLOR_GREEN);
  CHECK(Indicators::rampColor(Ramp::ToRed, ARC_MID_PERCENT, true) == FLOW_ARC_COLOR_AMBER);
  CHECK(Indicators::rampColor(Ramp::ToRed, 0, true) == FLOW_ARC_COLOR_RED);
  CHECK(Indicators::rampColor(Ramp::ToGreen, 100, true) == FLOW_ARC_COLOR_RED);
  CHECK(Indicators::rampColor(Ramp::ToGreen, 0, true) == FLOW_ARC_COLOR_GREEN);
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

void testClockTextMarksHoursWithAnH() {
  CHECK(std::strcmp(Indicators::clockText(0).text, "00:00") == 0);
  CHECK(std::strcmp(Indicators::clockText(TIMER_WORK_SECONDS).text, "25:00") == 0);
  CHECK(std::strcmp(Indicators::clockText(3599).text, "59:59") == 0);

  // Past an hour the h stands where the colon was, so 1h05 can't be read as
  // a minute and five seconds.
  CHECK(std::strcmp(Indicators::clockText(3600).text, "1h00") == 0);
  CHECK(std::strcmp(Indicators::clockText(3600 + 5 * 60 + 59).text, "1h05") == 0);
  CHECK(std::strcmp(Indicators::clockText(FLOW_MAX_SECONDS).text, "4h00") == 0);
}

// Bright keeps the arrangement the cube has always had. Dim swaps the ramp onto
// the background and draws everything over it in what used to be the
// background, which is also what tells the two modes apart once the field no
// longer does.
void testBrightPaletteIsUnchanged() {
  const Indicators::Palette normal = Indicators::palette(Ramp::ToRed, 100, false, false);
  CHECK(normal.background == SCREEN_BG_COLOR);
  CHECK(normal.text == COUNTDOWN_COLOR);
  CHECK(normal.arc == Indicators::rampColor(Ramp::ToRed, 100, false));
  CHECK(normal.track == ARC_TRACK_COLOR);
  CHECK(normal.battery == LOW_BATTERY_COLOR);

  const Indicators::Palette flow = Indicators::palette(Ramp::ToRed, 100, true, false);
  CHECK(flow.background == FLOW_BG_COLOR);
  CHECK(flow.text == COUNTDOWN_COLOR_FLOW);
  CHECK(flow.arc == Indicators::rampColor(Ramp::ToRed, 100, true));
  CHECK(flow.track == FLOW_ARC_TRACK_COLOR);
}

void testDimSwapsTheRampOntoTheBackground() {
  const Indicators::Palette p = Indicators::palette(Ramp::ToRed, 100, false, true);
  CHECK(p.background == Indicators::rampColor(Ramp::ToRed, 100, false));
  // Black, which is what the background was.
  CHECK(p.arc == SCREEN_BG_COLOR);
  CHECK(p.text == SCREEN_BG_COLOR);
  // Hidden: the groove would be a second tone on a field already carrying the
  // reading.
  CHECK(p.track == p.background);
}

// Flow keeps its darker ramp, so the one colour drawn over it is white.
void testDimFlowDrawsInWhite() {
  const Indicators::Palette p = Indicators::palette(Ramp::ToRed, 100, true, true);
  CHECK(p.background == Indicators::rampColor(Ramp::ToRed, 100, true));
  CHECK(p.arc == FLOW_BG_COLOR);
  CHECK(p.text == FLOW_BG_COLOR);
  CHECK(p.track == p.background);

  // The two dim schemes must never draw in the same colour, because that is the
  // only thing left distinguishing them.
  CHECK(Indicators::palette(Ramp::ToRed, 100, false, true).arc != p.arc);
}

// Red on a red field is nothing at all, and the end of a countdown is exactly
// when the warning matters.
void testDimBatteryWarningLeavesTheRampColour() {
  const Indicators::Palette low = Indicators::palette(Ramp::ToRed, 0, false, true);
  CHECK(low.background == ARC_COLOR_RED);
  CHECK(low.battery != ARC_COLOR_RED);
  CHECK(low.battery == SCREEN_BG_COLOR);

  CHECK(Indicators::palette(Ramp::ToRed, 0, true, true).battery == FLOW_BG_COLOR);
}

// The background follows the ramp the whole way down, so the field shifts
// green to red as a break runs out, and red to green as work does.
void testDimBackgroundFollowsTheRamp() {
  CHECK(Indicators::palette(Ramp::ToRed, 100, false, true).background == ARC_COLOR_GREEN);
  CHECK(Indicators::palette(Ramp::ToRed, ARC_MID_PERCENT, false, true).background == ARC_COLOR_AMBER);
  CHECK(Indicators::palette(Ramp::ToRed, ARC_LOW_PERCENT, false, true).background == ARC_COLOR_RED);
  CHECK(Indicators::palette(Ramp::ToRed, 0, false, true).background == ARC_COLOR_RED);

  CHECK(Indicators::palette(Ramp::ToGreen, 100, false, true).background == ARC_COLOR_RED);
  CHECK(Indicators::palette(Ramp::ToGreen, 0, false, true).background == ARC_COLOR_GREEN);
  CHECK(Indicators::palette(Ramp::ToCyan, 0, true, true).background == FLOW_ARC_COLOR_CYAN);
}

void testPalette() {
  testBrightPaletteIsUnchanged();
  testDimSwapsTheRampOntoTheBackground();
  testDimFlowDrawsInWhite();
  testDimBatteryWarningLeavesTheRampColour();
  testDimBackgroundFollowsTheRamp();
}

// The alarm is the whole face, not a detail on it: black with coloured digits,
// then coloured with black ones. Green when work is done -- go and take the
// break -- and red when a break is.
void testAlertFlashInvertsTheWholeFace() {
  const Indicators::Palette dark = Indicators::alertPalette(false, false);
  CHECK(dark.background == SCREEN_BG_COLOR);
  CHECK(dark.text == ARC_COLOR_RED);

  const Indicators::Palette lit = Indicators::alertPalette(true, false);
  CHECK(lit.background == ARC_COLOR_RED);
  CHECK(lit.text == SCREEN_BG_COLOR);

  // The two halves are each other's inverse, which is what makes the flash a
  // flash rather than two unrelated frames.
  CHECK(dark.background == lit.text);
  CHECK(dark.text == lit.background);
}

// Each alarm is the colour its interval's ramp was arriving at.
void testAlertIsTheColourTheRampEndedOn() {
  CHECK(Indicators::alertPalette(true, true).background ==
        Indicators::rampColor(Indicators::rampFor(true, false, 0), 0, false));
  CHECK(Indicators::alertPalette(true, false).background ==
        Indicators::rampColor(Indicators::rampFor(false, false, 0), 0, false));
  CHECK(Indicators::alertPalette(false, true).text == ARC_COLOR_GREEN);
}

constexpr bool kFlashHalves[] = {false, true};
constexpr bool kKinds[] = {false, true};

// The ring is hidden while alerting, but a groove left in the old track colour
// would still draw a circle on the face.
void testAlertLeavesNoRingBehind() {
  for (const bool work : kKinds) {
    for (const bool inverted : kFlashHalves) {
      const Indicators::Palette p = Indicators::alertPalette(inverted, work);
      CHECK(p.track == p.background);
      CHECK(p.arc == p.background);
    }
  }
}

// A colour on itself is nothing at all, and a flat pack is worth knowing about
// while the alarm has your attention.
void testAlertBatteryWarningStaysVisible() {
  CHECK(Indicators::alertPalette(true, false).battery == SCREEN_BG_COLOR);
  CHECK(Indicators::alertPalette(false, false).battery == ARC_COLOR_RED);
  for (const bool work : kKinds) {
    for (const bool inverted : kFlashHalves) {
      const Indicators::Palette p = Indicators::alertPalette(inverted, work);
      CHECK(p.battery != p.background);
    }
  }
}

void testAlertPalette() {
  testAlertFlashInvertsTheWholeFace();
  testAlertIsTheColourTheRampEndedOn();
  testAlertLeavesNoRingBehind();
  testAlertBatteryWarningStaysVisible();
}
