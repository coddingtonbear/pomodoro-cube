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

// Repaints the whole face from the timer. `seconds` is the remaining time when
// counting down and the elapsed time when counting up; `selSeconds` is the
// interval being counted down, and is ignored when counting up because there
// isn't one.
void updateTimer(int seconds, int selSeconds, bool countingUp);
void cycleTimerFinish();
}