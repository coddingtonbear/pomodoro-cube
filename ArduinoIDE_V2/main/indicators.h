#pragma once
#include <stdint.h>

// Pure display logic, kept free of LVGL so it can be unit tested on the host.
// display.cpp does the drawing; these decide what should be drawn.
namespace Indicators {

// How much of the selected timer is left, 0-100. Clamped, and safe when
// selSeconds is zero.
int remainingPercent(int remSeconds, int selSeconds);

// Which colours the arc runs between. The colour at the end of an interval is
// what it is asking you to do next: green for a break, red for work.
enum class Ramp {
  // Work: red through amber to green.
  ToGreen,
  // A break: green through amber to red.
  ToRed,
  // Flow's laps after the first: green straight to cyan.
  ToCyan,
};

// The ramp for an interval. `seconds` is only read counting up, where it is the
// elapsed time and says which lap the stint is on: the first runs to green, like
// any work, and every one after it runs on from that green to cyan.
Ramp rampFor(bool work, bool countingUp, int seconds);

// Arc colour as 0xRRGGBB for how much of the interval is left, interpolated
// between the stops in ui_colors.h: the first colour at 100%, amber at
// ARC_MID_PERCENT (ToCyan passes straight through), the last at and below
// ARC_LOW_PERCENT. `flow` picks the darker stops drawn on the white panel. A
// lap passes `100 - lapPercent`, since a fresh lap has all of itself to run.
uint32_t rampColor(Ramp ramp, int remainingPercent, bool flow);

// Counting up has no total to measure against, so the arc becomes a lap
// indicator: 0-100 filling over FLOW_LAP_SECONDS, then starting again.
int lapPercent(int elapsedSeconds);

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
// rampColor() takes it.
//
// Bright is the arrangement the cube has always had: a black or white field
// with the ramp drawn on it as a thin arc. Dim swaps the two -- the ramp
// becomes the whole field, and everything drawn over it takes what used to be
// the background. A field of colour is readable across a desk at a fifth of the
// backlight, where a thin arc is not, and since the background no longer tells
// the modes apart, the arc does: black over the countdown's brighter ramp,
// white over flow's darker one.
Palette palette(Ramp ramp, int rampAt, bool flow, bool dim);

// The two halves a finished timer alternates between: black face with coloured
// digits, then a coloured face with black ones. Green when work has finished
// and red when a break has -- the colour its ramp was arriving at, so the alarm
// says what to do next as well as that it is time. The arc is not in it -- an
// alarm has nothing left to measure, and a ring still on screen is the one
// thing that could read as a timer still running. Whole-face rather than a
// detail, because what a finished timer has to do is be noticed from wherever
// you have wandered off to.
Palette alertPalette(bool inverted, bool work);

// How the countdown label should be split. Past an hour there are not enough
// digits for MM:SS at the size the panel needs, so it becomes hours and
// minutes -- which `hours` says, since the two would otherwise read alike.
struct ClockFields {
  int left;
  int right;
  bool hours;
};
ClockFields clockFields(int seconds);

// The countdown as the label shows it: 25:00, or past an hour 1h25. The h
// stands where the colon would, so an hour and a quarter can't pass for a
// minute and a quarter.
struct ClockText {
  char text[8];
};
ClockText clockText(int seconds);

// Whether the low-battery warning should be on screen, given the smoothed
// pack voltage in volts.
bool showLowBattery(float batteryVoltage);

}  // namespace Indicators
