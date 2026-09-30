#pragma once

namespace SimHost {

// Installed by the SDL layer so that blocking delay() calls inside the
// firmware (the deep-sleep path, mainly) still pump the window's event queue
// and repaint, instead of freezing the UI.
extern void (*delayHook)(unsigned long ms);

// How many times faster than the wall clock millis() runs, from
// SIM_TIME_SCALE; 1 unless set. Scripted keys and screenshots are timed on
// millis(), so a scaled run reaches them sooner without moving them.
double timeScale();

// How long a stretch of simulated time takes on the wall clock.
unsigned long wallMs(unsigned long simMs);

}  // namespace SimHost
