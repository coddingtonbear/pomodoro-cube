#pragma once
#include "consts.h"

namespace Display {

void setup();
void updateBattery(float voltage);

// Drive the backlight at 0-100 percent. PWM, so the sleep paths take the pin
// back as a plain output before they park it: gpio_hold_en() freezes whatever
// instant it catches, which on a PWM signal is a coin toss.
void setBacklight(int percent);

void deepSleep();

// Leave the last frame on screen through deep sleep: the GC9A01 refreshes
// itself from its own memory, so the panel keeps showing it with the CPU off.
void holdPausedFrame();

// Recolour the countdown to read as paused, and push it to the panel.
void showPaused();

// A line or two of white on black in place of the face -- black on white when
// `inverted` -- for switching on and off, when there is no timer to show.
// Brings the panel up if it is not already, and pushes the frame out at once,
// since nothing is running loop() while one of these is up. Showing what is
// already up does nothing.
void showMessage(const char *text, bool inverted = false);

// Back to the face. Does nothing if no message is up.
void hideMessage();

// Draw the face turned this many degrees clockwise from the default
// orientation. The nearest quarter turn is the panel's to do, for nothing; only
// what is left over is drawn at an angle by LVGL, so a face at a quarter turn
// costs what it always did.
void setAngle(float degrees);

// Draw the face square on a timer face, whatever angle it was at.
void rotateScreen(Orientation ori);

// Everything the face is drawn from, gathered rather than passed as a row of
// unlabelled flags.
struct TimerView {
  // Remaining when counting down, elapsed when counting up.
  int seconds;
  // The interval being counted down; 0 counting up, where there isn't one.
  int selSeconds;
  bool countingUp;
  // One of flow's faces, either of them: the panel inverts for both.
  bool flow;
  // Break time banked. Shown above the counter on the flow work face, where it
  // includes what the running stint has earned so far.
  int bankSeconds;
};

// Repaints the whole face.
void updateTimer(const TimerView &view);
void cycleTimerFinish();
}