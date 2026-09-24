#pragma once
#include <stdint.h>

// Pure display logic, kept free of LVGL so it can be unit tested on the host.
// display.cpp does the drawing; these decide what should be drawn.
namespace Indicators {

// How much of the selected timer is left, 0-100. Clamped, and safe when
// selSeconds is zero.
int remainingPercent(int remSeconds, int selSeconds);

// Arc colour as 0xRRGGBB, interpolated between the stops in consts.h: full at
// 100%, mid at ARC_MID_PERCENT, low at and below ARC_LOW_PERCENT.
uint32_t arcColor(int remainingPercent);

// Counting up has no total to measure against, so the arc becomes a lap
// indicator: 0-100 filling over FLOW_LAP_SECONDS, then starting again.
int lapPercent(int elapsedSeconds);

// Colour for a lap at that percentage. Runs the ramp backwards -- a fresh lap is
// green and an old one red, which is the nudge to take a break -- and in flow's
// darker stops, since it is drawn on white.
uint32_t flowArcColor(int lapPercent);

// How the countdown label should be split. Past an hour there are not enough
// digits for MM:SS at the size the panel needs, so it becomes HH:MM -- and the
// two are indistinguishable on screen, which is what `hours` is for.
struct ClockFields {
  int left;
  int right;
  bool hours;
};
ClockFields clockFields(int seconds);

// Whether the low-battery warning should be on screen, given the smoothed
// pack voltage in volts.
bool showLowBattery(float batteryVoltage);

}  // namespace Indicators
