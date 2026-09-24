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

// The same ramp for flow's white panel, a step darker at every stop: the
// countdown palette was picked against black, and amber on white is all but
// invisible. Takes how much is *left*, like arcColor -- a lap passes
// `100 - lapPercent`, since a fresh lap has all of itself still to run.
uint32_t flowArcColor(int remainingPercent);

// Every colour on the panel for one moment. Gathered rather than set from
// scattered branches, so the bright and dim schemes can be read side by side --
// and so the dim one can be tested without LVGL.
struct Palette {
  uint32_t background;
  uint32_t text;
  // The filled part of the ring, and the groove it runs in.
  uint32_t arc;
  uint32_t track;
  // The low-battery outline. Its own field because bright mode wants it red and
  // shouting, while on a dim red field red is invisible.
  uint32_t battery;
};

// The panel's colours for one moment. `rampAt` is how much is left, as
// arcColor() takes it.
//
// Bright is the arrangement the cube has always had: a black or white field
// with the ramp drawn on it as a thin arc. Dim swaps the two -- the ramp
// becomes the whole field, and everything drawn over it takes what used to be
// the background. A field of colour is readable across a desk at a fifth of the
// backlight, where a thin arc is not, and since the background no longer tells
// the modes apart, the arc does: black over the countdown's brighter ramp,
// white over flow's darker one.
Palette palette(int rampAt, bool flow, bool dim);

// The two halves a finished timer alternates between: black face with red
// digits, then red face with black ones. The arc is not in it -- an alarm has
// nothing left to measure, and a ring still on screen is the one thing that
// could read as a timer still running. Whole-face rather than a detail, because
// what a finished timer has to do is be noticed from wherever you have wandered
// off to. The same on every face: at this point which interval it was no longer
// matters.
Palette alertPalette(bool inverted);

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
