#pragma once
#include "consts.h"

namespace Display {

void setup();
void updateBattery(float voltage);
void deepSleep();

// Leave the last frame on screen through deep sleep: the GC9A01 refreshes
// itself from its own memory, so the panel keeps showing it with the CPU off.
void holdPausedFrame();

// Recolour the countdown to read as paused, and push it to the panel.
void showPaused();
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